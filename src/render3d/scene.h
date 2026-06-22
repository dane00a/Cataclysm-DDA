#pragma once
#ifndef CATA_SRC_RENDER3D_SCENE_H
#define CATA_SRC_RENDER3D_SCENE_H

#include <vector>

// Builds the per-frame 3D geometry for the cdda-3d renderer by querying live
// game state (map, avatar, visibility). Phase 2: a flat ground grid of the
// visible cells, colored by terrain + lighting.

namespace cdda3d
{

// Fill `out` with interleaved vertices (position.xyz, color.rgb) of the visible
// ground cells. Coordinates are player-relative, so the camera always looks at
// the world origin (0,0,0). Each cell is a 1x1 quad on the ground plane (y = 0).
void build_ground_mesh( std::vector<float> &out );

} // namespace cdda3d

#endif // CATA_SRC_RENDER3D_SCENE_H
