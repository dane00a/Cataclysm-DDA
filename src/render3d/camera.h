#pragma once
#ifndef CATA_SRC_RENDER3D_CAMERA_H
#define CATA_SRC_RENDER3D_CAMERA_H

#include "gl_math.h"

namespace cdda3d
{

// Orthographic camera that orbits a center point. Pitch is the elevation above
// the horizon (clamped 35..75 per the project's locked decision); yaw is the
// azimuth (we drive it in 90-degree snaps); zoom is the half-height of the view
// in tiles.
class Camera
{
    public:
        float yaw_deg = 45.0f;
        float pitch_deg = 55.0f;
        float zoom_tiles = 12.0f;

        void rotate_yaw( float delta_deg );
        void adjust_pitch( float delta_deg );
        // factor < 1 zooms in (fewer tiles visible), > 1 zooms out.
        void adjust_zoom( float factor );

        // Combined projection * view matrix for looking at `center` (world space)
        // with the given viewport aspect ratio (width / height).
        mat4 view_proj( const vec3 &center, float aspect ) const;
};

} // namespace cdda3d

#endif // CATA_SRC_RENDER3D_CAMERA_H
