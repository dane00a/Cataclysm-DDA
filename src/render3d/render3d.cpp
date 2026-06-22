#include "render3d.h"

// glad must come before any other OpenGL header.
#include <glad/glad.h>

#if defined(_MSC_VER) && defined(USE_VCPKG)
#   include <SDL2/SDL.h>
#else
#   include <SDL.h>
#endif

#include <cmath>
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
std::vector<cdda3d::DrawItem> s_items;
std::vector<float> s_vertex_buf;
std::unordered_map<SDL_Texture *, std::vector<const cdda3d::DrawItem *>> s_by_atlas;
GLuint s_program = 0;
GLint s_mvp_loc = -1;
GLint s_tex_loc = -1;

// Our own render target (so we get a depth buffer regardless of SDL's context).
GLuint s_fbo = 0;
GLuint s_fbo_color = 0;
GLuint s_fbo_depth = 0;
int s_fbo_w = 0;
int s_fbo_h = 0;

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

bool ensure_fbo( int w, int h )
{
    if( s_fbo != 0 && s_fbo_w == w && s_fbo_h == h ) {
        return true;
    }
    if( s_fbo == 0 ) {
        glGenFramebuffers( 1, &s_fbo );
    }
    if( s_fbo_color == 0 ) {
        glGenTextures( 1, &s_fbo_color );
    }
    if( s_fbo_depth == 0 ) {
        glGenRenderbuffers( 1, &s_fbo_depth );
    }

    glBindTexture( GL_TEXTURE_2D, s_fbo_color );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glBindTexture( GL_TEXTURE_2D, 0 );

    glBindRenderbuffer( GL_RENDERBUFFER, s_fbo_depth );
    glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h );
    glBindRenderbuffer( GL_RENDERBUFFER, 0 );

    glBindFramebuffer( GL_FRAMEBUFFER, s_fbo );
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_fbo_color, 0 );
    glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_fbo_depth );
    const GLenum st = glCheckFramebufferStatus( GL_FRAMEBUFFER );
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    if( st != GL_FRAMEBUFFER_COMPLETE ) {
        std::fprintf( stderr, "[cdda3d] framebuffer incomplete: 0x%x\n", st );
        return false;
    }
    s_fbo_w = w;
    s_fbo_h = h;
    return true;
}
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
    if( s_fbo != 0 ) {
        glDeleteFramebuffers( 1, &s_fbo );
        s_fbo = 0;
    }
    if( s_fbo_color != 0 ) {
        glDeleteTextures( 1, &s_fbo_color );
        s_fbo_color = 0;
    }
    if( s_fbo_depth != 0 ) {
        glDeleteRenderbuffers( 1, &s_fbo_depth );
        s_fbo_depth = 0;
    }
    s_fbo_w = s_fbo_h = 0;
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
    if( !ensure_fbo( w, h ) ) {
        return;
    }

    // CDDA/SDL use a top-left origin; OpenGL's window space is bottom-left.
    const int gl_y = win_h - ( y + h );

    build_draw_items( s_items );
    s_by_atlas.clear();
    for( const DrawItem &it : s_items ) {
        s_by_atlas[static_cast<SDL_Texture *>( it.atlas )].push_back( &it );
    }

    // --- render the 3D scene into our own FBO (with depth) ---
    glBindFramebuffer( GL_FRAMEBUFFER, s_fbo );
    glViewport( 0, 0, w, h );
    glDisable( GL_SCISSOR_TEST );
    glEnable( GL_DEPTH_TEST );
    glDepthFunc( GL_LESS );
    glDepthMask( GL_TRUE );
    glDisable( GL_BLEND );        // sprite edges handled by alpha-discard in shader
    glDisable( GL_CULL_FACE );
    glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

    glClearColor( 0.07f, 0.07f, 0.09f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    const float aspect = static_cast<float>( w ) / static_cast<float>( h );
    const mat4 mvp = s_camera.view_proj( vec3{ 0.0f, 0.0f, 0.0f }, aspect );
    glUseProgram( s_program );
    glUniformMatrix4fv( s_mvp_loc, 1, GL_FALSE, mvp.data() );
    glActiveTexture( GL_TEXTURE0 );
    glUniform1i( s_tex_loc, 0 );

    // Y-axis billboard horizontal "right" vector for the current yaw.
    const float yaw_rad = s_camera.yaw_deg * 3.14159265f / 180.0f;
    const float bb_rx = std::cos( yaw_rad );
    const float bb_rz = -std::sin( yaw_rad );

    for( const auto &kv : s_by_atlas ) {
        SDL_Texture *atlas = kv.first;
        if( atlas == nullptr ) {
            continue;
        }
        int aw = 0;
        int ah = 0;
        if( SDL_QueryTexture( atlas, nullptr, nullptr, &aw, &ah ) != 0 || aw <= 0 || ah <= 0 ) {
            continue;
        }
        float texw = 1.0f;
        float texh = 1.0f;
        if( SDL_GL_BindTexture( atlas, &texw, &texh ) != 0 ) {
            continue;
        }

        s_vertex_buf.clear();
        for( const DrawItem *ip : kv.second ) {
            const DrawItem &it = *ip;
            const float u0 = ( static_cast<float>( it.sx ) / aw ) * texw;
            const float u1 = ( static_cast<float>( it.sx + it.sw ) / aw ) * texw;
            const float v0 = ( static_cast<float>( it.sy ) / ah ) * texh;
            const float v1 = ( static_cast<float>( it.sy + it.sh ) / ah ) * texh;
            const float t = it.bright;
            const float x0 = it.wx - 0.5f, x1 = it.wx + 0.5f;
            const float z0 = it.wz - 0.5f, z1 = it.wz + 0.5f;

            const auto vert = [&]( float vx, float vy, float vz, float u, float vv ) {
                s_vertex_buf.push_back( vx );
                s_vertex_buf.push_back( vy );
                s_vertex_buf.push_back( vz );
                s_vertex_buf.push_back( u );
                s_vertex_buf.push_back( vv );
                s_vertex_buf.push_back( t );
                s_vertex_buf.push_back( t );
                s_vertex_buf.push_back( t );
            };
            const auto quad = [&]( float ax, float ay, float az, float au, float av,
                                   float bx, float by, float bz, float bu, float bv,
                                   float cx, float cy, float cz, float cu, float cv,
                                   float dx, float dy, float dz, float du, float dv ) {
                vert( ax, ay, az, au, av );
                vert( bx, by, bz, bu, bv );
                vert( cx, cy, cz, cu, cv );
                vert( ax, ay, az, au, av );
                vert( cx, cy, cz, cu, cv );
                vert( dx, dy, dz, du, dv );
            };

            if( it.kind == DrawKind::Floor ) {
                // ground quad, sprite top (v0) -> north (z0)
                quad( x0, 0.0f, z0, u0, v0, x1, 0.0f, z0, u1, v0,
                      x1, 0.0f, z1, u1, v1, x0, 0.0f, z1, u0, v1 );
            } else if( it.kind == DrawKind::Wall ) {
                const float hh = it.height;
                // top face
                quad( x0, hh, z0, u0, v0, x1, hh, z0, u1, v0,
                      x1, hh, z1, u1, v1, x0, hh, z1, u0, v1 );
                // side faces: bottom (y=0) -> v1, top (y=hh) -> v0
                quad( x0, 0.0f, z0, u0, v1, x1, 0.0f, z0, u1, v1,
                      x1, hh, z0, u1, v0, x0, hh, z0, u0, v0 );   // north
                quad( x0, 0.0f, z1, u0, v1, x1, 0.0f, z1, u1, v1,
                      x1, hh, z1, u1, v0, x0, hh, z1, u0, v0 );   // south
                quad( x0, 0.0f, z0, u0, v1, x0, 0.0f, z1, u1, v1,
                      x0, hh, z1, u1, v0, x0, hh, z0, u0, v0 );   // west
                quad( x1, 0.0f, z0, u0, v1, x1, 0.0f, z1, u1, v1,
                      x1, hh, z1, u1, v0, x1, hh, z0, u0, v0 );   // east
            } else { // Billboard: upright quad facing the camera
                const float hh = it.height;
                const float blx = it.wx - bb_rx * 0.5f, blz = it.wz - bb_rz * 0.5f;
                const float brx = it.wx + bb_rx * 0.5f, brz = it.wz + bb_rz * 0.5f;
                // top of sprite v0, bottom v1
                quad( blx, hh, blz, u0, v0, brx, hh, brz, u1, v0,
                      brx, 0.0f, brz, u1, v1, blx, 0.0f, blz, u0, v1 );
            }
        }
        s_ground.upload( s_vertex_buf );
        s_ground.draw();
        SDL_GL_UnbindTexture( atlas );
    }

    // --- blit our FBO color into the window's map rect ---
    glBindFramebuffer( GL_READ_FRAMEBUFFER, s_fbo );
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
    glBlitFramebuffer( 0, 0, w, h, x, gl_y, x + w, gl_y + h, GL_COLOR_BUFFER_BIT, GL_NEAREST );

    // --- restore enough state that SDL_Renderer keeps working ---
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    glUseProgram( 0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glDisable( GL_DEPTH_TEST );
    glDepthMask( GL_TRUE );
    glViewport( 0, 0, win_w, win_h );
}

} // namespace cdda3d
