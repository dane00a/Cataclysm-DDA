#include "render3d.h"

// glad must come before any other OpenGL header.
#include <glad/glad.h>

#if defined(_MSC_VER) && defined(USE_VCPKG)
#   include <SDL2/SDL.h>
#else
#   include <SDL.h>
#endif

#include <cstdio>
#include <unordered_map>
#include <vector>

#include "camera.h"
#include "gl_util.h"
#include "scene.h"

namespace
{
bool s_loaded = false;          // glad loaded successfully
bool s_init_attempted = false;
bool s_enabled = true;          // user toggle; default on for now

cdda3d::Camera s_camera;
cdda3d::Mesh s_ground;
std::vector<cdda3d::CellSprite> s_cells;
std::vector<float> s_vertex_buf;
std::unordered_map<SDL_Texture *, std::vector<const cdda3d::CellSprite *>> s_by_atlas;
GLuint s_program = 0;
GLint s_mvp_loc = -1;
GLint s_tex_loc = -1;

const char *const VERT_SRC =
    "#version 330 core\n"
    "layout(location=0) in vec3 in_pos;\n"
    "layout(location=1) in vec2 in_uv;\n"
    "layout(location=2) in vec3 in_tint;\n"
    "uniform mat4 u_mvp;\n"
    "out vec2 v_uv;\n"
    "out vec3 v_tint;\n"
    "void main() { v_uv = in_uv; v_tint = in_tint; gl_Position = u_mvp * vec4( in_pos, 1.0 ); }\n";

const char *const FRAG_SRC =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "in vec3 v_tint;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 o_frag;\n"
    "void main() {\n"
    "    vec4 t = texture( u_tex, v_uv );\n"
    "    if( t.a < 0.5 ) { discard; }\n"
    "    o_frag = vec4( t.rgb * v_tint, 1.0 );\n"
    "}\n";
} // namespace

namespace cdda3d
{

bool init()
{
    if( s_init_attempted ) {
        return s_loaded;
    }
    s_init_attempted = true;

    if( gladLoadGLLoader( reinterpret_cast<GLADloadproc>( SDL_GL_GetProcAddress ) ) == 0 ) {
        std::fprintf( stderr,
                      "[cdda3d] gladLoadGLLoader failed - renderer is probably not OpenGL-backed; "
                      "3D layer disabled.\n" );
        s_loaded = false;
        return false;
    }
    s_loaded = true;

    const GLubyte *ver = glGetString( GL_VERSION );
    const GLubyte *rend = glGetString( GL_RENDERER );
    std::fprintf( stderr, "[cdda3d] OpenGL loaded: version='%s' renderer='%s'\n",
                  ver ? reinterpret_cast<const char *>( ver ) : "?",
                  rend ? reinterpret_cast<const char *>( rend ) : "?" );

    s_program = make_program( VERT_SRC, FRAG_SRC );
    if( s_program != 0 ) {
        s_mvp_loc = glGetUniformLocation( s_program, "u_mvp" );
        s_tex_loc = glGetUniformLocation( s_program, "u_tex" );
    } else {
        std::fprintf( stderr, "[cdda3d] failed to build scene shader; 3D layer disabled.\n" );
        s_loaded = false;
    }
    return s_loaded;
}

void shutdown()
{
    s_ground.destroy();
    if( s_program != 0 ) {
        glDeleteProgram( s_program );
        s_program = 0;
    }
    s_loaded = false;
    s_init_attempted = false;
}

bool active()
{
    return s_enabled && s_loaded && s_program != 0;
}

void toggle()
{
    s_enabled = !s_enabled;
}

void rotate_yaw( float delta_deg )
{
    s_camera.rotate_yaw( delta_deg );
}

void adjust_pitch( float delta_deg )
{
    s_camera.adjust_pitch( delta_deg );
}

void adjust_zoom( float factor )
{
    s_camera.adjust_zoom( factor );
}

void render_map_viewport( int x, int y, int w, int h, int win_w, int win_h )
{
    if( !active() || w <= 0 || h <= 0 ) {
        return;
    }

    // CDDA/SDL use a top-left origin; OpenGL's window space is bottom-left.
    const int gl_y = win_h - ( y + h );

    build_ground_sprites( s_cells );

    // Group cells by their atlas page so we bind each texture once.
    s_by_atlas.clear();
    for( const CellSprite &c : s_cells ) {
        s_by_atlas[static_cast<SDL_Texture *>( c.atlas )].push_back( &c );
    }

    // --- our GL pass, scoped to the map viewport ---
    glViewport( x, gl_y, w, h );
    glEnable( GL_SCISSOR_TEST );
    glScissor( x, gl_y, w, h );
    glDisable( GL_DEPTH_TEST );   // flat coplanar ground; default FBO may lack depth
    glDisable( GL_BLEND );        // sprite edges handled by alpha-discard in shader
    glDisable( GL_CULL_FACE );
    glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

    glClearColor( 0.07f, 0.07f, 0.09f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    const float aspect = static_cast<float>( w ) / static_cast<float>( h );
    const mat4 mvp = s_camera.view_proj( vec3{ 0.0f, 0.0f, 0.0f }, aspect );

    glUseProgram( s_program );
    glUniformMatrix4fv( s_mvp_loc, 1, GL_FALSE, mvp.data() );
    glActiveTexture( GL_TEXTURE0 );
    glUniform1i( s_tex_loc, 0 );

    for( const auto &kv : s_by_atlas ) {
        SDL_Texture *atlas = kv.first;
        if( atlas == nullptr ) {
            continue;
        }
        int atlas_w = 0;
        int atlas_h = 0;
        if( SDL_QueryTexture( atlas, nullptr, nullptr, &atlas_w, &atlas_h ) != 0 ||
            atlas_w <= 0 || atlas_h <= 0 ) {
            continue;
        }
        float texw = 1.0f;
        float texh = 1.0f;
        if( SDL_GL_BindTexture( atlas, &texw, &texh ) != 0 ) {
            continue;
        }

        s_vertex_buf.clear();
        s_vertex_buf.reserve( kv.second.size() * 6 * 8 );
        for( const CellSprite *cp : kv.second ) {
            const CellSprite &c = *cp;
            const float u0 = ( static_cast<float>( c.sx ) / atlas_w ) * texw;
            const float u1 = ( static_cast<float>( c.sx + c.sw ) / atlas_w ) * texw;
            const float v0 = ( static_cast<float>( c.sy ) / atlas_h ) * texh;
            const float v1 = ( static_cast<float>( c.sy + c.sh ) / atlas_h ) * texh;
            const float x0 = c.wx - 0.5f, x1 = c.wx + 0.5f;
            const float z0 = c.wz - 0.5f, z1 = c.wz + 0.5f;
            const float t = c.bright;
            const auto push = [&]( float vx, float vz, float u, float vv ) {
                s_vertex_buf.push_back( vx );
                s_vertex_buf.push_back( 0.0f );
                s_vertex_buf.push_back( vz );
                s_vertex_buf.push_back( u );
                s_vertex_buf.push_back( vv );
                s_vertex_buf.push_back( t );
                s_vertex_buf.push_back( t );
                s_vertex_buf.push_back( t );
            };
            // sprite top (v0) -> north edge (z0)
            push( x0, z0, u0, v0 );
            push( x1, z0, u1, v0 );
            push( x1, z1, u1, v1 );
            push( x0, z0, u0, v0 );
            push( x1, z1, u1, v1 );
            push( x0, z1, u0, v1 );
        }
        s_ground.upload( s_vertex_buf );
        s_ground.draw();

        SDL_GL_UnbindTexture( atlas );
    }

    // --- restore enough state that SDL_Renderer keeps working ---
    glUseProgram( 0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glDisable( GL_SCISSOR_TEST );
    glViewport( 0, 0, win_w, win_h );
}

} // namespace cdda3d
