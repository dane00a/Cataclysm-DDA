#pragma once
#ifndef CATA_SRC_RENDER3D_GL_MATH_H
#define CATA_SRC_RENDER3D_GL_MATH_H

// Minimal self-contained vector/matrix math for the cdda-3d renderer, so we
// don't pull in an external math library. Matrices are column-major to match
// OpenGL (m[col * 4 + row]).

#include <array>
#include <cmath>

namespace cdda3d
{

struct vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

inline vec3 operator+( const vec3 &a, const vec3 &b )
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}
inline vec3 operator-( const vec3 &a, const vec3 &b )
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}
inline vec3 operator*( const vec3 &a, float s )
{
    return { a.x * s, a.y * s, a.z * s };
}
inline float dot( const vec3 &a, const vec3 &b )
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline vec3 cross( const vec3 &a, const vec3 &b )
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
inline vec3 normalize( const vec3 &v )
{
    const float len = std::sqrt( dot( v, v ) );
    return len > 0.0f ? vec3{ v.x / len, v.y / len, v.z / len } : v;
}

struct mat4 {
    std::array<float, 16> m{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    const float *data() const {
        return m.data();
    }
};

// a * b, column-major.
inline mat4 operator*( const mat4 &a, const mat4 &b )
{
    mat4 r;
    for( int c = 0; c < 4; ++c ) {
        for( int row = 0; row < 4; ++row ) {
            float s = 0.0f;
            for( int k = 0; k < 4; ++k ) {
                s += a.m[k * 4 + row] * b.m[c * 4 + k];
            }
            r.m[c * 4 + row] = s;
        }
    }
    return r;
}

inline mat4 mat4_ortho( float l, float r, float b, float t, float n, float f )
{
    mat4 o;
    o.m = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    o.m[0]  = 2.0f / ( r - l );
    o.m[5]  = 2.0f / ( t - b );
    o.m[10] = -2.0f / ( f - n );
    o.m[12] = -( r + l ) / ( r - l );
    o.m[13] = -( t + b ) / ( t - b );
    o.m[14] = -( f + n ) / ( f - n );
    return o;
}

inline mat4 mat4_look_at( const vec3 &eye, const vec3 &center, const vec3 &up )
{
    const vec3 f = normalize( center - eye );
    const vec3 s = normalize( cross( f, up ) );
    const vec3 u = cross( s, f );
    mat4 r;
    r.m = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot( s, eye );
    r.m[13] = -dot( u, eye );
    r.m[14] = dot( f, eye );
    return r;
}

} // namespace cdda3d

#endif // CATA_SRC_RENDER3D_GL_MATH_H
