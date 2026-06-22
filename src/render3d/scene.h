#pragma once
#ifndef CATA_SRC_RENDER3D_SCENE_H
#define CATA_SRC_RENDER3D_SCENE_H

#include <vector>

// Builds the per-frame 3D geometry for the cdda-3d renderer by querying live
// game state (map, avatar, visibility) and resolving each visible cell to its
// terrain sprite via the tileset. Phase 3: textured ground cells.

namespace cdda3d
{

// One visible ground cell with its resolved terrain sprite.
struct CellSprite {
    float wx = 0.0f;        // world position (player-relative), X = east
    float wz = 0.0f;        // world position (player-relative), Z = south
    void *atlas = nullptr;  // SDL_Texture* of the sprite's atlas page (opaque here)
    int sx = 0, sy = 0, sw = 0, sh = 0; // source rect within the atlas (pixels)
    float bright = 1.0f;    // brightness tint from lighting/visibility
};

// Gather visible ground cells with their resolved terrain sprite. Cells with no
// sprite (or never seen) are skipped. Player-relative coords -> camera looks at
// the world origin.
void build_ground_sprites( std::vector<CellSprite> &out );

} // namespace cdda3d

#endif // CATA_SRC_RENDER3D_SCENE_H
