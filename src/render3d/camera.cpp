#include "camera.h"

#include <algorithm>
#include <cmath>

namespace cdda3d
{

static float deg2rad( float d )
{
    return d * 3.14159265358979f / 180.0f;
}

void Camera::rotate_yaw( float delta_deg )
{
    yaw_deg += delta_deg;
}

void Camera::adjust_pitch( float delta_deg )
{
    pitch_deg = std::clamp( pitch_deg + delta_deg, 35.0f, 75.0f );
}

void Camera::adjust_zoom( float factor )
{
    zoom_tiles = std::clamp( zoom_tiles * factor, 4.0f, 60.0f );
}

mat4 Camera::view_proj( const vec3 &center, float aspect ) const
{
    const float yr = deg2rad( yaw_deg );
    const float pr = deg2rad( pitch_deg );

    // Direction from the center out to the camera eye: azimuth `yaw`, elevation
    // `pitch` above the horizon. Distance is arbitrary for an orthographic
    // projection (it only affects near/far clipping).
    constexpr float dist = 100.0f;
    const vec3 dir{ std::cos( pr ) * std::sin( yr ), std::sin( pr ), std::cos( pr ) * std::cos( yr ) };
    const vec3 eye = center + dir * dist;
    const mat4 view = mat4_look_at( eye, center, vec3{ 0.0f, 1.0f, 0.0f } );

    const float half_w = zoom_tiles * aspect;
    const mat4 proj = mat4_ortho( -half_w, half_w, -zoom_tiles, zoom_tiles,
                                  0.1f, dist * 2.0f + 200.0f );
    return proj * view;
}

} // namespace cdda3d
