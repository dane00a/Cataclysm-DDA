#include "scene.h"

#include <algorithm>

#include "avatar.h"
#include "level_cache.h"
#include "lightmap.h"
#include "map.h"
#include "map_scale_constants.h"
#include "type_id.h"

namespace cdda3d
{

void build_ground_mesh( std::vector<float> &out )
{
    out.clear();

    map &here = get_map();
    avatar &you = get_avatar();
    const tripoint_bub_ms ppos = you.pos_bub( here );
    const int pz = you.posz();
    const int px = ppos.x();
    const int py = ppos.y();

    const level_cache &ch = here.access_cache( pz );

    // Visible window around the player, clamped to the reality bubble.
    constexpr int radius = 40;
    const int xmin = std::max( 0, px - radius );
    const int xmax = std::min( MAPSIZE_X - 1, px + radius );
    const int ymin = std::max( 0, py - radius );
    const int ymax = std::min( MAPSIZE_Y - 1, py + radius );

    out.reserve( static_cast<size_t>( std::max( 0, xmax - xmin + 1 ) ) *
                 static_cast<size_t>( std::max( 0, ymax - ymin + 1 ) ) * 6 * 6 );

    for( int y = ymin; y <= ymax; ++y ) {
        for( int x = xmin; x <= xmax; ++x ) {
            const lit_level ll = ch.visibility_cache[x][y];
            if( ll == lit_level::BLANK ) {
                continue; // never seen -> draw nothing
            }

            const tripoint_bub_ms p( x, y, pz );
            const unsigned h = static_cast<unsigned>( here.ter( p ).to_i() ) * 2654435761u;
            float r = 0.25f + 0.55f * ( ( h & 0xFF ) / 255.0f );
            float g = 0.25f + 0.55f * ( ( ( h >> 8 ) & 0xFF ) / 255.0f );
            float b = 0.25f + 0.55f * ( ( ( h >> 16 ) & 0xFF ) / 255.0f );

            float dim = 1.0f;
            switch( ll ) {
                case lit_level::DARK:
                    dim = 0.25f;
                    break;
                case lit_level::LOW:
                    dim = 0.5f;
                    break;
                case lit_level::MEMORIZED: {
                    // grey out memorized-but-not-visible tiles
                    const float grey = ( r + g + b ) / 3.0f;
                    r = g = b = grey;
                    dim = 0.4f;
                    break;
                }
                default:
                    break;
            }
            r *= dim;
            g *= dim;
            b *= dim;

            // Player-relative world coords: X east, Z south, Y up.
            const float wx = static_cast<float>( x - px );
            const float wz = static_cast<float>( y - py );
            const float x0 = wx - 0.5f, x1 = wx + 0.5f;
            const float z0 = wz - 0.5f, z1 = wz + 0.5f;

            const auto push = [&]( float vx, float vz ) {
                out.push_back( vx );
                out.push_back( 0.0f );
                out.push_back( vz );
                out.push_back( r );
                out.push_back( g );
                out.push_back( b );
            };
            // two triangles per cell
            push( x0, z0 );
            push( x1, z0 );
            push( x1, z1 );
            push( x0, z0 );
            push( x1, z1 );
            push( x0, z1 );
        }
    }
}

} // namespace cdda3d
