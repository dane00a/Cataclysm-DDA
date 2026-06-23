#pragma once
#ifndef CATA_SRC_RENDER3D_SCENE_H
#define CATA_SRC_RENDER3D_SCENE_H

#include <vector>

// Builds the per-frame 3D draw list for the cdda-3d renderer by querying live
// game state (map, avatar, furniture, items, creatures, visibility) and
// resolving each to a tileset sprite. Phase 4: floors, extruded walls, and
// upright billboards for furniture/items/creatures/the player.

namespace cdda3d
{

enum class DrawKind {
    Floor,      // flat quad on the ground (y = 0)
    Wall,       // extruded box (y = 0..height)
    Billboard,  // upright, camera-facing quad
};

struct DrawItem {
    float wx = 0.0f;        // world position (player-relative), X = east
    float wz = 0.0f;        // world position (player-relative), Z = south
    DrawKind kind = DrawKind::Floor;
    void *atlas = nullptr;  // SDL_Texture* of the sprite's atlas page (opaque here)
    int sx = 0, sy = 0, sw = 0, sh = 0; // source rect within the atlas (pixels)
    float bright = 1.0f;    // brightness tint from lighting/visibility
    float height = 0.0f;    // world-unit height for Wall extrusion / Billboard
    float wy = 0.0f;        // world-unit base Y (z-level offset; player level = 0)
};

// Gather the visible cells' draw items (terrain floor/wall, furniture, items,
// creatures). Player-relative coords -> camera looks at the world origin.
void build_draw_items( std::vector<DrawItem> &out );

} // namespace cdda3d

#endif // CATA_SRC_RENDER3D_SCENE_H
