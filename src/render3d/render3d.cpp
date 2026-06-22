#include "render3d.h"

// glad must come before any other OpenGL header.
#include <glad/glad.h>

#if defined(_MSC_VER) && defined(USE_VCPKG)
#   include <SDL2/SDL.h>
#else
#   include <SDL.h>
#endif

#include <cstdio>
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
std::vector<float> s_vertex_buf;
GLuint s_program = 0;
GLint s_mvp_loc = -1;

const char *const VERT_SRC =
    "#version 330 core\n"
    "layout(location=0) in vec3 in_pos;\n"
    "layout(location=1) in vec3 in_col;\n"
    "uniform mat4 u_mvp;\n"
    "out vec3 v_col;\n"
    "void main() { v_col = in_col; gl_Position = u_mvp * vec4( in_pos, 1.0 ); }\n";

const char *const FRAG_SRC =
    "#version 330 core\n"
    "in vec3 v_col;\n"
    "out vec4 o_frag;\n"
    "void main() { o_frag = vec4( v_col, 1.0 ); }\n";
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

    // Build this frame's geometry from live game state.
    build_ground_mesh( s_vertex_buf );
    s_ground.upload( s_vertex_buf );

    // --- our GL pass, scoped to the map viewport ---
    glViewport( x, gl_y, w, h );
    glEnable( GL_SCISSOR_TEST );
    glScissor( x, gl_y, w, h );
    glDisable( GL_DEPTH_TEST );   // flat coplanar grid; default FBO may lack depth
    glDisable( GL_BLEND );
    glDisable( GL_CULL_FACE );
    glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

    glClearColor( 0.08f, 0.08f, 0.10f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    const float aspect = static_cast<float>( w ) / static_cast<float>( h );
    const mat4 mvp = s_camera.view_proj( vec3{ 0.0f, 0.0f, 0.0f }, aspect );

    glUseProgram( s_program );
    glUniformMatrix4fv( s_mvp_loc, 1, GL_FALSE, mvp.data() );
    s_ground.draw();

    // --- restore enough state that SDL_Renderer keeps working ---
    glUseProgram( 0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glDisable( GL_SCISSOR_TEST );
    glViewport( 0, 0, win_w, win_h );
}

} // namespace cdda3d
