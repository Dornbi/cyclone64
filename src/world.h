#ifndef CYCLONE_WORLD_H
#define CYCLONE_WORLD_H

#include <c64/types.h>
#include "../generated/islands_data.h"

// Unpack compressed charsets from `tables` region into VIC Bank 3 ($E000-$F7FF)
void world_unpack_assets(void);

// Find island index (0..13) nearest/containing world coordinate (wx, wy), or 0xFF if open sea
byte world_find_island(int wx, int wy);

// Pre-filter active islands overlapping the current 40x19 viewport (+ 8-tile 3D margin)
void world_begin_viewport(int cam_tx, int cam_ty);

// Fast 3D projection of an entire 40-cell screen row (sy: 0..18) into dst[40]
void world_render_row(byte *dst, int cam_tx, int cam_ty, byte sy, byte south_view);

// Fast 3D projection of an entire 19-cell screen column (sx: 0..39) into dst (stride 40)
void world_render_col(byte *dst, int cam_tx, int cam_ty, byte sx, byte south_view);

// Exact terrain tile, elevation altitude (0..56 px), and surface type at world tile (wx, wy)
byte world_get_tile_at(int wx, int wy);
byte world_get_ground_alt(int wx, int wy);
byte world_get_surface_type(int wx, int wy);

// Per-char color lookup (0..15 for Color RAM)
extern const byte CHAR_COLORS[256];

// Tile height (1..8) and surface type (SURF_*) lookups for dense tile IDs 0..129
extern const byte TILE_HEIGHTS[NUM_DENSE_TILES];
extern const byte TILE_SURFACE[NUM_DENSE_TILES];

#endif
