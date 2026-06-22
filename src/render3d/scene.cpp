#include "scene.h"

#include <algorithm>
#include <string>

#include "avatar.h"
#include "cata_tiles.h"
#include "level_cache.h"
#include "lightmap.h"
#include "map.h"
#include "map_scale_constants.h"
#include "sdltiles.h"   // tilecontext
#include "type_id.h"

namespace cdda3d
{

void build_ground_sprites( std::vector<CellSprite> &out )
{
    out.clear();
    if( !tilecontext ) {
        return;
    }

    map &here = get_map();
    avatar &you = get_avatar();
    const tripoint_bub_ms ppos = you.pos_bub( here );
    const int pz = you.posz();
    const int px = ppos.x();
    const int py = ppos.y();
    const level_cache &ch = here.access_cache( pz );

    constexpr int radius = 40;
    const int xmin = std::max( 0, px - radius );
    const int xmax = std::min( MAPSIZE_X - 1, px + radius );
    const int ymin = std::max( 0, py - radius );
    const int ymax = std::min( MAPSIZE_Y - 1, py + radius );

    out.reserve( static_cast<size_t>( std::max( 0, xmax - xmin + 1 ) ) *
                 static_cast<size_t>( std::max( 0, ymax - ymin + 1 ) ) );

    for( int y = ymin; y <= ymax; ++y ) {
        for( int x = xmin; x <= xmax; ++x ) {
            const lit_level ll = ch.visibility_cache[x][y];
            if( ll == lit_level::BLANK ) {
                continue; // never seen
            }

            const tripoint_bub_ms p( x, y, pz );
            const std::string ter_id_str = here.ter( p ).id().str();
            const unsigned loc_rand = static_cast<unsigned>( x * 73856093 ) ^
                                      static_cast<unsigned>( y * 19349663 );

            SDL_Texture *tex = nullptr;
            SDL_Rect src{ 0, 0, 0, 0 };
            if( !tilecontext->cdda3d_lookup_sprite( ter_id_str, TILE_CATEGORY::TERRAIN, ll,
                    loc_rand, tex, src ) ) {
                continue;
            }

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

            CellSprite cs;
            cs.wx = static_cast<float>( x - px );
            cs.wz = static_cast<float>( y - py );
            cs.atlas = tex;
            cs.sx = src.x;
            cs.sy = src.y;
            cs.sw = src.w;
            cs.sh = src.h;
            cs.bright = bright;
            out.push_back( cs );
        }
    }
}

} // namespace cdda3d
