#pragma once
#ifndef CATA_SRC_RENDER3D_GL_UTIL_H
#define CATA_SRC_RENDER3D_GL_UTIL_H

#include <glad/glad.h>
#include <vector>

// Small OpenGL helpers for the cdda-3d renderer: shader programs and a dynamic
// vertex mesh. Kept intentionally tiny.

namespace cdda3d
{

// Compile + link a program from GLSL source. Returns 0 on failure (logs why).
GLuint make_program( const char *vs_src, const char *fs_src );

// A dynamic mesh of interleaved vertices: position(vec3) + color(vec3).
class Mesh
{
    public:
        Mesh() = default;
        ~Mesh();
        Mesh( const Mesh & ) = delete;
        Mesh &operator=( const Mesh & ) = delete;

        // (Re)upload the whole vertex buffer. `verts` is 6 floats per vertex.
        void upload( const std::vector<float> &verts );
        // Draw as GL_TRIANGLES. No-op if empty.
        void draw() const;
        // Free GL objects (must be called with the GL context current).
        void destroy();

    private:
        void ensure();
        GLuint vao = 0;
        GLuint vbo = 0;
        GLsizei vertex_count = 0;
};

} // namespace cdda3d

#endif // CATA_SRC_RENDER3D_GL_UTIL_H
