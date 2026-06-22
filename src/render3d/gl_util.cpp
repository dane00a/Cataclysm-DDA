#include "gl_util.h"

#include <cstdio>

namespace cdda3d
{

static GLuint compile_shader( GLenum type, const char *src )
{
    const GLuint s = glCreateShader( type );
    glShaderSource( s, 1, &src, nullptr );
    glCompileShader( s );
    GLint ok = 0;
    glGetShaderiv( s, GL_COMPILE_STATUS, &ok );
    if( ok == GL_FALSE ) {
        char log[1024] = {};
        glGetShaderInfoLog( s, sizeof( log ), nullptr, log );
        std::fprintf( stderr, "[cdda3d] shader compile failed: %s\n", log );
        glDeleteShader( s );
        return 0;
    }
    return s;
}

GLuint make_program( const char *vs_src, const char *fs_src )
{
    const GLuint vs = compile_shader( GL_VERTEX_SHADER, vs_src );
    const GLuint fs = compile_shader( GL_FRAGMENT_SHADER, fs_src );
    if( vs == 0 || fs == 0 ) {
        if( vs ) {
            glDeleteShader( vs );
        }
        if( fs ) {
            glDeleteShader( fs );
        }
        return 0;
    }
    const GLuint prog = glCreateProgram();
    glAttachShader( prog, vs );
    glAttachShader( prog, fs );
    glLinkProgram( prog );
    glDeleteShader( vs );
    glDeleteShader( fs );
    GLint ok = 0;
    glGetProgramiv( prog, GL_LINK_STATUS, &ok );
    if( ok == GL_FALSE ) {
        char log[1024] = {};
        glGetProgramInfoLog( prog, sizeof( log ), nullptr, log );
        std::fprintf( stderr, "[cdda3d] program link failed: %s\n", log );
        glDeleteProgram( prog );
        return 0;
    }
    return prog;
}

Mesh::~Mesh()
{
    // Note: relies on destroy() being called while the GL context is alive;
    // see cdda3d::shutdown(). Destructor avoids GL calls if already destroyed.
    destroy();
}

void Mesh::ensure()
{
    if( vao != 0 ) {
        return;
    }
    glGenVertexArrays( 1, &vao );
    glGenBuffers( 1, &vbo );
    glBindVertexArray( vao );
    glBindBuffer( GL_ARRAY_BUFFER, vbo );
    constexpr GLsizei stride = 6 * sizeof( float );
    glEnableVertexAttribArray( 0 );
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>( 0 ) );
    glEnableVertexAttribArray( 1 );
    glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<void *>( 3 * sizeof( float ) ) );
    glBindVertexArray( 0 );
}

void Mesh::upload( const std::vector<float> &verts )
{
    ensure();
    glBindVertexArray( vao );
    glBindBuffer( GL_ARRAY_BUFFER, vbo );
    glBufferData( GL_ARRAY_BUFFER, static_cast<GLsizeiptr>( verts.size() * sizeof( float ) ),
                  verts.data(), GL_DYNAMIC_DRAW );
    glBindVertexArray( 0 );
    vertex_count = static_cast<GLsizei>( verts.size() / 6 );
}

void Mesh::draw() const
{
    if( vao == 0 || vertex_count == 0 ) {
        return;
    }
    glBindVertexArray( vao );
    glDrawArrays( GL_TRIANGLES, 0, vertex_count );
    glBindVertexArray( 0 );
}

void Mesh::destroy()
{
    if( vbo != 0 ) {
        glDeleteBuffers( 1, &vbo );
        vbo = 0;
    }
    if( vao != 0 ) {
        glDeleteVertexArrays( 1, &vao );
        vao = 0;
    }
    vertex_count = 0;
}

} // namespace cdda3d
