#include "memmap.h"
#include "world.h"
#include <c64/memmap.h>
#include <oscar.h>
#include <string.h>

// ============================================================================
// Packed 128 x 150 Dense Island Atlas in `worldmap` region ($5000 - $9AFF)
// ============================================================================
#pragma data(worldmap_sec)
static const byte ATLAS_DENSE[19200] = {
    #embed "../generated/atlas_dense.bin"
};

// ============================================================================
// 3D Tile Stack Tables & LZO-Compressed Charsets in `tables` region ($9B00-$BFFF)
// ============================================================================
#pragma data(tables_sec)
const byte CHAR_COLORS[256] = {
    #embed "../generated/char_colors.bin"
};

const byte TILE_HEIGHTS[NUM_DENSE_TILES] = {
    #embed "../generated/tile_heights.bin"
};

const byte TILE_SURFACE[NUM_DENSE_TILES] = {
    #embed "../generated/tile_surface.bin"
};

static const byte TILE_NORTH[NUM_DENSE_TILES * 8] = {
    #embed "../generated/tile_north.bin"
};

static const byte TILE_SOUTH[NUM_DENSE_TILES * 8] = {
    #embed "../generated/tile_south.bin"
};

// Boot-only LZO payloads staged in $C000-$CFFF (reclaimed as Screen A/B/HUD right after unpack!)
#pragma data(boot_lzo_sec)
static const byte CHARSET_NORTH_LZO[] = {
    #embed lzo "../generated/charset_north.bin"
};

static const byte CHARSET_HUD_LZO[] = {
    #embed lzo "../generated/hud_charset.bin"
};

static const byte SPRITES_D000_LZO[] = {
    #embed lzo "../generated/sprites_d000.bin"
};

#pragma data(data)

// Fast row-offset lookup table for 128-wide atlas (150 rows)
static const unsigned int ATLAS_ROW_OFF[150] = {
    0*128, 1*128, 2*128, 3*128, 4*128, 5*128, 6*128, 7*128, 8*128, 9*128,
    10*128, 11*128, 12*128, 13*128, 14*128, 15*128, 16*128, 17*128, 18*128, 19*128,
    20*128, 21*128, 22*128, 23*128, 24*128, 25*128, 26*128, 27*128, 28*128, 29*128,
    30*128, 31*128, 32*128, 33*128, 34*128, 35*128, 36*128, 37*128, 38*128, 39*128,
    40*128, 41*128, 42*128, 43*128, 44*128, 45*128, 46*128, 47*128, 48*128, 49*128,
    50*128, 51*128, 52*128, 53*128, 54*128, 55*128, 56*128, 57*128, 58*128, 59*128,
    60*128, 61*128, 62*128, 63*128, 64*128, 65*128, 66*128, 67*128, 68*128, 69*128,
    70*128, 71*128, 72*128, 73*128, 74*128, 75*128, 76*128, 77*128, 78*128, 79*128,
    80*128, 81*128, 82*128, 83*128, 84*128, 85*128, 86*128, 87*128, 88*128, 89*128,
    90*128, 91*128, 92*128, 93*128, 94*128, 95*128, 96*128, 97*128, 98*128, 99*128,
    100*128, 101*128, 102*128, 103*128, 104*128, 105*128, 106*128, 107*128, 108*128, 109*128,
    110*128, 111*128, 112*128, 113*128, 114*128, 115*128, 116*128, 117*128, 118*128, 119*128,
    120*128, 121*128, 122*128, 123*128, 124*128, 125*128, 126*128, 127*128, 128*128, 129*128,
    130*128, 131*128, 132*128, 133*128, 134*128, 135*128, 136*128, 137*128, 138*128, 139*128,
    140*128, 141*128, 142*128, 143*128, 144*128, 145*128, 146*128, 147*128, 148*128, 149*128,
};

// Active islands intersecting the current viewport (max 2 can ever overlap 40x19 + 8 margin)
static byte active_isl_count = 0;
static byte active_isl[4];

void world_unpack_assets(void) {
    // Must be called BEFORE raster IRQs start (while interrupts are disabled!).
    // Temporarily switch to MMAP_RAM ($01=$30) to unpack charsets ($E000/$F000)
    // AND static hardware sprites directly into VIC Bank 3 RAM under I/O ($D000-$DFFF)!
    byte old_map = mmap_set(MMAP_RAM);
    oscar_expand_lzo((char *)CHARSET_NORTH,   (const char *)CHARSET_NORTH_LZO);
    oscar_expand_lzo((char *)CHARSET_HUD,     (const char *)CHARSET_HUD_LZO);
    oscar_expand_lzo((char *)SPRITE_RAM_D000, (const char *)SPRITES_D000_LZO);
    mmap_set(old_map);
}

const byte *world_atlas_row(byte atlas_y, byte atlas_x) {
    return &ATLAS_DENSE[ATLAS_ROW_OFF[atlas_y] + atlas_x];
}

static byte last_query_isl = 5;

byte world_get_tile_at(int wx, int wy) {
    const IslandDesc *isl = &ISLANDS[last_query_isl];
    int dx = wx - isl->world_x;
    if ((unsigned int)dx < (unsigned int)isl->width) {
        int dy = wy - isl->world_y;
        if ((unsigned int)dy < (unsigned int)isl->height) {
            return ATLAS_DENSE[ATLAS_ROW_OFF[isl->atlas_y + (byte)dy] + isl->atlas_x + (byte)dx];
        }
    }
    for (byte i = 0; i < NUM_ISLANDS; i++) {
        isl = &ISLANDS[i];
        dx = wx - isl->world_x;
        if ((unsigned int)dx < (unsigned int)isl->width) {
            int dy = wy - isl->world_y;
            if ((unsigned int)dy < (unsigned int)isl->height) {
                last_query_isl = i;
                return ATLAS_DENSE[ATLAS_ROW_OFF[isl->atlas_y + (byte)dy] + isl->atlas_x + (byte)dx];
            }
        }
    }
    return 0;
}

byte world_get_ground_alt(int wx, int wy) {
    byte dt = world_get_tile_at(wx, wy);
    if (dt == 0) return 0;
    return (byte)((TILE_HEIGHTS[dt] - 1) << 3);
}

byte world_get_surface_type(int wx, int wy) {
    byte dt = world_get_tile_at(wx, wy);
    return TILE_SURFACE[dt];
}

byte world_find_island(int wx, int wy) {
    for (byte i = 0; i < NUM_ISLANDS; i++) {
        int ix = ISLANDS[i].world_x;
        int iy = ISLANDS[i].world_y;
        if (wx >= ix - 4 && wx < ix + (int)ISLANDS[i].width + 4 &&
            wy >= iy - 8 && wy < iy + (int)ISLANDS[i].height + 8) {
            return i;
        }
    }
    return 0xff;
}

void world_begin_viewport(int cam_tx, int cam_ty) {
    active_isl_count = 0;
    int vx0 = cam_tx;
    int vx1 = cam_tx + PF_COLS;
    int vy0 = cam_ty - 8;
    int vy1 = cam_ty + PF_ROWS + 8;

    for (byte i = 0; i < NUM_ISLANDS; i++) {
        int ix0 = ISLANDS[i].world_x;
        int iy0 = ISLANDS[i].world_y;
        int ix1 = ix0 + (int)ISLANDS[i].width;
        int iy1 = iy0 + (int)ISLANDS[i].height;
        if (ix0 < vx1 && ix1 > vx0 && iy0 < vy1 && iy1 > vy0) {
            active_isl[active_isl_count++] = i;
            if (active_isl_count == 4) break;
        }
    }
}

// Fast 3D row projection: paints ocean wave base, then overlays only active island slices z=0..7!
void world_render_row(byte *dst, int cam_tx, int cam_ty, byte sy, byte south_view) {
    int wy = south_view ? (cam_ty + (PF_ROWS - 1) - (int)sy) : (cam_ty + (int)sy);
    int wx_left = cam_tx;

    // 1. Fill calm ocean wave pattern across all 40 cols (~120 cycles!)
    byte phase = (byte)((south_view ? (cam_tx + (PF_COLS - 1) + wy) : (wx_left + wy)) & 3);
    if (!south_view) {
        for (byte sx = 0; sx < PF_COLS; sx++) {
            dst[sx] = phase;
            phase = (byte)((phase + 1) & 3);
        }
    } else {
        for (byte sx = 0; sx < PF_COLS; sx++) {
            dst[sx] = phase;
            phase = (byte)((phase - 1) & 3);
        }
    }

    if (active_isl_count == 0) return;

    const byte *stack_tbl = south_view ? TILE_SOUTH : TILE_NORTH;

    // 2. Painter's overlay: paint z = 0..7 back-to-front so taller front stacks overwrite cleanly!
    for (byte ai = 0; ai < active_isl_count; ai++) {
        const IslandDesc *isl = &ISLANDS[active_isl[ai]];
        int ix0 = isl->world_x;
        int iy0 = isl->world_y;
        byte iw = isl->width;
        byte ih = isl->height;

        // Horizontal intersection between viewport [cam_tx .. cam_tx+39] and island [ix0 .. ix0+iw-1]
        int x_start = (ix0 > wx_left) ? ix0 : wx_left;
        int x_end   = (ix0 + (int)iw < wx_left + PF_COLS) ? (ix0 + (int)iw) : (wx_left + PF_COLS);
        if (x_start >= x_end) continue;

        byte count = (byte)(x_end - x_start);
        byte ax0   = isl->atlas_x + (byte)(x_start - ix0);

        for (byte z = 0; z < 8; z++) {
            int row_wy = south_view ? (wy - (int)z) : (wy + (int)z);
            int dy = row_wy - iy0;
            if ((unsigned int)dy >= (unsigned int)ih) continue;

            const byte *arow = &ATLAS_DENSE[ATLAS_ROW_OFF[isl->atlas_y + (byte)dy] + ax0];
            if (!south_view) {
                byte *dp = dst + (byte)(x_start - wx_left);
                for (byte c = 0; c < count; c++) {
                    byte dt = arow[c];
                    if (dt != 0 && TILE_HEIGHTS[dt] > z) {
                        dp[c] = stack_tbl[((unsigned int)dt << 3) + z];
                    }
                }
            } else {
                // South view mirrors X: world x maps to sx = (cam_tx + 39) - x
                byte *dp = dst + (byte)((wx_left + (PF_COLS - 1)) - x_start);
                for (byte c = 0; c < count; c++) {
                    byte dt = arow[c];
                    if (dt != 0 && TILE_HEIGHTS[dt] > z) {
                        *dp = stack_tbl[((unsigned int)dt << 3) + z];
                    }
                    dp--;
                }
            }
        }
    }
}

// Fast 3D column projection: single sweep down 19+7 = 26 tiles per active island (~500 cycles!)
void world_render_col(byte *dst, int cam_tx, int cam_ty, byte sx, byte south_view) {
    int wx = south_view ? (cam_tx + (PF_COLS - 1) - (int)sx) : (cam_tx + (int)sx);

    // 1. Fill column with ocean waves (use tiny 19-byte local buffer for fast random z-writes!)
    byte col_buf[PF_ROWS];
    byte phase = (byte)((south_view ? (wx + cam_ty + (PF_ROWS - 1)) : (wx + cam_ty)) & 3);
    if (!south_view) {
        for (byte sy = 0; sy < PF_ROWS; sy++) {
            col_buf[sy] = phase;
            phase = (byte)((phase + 1) & 3);
        }
    } else {
        for (byte sy = 0; sy < PF_ROWS; sy++) {
            col_buf[sy] = phase;
            phase = (byte)((phase - 1) & 3);
        }
    }

    // 2. Single back-to-front vertical sweep over each intersecting island (at most 26 reads!)
    if (active_isl_count > 0) {
        const byte *stack_tbl = south_view ? TILE_SOUTH : TILE_NORTH;
        for (byte ai = 0; ai < active_isl_count; ai++) {
            const IslandDesc *isl = &ISLANDS[active_isl[ai]];
            int dx = wx - isl->world_x;
            if ((unsigned int)dx >= (unsigned int)isl->width) continue;

            byte ax = isl->atlas_x + (byte)dx;
            int iy0 = isl->world_y;
            byte ih = isl->height;

            // Sweep screen rows sy = 0 .. PF_ROWS+6 (26 rows) back-to-front!
            for (byte sy = 0; sy < PF_ROWS + 7; sy++) {
                int wy = south_view ? (cam_ty + (PF_ROWS - 1) - (int)sy) : (cam_ty + (int)sy);
                int dy = wy - iy0;
                if ((unsigned int)dy >= (unsigned int)ih) continue;

                byte dt = ATLAS_DENSE[ATLAS_ROW_OFF[isl->atlas_y + (byte)dy] + ax];
                if (dt == 0) continue;

                byte h = TILE_HEIGHTS[dt];
                const byte *st = &stack_tbl[(unsigned int)dt << 3];
                for (byte z = 0; z < h; z++) {
                    if (sy >= z) {
                        byte out_sy = (byte)(sy - z);
                        if (out_sy < PF_ROWS) {
                            col_buf[out_sy] = st[z];
                        }
                    }
                }
            }
        }
    }

    // 3. Scatter 19 bytes to destination screen column (stride 40)
    byte *p = dst + sx;
    for (byte sy = 0; sy < PF_ROWS; sy++) {
        *p = col_buf[sy];
        p += PF_COLS;
    }
}
