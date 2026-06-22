#include "render3d.h"

// glad must come before any other OpenGL header.
#include <glad/glad.h>

#if defined(_MSC_VER) && defined(USE_VCPKG)
#   include <SDL2/SDL.h>
#else
#   include <SDL.h>
#endif

#include <cstdio>

namespace
{
bool s_loaded = false;       // glad loaded successfully
bool s_init_attempted = false;
bool s_enabled = true;       // user toggle; default on for the Phase 1 spike
} // namespace

namespace cdda3d
{

bool init()
{
    if( s_init_attempted ) {
        return s_loaded;
    }
    s_init_attempted = true;

    // SDL_Renderer (opengl backend) keeps a GL context current; load our own GL
    // function pointers through SDL's resolver.
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
    return true;
}

void shutdown()
{
    s_loaded = false;
    s_init_attempted = false;
}

bool active()
{
    return s_enabled && s_loaded;
}

void toggle()
{
    s_enabled = !s_enabled;
}

void render_map_viewport( int x, int y, int w, int h, int win_w, int win_h )
{
    if( !active() || w <= 0 || h <= 0 ) {
        return;
    }
    ( void )win_w;

    // CDDA/SDL use a top-left origin; OpenGL's window space is bottom-left.
    const int gl_y = win_h - ( y + h );

    // Restore the bits of GL state SDL_Renderer may have left set, so our clear
    // behaves predictably, then scope our draw to the map viewport rectangle.
    glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
    glViewport( x, gl_y, w, h );
    glEnable( GL_SCISSOR_TEST );
    glScissor( x, gl_y, w, h );

    // Phase 1 proof-of-life: paint the map viewport a solid colour with OpenGL.
    // If this shows up while the sidebar/menus still render, the 3D-under-2D
    // compositing path works.
    glClearColor( 0.55f, 0.10f, 0.65f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    glDisable( GL_SCISSOR_TEST );
}

} // namespace cdda3d
