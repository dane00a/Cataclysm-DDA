#include "scene.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

#include "avatar.h"
#include "cata_tiles.h"
#include "character.h"
#include "creature_tracker.h"
#include "game_constants.h"   // fov_3d_z_range
#include "item.h"
#include "level_cache.h"
#include "lightmap.h"
#include "map.h"
#include "map_scale_constants.h"
#include "mapdata.h"
#include "monster.h"
#include "mtype.h"
#include "sdltiles.h"   // tilecontext
#include "type_id.h"

namespace cdda3d
{

// World-unit height of one z-level. Matches the wall height so a level's walls
// reach the floor above it.
static constexpr float LEVEL_HEIGHT = 1.0f;

void build_draw_items( std::vector<DrawItem> &out )
{
    out.clear();
    if( !tilecontext ) {
        return;
    }

    map &here = get_map();
    avatar &you = get_avatar();
    creature_tracker &tracker = get_creature_tracker();
    const tripoint_bub_ms ppos = you.pos_bub( here );
    const int pz = you.posz();
    const int px = ppos.x();
    const int py = ppos.y();

    constexpr int radius = 40;
    const int xmin = std::max( 0, px - radius );
    const int xmax = std::min( MAPSIZE_X - 1, px + radius );
    const int ymin = std::max( 0, py - radius );
    const int ymax = std::min( MAPSIZE_Y - 1, py + radius );

    // Resolve an id+category to a sprite and stamp it onto a DrawItem.
    const auto resolve = [&]( const std::string & id, TILE_CATEGORY cat, lit_level ll,
    unsigned rnd, DrawItem & it ) -> bool {
        SDL_Texture *tex = nullptr;
        SDL_Rect src{ 0, 0, 0, 0 };
        if( !tilecontext->cdda3d_lookup_sprite( id, cat, ll, rnd, tex, src ) )
        {
            return false;
        }
        it.atlas = tex;
        it.sx = src.x;
        it.sy = src.y;
        it.sw = src.w;
        it.sh = src.h;
        return true;
    };

    // Iterate visible z-levels from the lowest up to the player's level; stopping
    // at pz is the roof cull (nothing above the player is drawn).
    const int zmin = std::max( pz - fov_3d_z_range, -OVERMAP_DEPTH );
    for( int z = zmin; z <= pz; ++z ) {
        const level_cache &ch = here.access_cache( z );
        const float wy = static_cast<float>( z - pz ) * LEVEL_HEIGHT;
        const float zdim = std::pow( 0.78f, static_cast<float>( pz - z ) );

        for( int y = ymin; y <= ymax; ++y ) {
            for( int x = xmin; x <= xmax; ++x ) {
                const lit_level ll = ch.visibility_cache[x][y];
                if( ll == lit_level::BLANK ) {
                    continue; // never seen / solid floor above blocks it
                }
                const tripoint_bub_ms p( x, y, z );
                const float wx = static_cast<float>( x - px );
                const float wz = static_cast<float>( y - py );
                const unsigned rnd = static_cast<unsigned>( x * 73856093 ) ^
                                     static_cast<unsigned>( y * 19349663 );

                float bright = 1.0f;
                switch( ll ) {
                    case lit_level::DARK:
                        bright = 0.35f;
                        break;
                    case lit_level::LOW:
                        bright = 0.6f;
                        break;
                    case lit_level::MEMORIZED:
                        bright = 0.7f;
                        break;
                    default:
                        break;
                }
                bright *= zdim;

                // --- terrain (always): floor or extruded wall ---
                {
                    const ter_id t = here.ter( p );
                    const bool is_wall = t.obj().has_flag( ter_furn_flag::TFLAG_WALL );
                    DrawItem it;
                    it.wx = wx;
                    it.wz = wz;
                    it.wy = wy;
                    it.bright = bright;
                    if( resolve( t.id().str(), TILE_CATEGORY::TERRAIN, ll, rnd, it ) ) {
                        it.kind = is_wall ? DrawKind::Wall : DrawKind::Floor;
                        it.height = is_wall ? LEVEL_HEIGHT : 0.0f;
                        out.push_back( it );
                    }
                }

                // --- furniture: upright billboard ---
                {
                    const furn_id f = here.furn( p );
                    if( f ) {
                        DrawItem it;
                        it.wx = wx;
                        it.wz = wz;
                        it.wy = wy;
                        it.bright = bright;
                        if( resolve( f.id().str(), TILE_CATEGORY::FURNITURE, ll, rnd, it ) ) {
                            it.kind = DrawKind::Billboard;
                            it.height = 1.0f;
                            out.push_back( it );
                        }
                    }
                }

                // --- top item (only when currently visible) ---
                if( ll != lit_level::MEMORIZED && here.sees_some_items( p, you ) ) {
                    const map_stack ms = here.i_at( p );
                    if( !ms.empty() ) {
                        const item &itm = *std::prev( ms.end() );
                        DrawItem it;
                        it.wx = wx;
                        it.wz = wz;
                        it.wy = wy;
                        it.bright = bright;
                        if( resolve( itm.typeId().str(), TILE_CATEGORY::ITEM, ll, rnd, it ) ) {
                            it.kind = DrawKind::Billboard;
                            it.height = 0.6f;
                            out.push_back( it );
                        }
                    }
                }

                // --- creature (monster / npc): upright billboard ---
                {
                    const Creature *cr = tracker.creature_at( p, true );
                    if( cr != nullptr ) {
                        std::string id;
                        TILE_CATEGORY cat = TILE_CATEGORY::NONE;
                        if( const monster *m = dynamic_cast<const monster *>( cr ) ) {
                            id = m->type->id.str();
                            cat = TILE_CATEGORY::MONSTER;
                        } else if( const Character *cha = dynamic_cast<const Character *>( cr ) ) {
                            // The avatar is drawn explicitly at the view center below
                            // (creature_at does not reliably return it).
                            if( !cha->is_avatar() ) {
                                id = cha->male ? "npc_male" : "npc_female";
                                cat = TILE_CATEGORY::NONE;
                            }
                        }
                        if( !id.empty() ) {
                            DrawItem it;
                            it.wx = wx;
                            it.wz = wz;
                            it.wy = wy;
                            it.bright = std::max( bright, 0.8f * zdim );
                            if( resolve( id, cat, ll, rnd, it ) ) {
                                it.kind = DrawKind::Billboard;
                                it.height = 1.0f;
                                out.push_back( it );
                            }
                        }
                    }
                }
            }
        }
    }

    // Always draw the avatar at the view center (it isn't reliably returned by
    // creature_at, and the player should be unmistakably present).
    {
        DrawItem it;
        it.wx = 0.0f;
        it.wz = 0.0f;
        it.wy = 0.0f;
        it.bright = 1.0f;
        const std::string pid = you.male ? "player_male" : "player_female";
        if( resolve( pid, TILE_CATEGORY::NONE, lit_level::LIT, 0u, it ) ) {
            it.kind = DrawKind::Billboard;
            it.height = 1.0f;
            out.push_back( it );
        }
    }
}

} // namespace cdda3d
