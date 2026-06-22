#pragma once
#ifndef CATA_SRC_RENDER3D_RENDER3D_H
#define CATA_SRC_RENDER3D_RENDER3D_H

// cdda-3d: experimental 3D renderer layer that draws the map viewport in 3D
// underneath CDDA's native 2D UI. This is Phase 1 (integration spike): it only
// proves we can issue our own OpenGL into the same context SDL_Renderer uses and
// composite it under the live 2D UI without breaking input/menus.
//
// All of this is compiled only when CDDA_3D is defined, and every call site in
// upstream code is guarded by `#ifdef CDDA_3D`, so the upstream merge surface
// stays tiny.

namespace cdda3d
{

// Load OpenGL (via glad against SDL's current GL context). Call once, after the
// OpenGL-backed SDL_Renderer has been created. Returns false and leaves the
// layer inactive if GL could not be loaded (e.g. renderer is not OpenGL-backed).
bool init();

// Release GL resources. Call before the renderer/window are destroyed.
void shutdown();

// Whether the 3D overlay is currently enabled AND usable (GL loaded).
bool active();

// Toggle the 3D overlay on/off at runtime.
void toggle();

// Camera controls (no-ops while the layer is inactive).
void rotate_yaw( float delta_deg );
void adjust_pitch( float delta_deg );
void adjust_zoom( float factor );

// Draw the 3D view into the map-viewport rectangle, given in window pixel
// coordinates with a top-left origin (the same space CDDA uses for windows).
// `win_w`/`win_h` are the full drawable size in pixels (for the GL Y-flip).
// SDL's own 2D batch must already be flushed (SDL_RenderFlush) before calling.
void render_map_viewport( int x, int y, int w, int h, int win_w, int win_h );

} // namespace cdda3d

#endif // CATA_SRC_RENDER3D_RENDER3D_H
