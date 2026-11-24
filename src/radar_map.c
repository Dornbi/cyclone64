#include "memmap.h"
#include "world.h"
#include "raster.h"
#include "scroller.h"
#include "helicopter.h"
#include "entities.h"
#include "cyclone.h"
#include "hud.h"
#include "audio.h"
#include "radar_map.h"

#include <c64/vic.h>
#include <c64/joystick.h>
#include <c64/keyboard.h>
#include <string.h>

// Place compact bitmap drawing primitives in $CC00-$CFFF (code_cc) and main radar in code_hi ($9B00-$BFFF)
#pragma code(code_cc)
#pragma data(data_hi)

#define BITMAP_RAM      ((byte *)0xe000)
#define FONT_BACKUP     SCREEN_B          // Active HUD charset ($F000..$F27F) backed up at $C400
#define MEMPTR_BITMAP   0x28              // Screen $C800 + Bitmap $E000 (VIC Bank 3)

static const char * const SHORT_NAMES[NUM_ISLANDS] = {
    "BANANA", "ORTE", "KOKOLA", "LAGOON",
    "PEAK", "BASE", "GILLIG", "RED",
    "SKEG", "BONE", "GIANTS", "CLAW",
    "LUKELD", "ENTPRS"
};

// Precomputed bitmap row base addresses (row 0..24 -> $E000 + row * 320)
static const unsigned int BM_ROW_OFF[25] = {
    0*320,  1*320,  2*320,  3*320,  4*320,
    5*320,  6*320,  7*320,  8*320,  9*320,
    10*320, 11*320, 12*320, 13*320, 14*320,
    15*320, 16*320, 17*320, 18*320, 19*320,
    20*320, 21*320, 22*320, 23*320, 24*320
};

static const byte BIT_MASK[8] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

// Plot a single 1x1 pixel at (bx: 0..319, by: 0..199) in true 320x200 Hi-Res Bitmap RAM
static void bm_plot(unsigned int bx, byte by) {
    if (bx >= 320 || by >= 200) return;
    unsigned int off = BM_ROW_OFF[by >> 3] + (bx & 0x01f8) + (by & 7);
    BITMAP_RAM[off] |= BIT_MASK[bx & 7];
}

// Set the Hi-Res Bitmap 8x8 cell foreground/background color attribute in $C800
static void bm_set_cell_col(byte col, byte row, byte fg, byte bg) {
    if (col >= 40 || row >= 25) return;
    SCREEN_HUD[(unsigned int)row * 40 + col] = (byte)((fg << 4) | (bg & 0x0f));
}

// Stamp a crisp 8x8 glyph from backed-up HUD charset ($C400) onto cell (col, row)
static void bm_putc(byte row, byte col, byte code, byte fg, byte bg) {
    if (col >= 40 || row >= 25) return;
    byte *dst = BITMAP_RAM + BM_ROW_OFF[row] + ((unsigned int)col << 3);
    const byte *src = FONT_BACKUP + ((unsigned int)code << 3);
    for (byte r = 0; r < 8; r++) {
        dst[r] = src[r];
    }
    SCREEN_HUD[(unsigned int)row * 40 + col] = (byte)((fg << 4) | (bg & 0x0f));
}

static void bm_puts(byte row, byte col, const char *s, byte fg, byte bg) {
    byte i = 0;
    while (s[i] && (byte)(col + i) < 40) {
        bm_putc(row, (byte)(col + i), hud_ascii_to_code(s[i]), fg, bg);
        i++;
    }
}

// Unified exact projection: world tile (0..767) -> 256x160 tactical viewport (bx=32..287, by=20..179)
static unsigned int tile_to_bm_x(unsigned int wx) {
    return 32 + (wx / 3);
}

static byte tile_to_bm_y(unsigned int wy) {
    return (byte)(20 + ((wy * 5) / 24));
}

#pragma code(code_hi)

static void draw_bitmap_radar(void) {
    // 1. Clear entire 8,000-byte 320x200 Bitmap ($E000..$FF3F) & initialize Color Matrix ($C800)
    memset(BITMAP_RAM, 0, 8000);
    // Header rows 0..1: black bar; Ocean rows 2..22: Light Blue grid on Blue; Footer 23..24: black bar
    memset(SCREEN_HUD,            (VCOL_YELLOW << 4)  | VCOL_BLACK, 80);
    memset(SCREEN_HUD + 80,       (VCOL_LT_BLUE << 4) | VCOL_BLUE,  840);
    memset(SCREEN_HUD + 23 * 40,  (VCOL_WHITE << 4)   | VCOL_BLACK, 80);

    // 2. Crisp top header & fast 40-byte horizontal divider lines (y=12 & y=184)
    bm_puts(0, 4, "CYCLONE ARCHIPELAGO TACTICAL MAP", VCOL_YELLOW, VCOL_BLACK);
    {
        byte *d1 = BITMAP_RAM + BM_ROW_OFF[1]  + 4; // y = 12 (row 1, line 4)
        byte *d2 = BITMAP_RAM + BM_ROW_OFF[23] + 0; // y = 184 (row 23, line 0)
        for (byte c = 0; c < 40; c++) {
            *d1 = 0xff; d1 += 8;
            *d2 = 0xff; d2 += 8;
            SCREEN_HUD[40 + c]      = (VCOL_CYAN << 4) | VCOL_BLACK;
            SCREEN_HUD[23 * 40 + c] = (VCOL_CYAN << 4) | VCOL_BLACK;
        }
    }

    // 3. Fast byte-aligned 256x160 frame + 3x3 sector dotted grid + compass ticks
    {
        byte gy1 = tile_to_bm_y(256); // 73
        byte gy2 = tile_to_bm_y(512); // 126
        unsigned int gx1 = tile_to_bm_x(256); // 117
        unsigned int gx2 = tile_to_bm_x(512); // 202

        byte *ft = BITMAP_RAM + BM_ROW_OFF[19 >> 3]  + (19 & 7)  + 32;
        byte *fb = BITMAP_RAM + BM_ROW_OFF[180 >> 3] + (180 & 7) + 32;
        byte *g1 = BITMAP_RAM + BM_ROW_OFF[gy1 >> 3] + (gy1 & 7) + 32;
        byte *g2 = BITMAP_RAM + BM_ROW_OFF[gy2 >> 3] + (gy2 & 7) + 32;
        for (byte c = 0; c < 32; c++) {
            *ft = 0xaa; ft += 8;
            *fb = 0xaa; fb += 8;
            *g1 = 0x44; g1 += 8;
            *g2 = 0x44; g2 += 8;
        }
        for (byte y = 19; y <= 180; y += 2) {
            bm_plot(31,  y);
            bm_plot(288, y);
            if ((y & 3) == 1) {
                bm_plot(gx1, y);
                bm_plot(gx2, y);
            }
        }
    }
    bm_putc(2,  1,  hud_ascii_to_code('N'), VCOL_CYAN, VCOL_BLUE);
    bm_putc(12, 1,  hud_ascii_to_code('W'), VCOL_CYAN, VCOL_BLUE);
    bm_putc(12, 38, hud_ascii_to_code('E'), VCOL_CYAN, VCOL_BLUE);
    bm_putc(22, 1,  hud_ascii_to_code('S'), VCOL_CYAN, VCOL_BLUE);

    // 4. Ultra-fast direct 1-pixel rasterizer for ALL 14 islands from ATLAS_DENSE (~0.06s total!)
    //    Steps (dx+=2, dy+=2) -> zero gaps (1 px = 3 x 4.8 tiles), zero inner-loop divs/muls/calls!
    for (byte i = 0; i < NUM_ISLANDS; i++) {
        const IslandDesc *isl = &ISLANDS[i];
        byte col_attr = (byte)(((i == 5 ? VCOL_YELLOW : VCOL_LT_GREEN) << 4) | VCOL_BLUE);
        unsigned int ix0 = (unsigned int)isl->world_x;
        unsigned int iy0 = (unsigned int)isl->world_y;
        byte iw = isl->width;
        byte ih = isl->height;

        unsigned int bx0 = tile_to_bm_x(ix0);
        byte sub0 = (byte)(ix0 % 3);

        unsigned int pad_bx = 0;
        byte pad_by = 0;
        byte found_pad = 0;

        for (byte dy = 0; dy < ih; dy += 2) {
            byte by = tile_to_bm_y(iy0 + dy);
            byte row = by >> 3;
            byte *row_bm  = BITMAP_RAM + BM_ROW_OFF[row] + (by & 7);
            byte *row_col = SCREEN_HUD + (unsigned int)row * 40;
            const byte *arow = world_atlas_row((byte)(isl->atlas_y + dy), isl->atlas_x);

            unsigned int bx_byte = bx0 & 0x01f8;
            byte bx_bit = (byte)(bx0 & 7);
            byte bx_col = (byte)(bx0 >> 3);
            byte sub = sub0;

            for (byte dx = 0; dx < iw; dx += 2) {
                byte dt = arow[dx];
                if (dt != 0) {
                    row_bm[bx_byte] |= BIT_MASK[bx_bit];
                    row_col[bx_col]  = col_attr;
                    if (!found_pad && TILE_SURFACE[dt] == SURF_HELIPAD) {
                        pad_bx = bx_byte + bx_bit;
                        pad_by = by;
                        found_pad = 1;
                    }
                }
                sub += 2;
                if (sub >= 3) {
                    sub -= 3;
                    bx_bit++;
                    if (bx_bit == 8) {
                        bx_bit = 0;
                        bx_byte += 8;
                        bx_col++;
                    }
                }
            }
        }

        // Mark real Helipad coordinate with a crisp bright White 3x3 cross
        if (found_pad) {
            bm_plot(pad_bx, pad_by);
            if (pad_bx > 0) bm_plot(pad_bx - 1, pad_by);
            bm_plot(pad_bx + 1, pad_by);
            if (pad_by > 0) bm_plot(pad_bx, (byte)(pad_by - 1));
            bm_plot(pad_bx, (byte)(pad_by + 1));
            bm_set_cell_col((byte)(pad_bx >> 3), (byte)(pad_by >> 3), VCOL_WHITE, VCOL_BLUE);
        }
    }

    // 5. Place crisp island name labels in open ocean cells adjacent to each island (zero land clash!)
    for (byte i = 0; i < NUM_ISLANDS; i++) {
        const IslandDesc *isl = &ISLANDS[i];
        byte cx = (byte)(tile_to_bm_x((unsigned int)(isl->world_x + (isl->width >> 1))) >> 3);
        byte top_row = (byte)(tile_to_bm_y((unsigned int)isl->world_y) >> 3);
        byte bot_row = (byte)(tile_to_bm_y((unsigned int)(isl->world_y + isl->height)) >> 3);

        byte lrow = (top_row > 2) ? (byte)(top_row - 1) : (byte)(bot_row + 1);
        byte lcol = (cx >= 2) ? (byte)(cx - 2) : 2;
        // Clean manual offsets for tightly clustered islands so every label is 100% legible
        if (i == 1) { lrow = top_row; lcol = (byte)(cx - 5); }      // ORTE left of island
        else if (i == 2) { lrow = (byte)(bot_row + 1); }            // KOKOLA below (GILLIG sits above)
        else if (i == 7) { lrow = (byte)(bot_row + 1); }            // RED below (ORTE sits above)
        else if (i == 8) { lrow = top_row; lcol = (byte)(cx + 2); } // SKEG right of island
        else if (i == 12) { lrow = (byte)(bot_row + 1); }           // LUKELD below
        if (lcol > 32) lcol = 32;
        if (lrow > 22) lrow = 22;

        // Only write onto empty water cells so we never overwrite island coastline pixels!
        const char *nm = SHORT_NAMES[i];
        byte lfg = (i == 5) ? VCOL_WHITE : VCOL_CYAN;
        for (byte k = 0; nm[k] && (byte)(lcol + k) < 38; k++) {
            byte cell_attr = SCREEN_HUD[(unsigned int)lrow * 40 + (lcol + k)];
            if ((cell_attr >> 4) == VCOL_LT_BLUE) {
                bm_putc(lrow, (byte)(lcol + k), hud_ascii_to_code(nm[k]), lfg, VCOL_BLUE);
            }
        }
    }

    // 6. Footer status & legend bar (Row 24 = pixel rows 192..199, pure black bg)
    bm_puts(24, 1, "CR: /5 WND:   ", VCOL_WHITE, VCOL_BLACK);
    bm_putc(24, 16, 75, VCOL_CYAN, VCOL_BLACK); // Mini helicopter glyph
    bm_puts(24, 17, ":HELI O:STORM [M:FLY]", VCOL_YELLOW, VCOL_BLACK);
}

void radar_map_show(byte is_ntsc) {
    // 1. Pause playfield raster IRQ split & switch audio to radar sonar
    raster_pause_split();
    audio_set_mode(AUDIO_MODE_RADAR);

    // 2. Blank screen briefly (full 100% CPU speed, zero charset-overwrite glitch!) & back up charsets
    vic.color_border = VCOL_BLACK;
    vic.ctrl1        = 0x0b; // DEN=0: clean black screen & zero VIC badline steals while rasterizing!
    memcpy(SCREEN_A, CHARSET_NORTH, 640);
    memcpy(SCREEN_B, CHARSET_HUD,   640);

    // 3. Draw the 320x200 pixel-accurate archipelago bitmap & snap cleanly into BMM=1!
    vic.color_back   = VCOL_BLUE;
    draw_bitmap_radar();

    vic_waitFrame();
    vic.ctrl2  = 0x08;           // 40-col Hi-Res (MCM=0)
    vic.memptr = MEMPTR_BITMAP;  // Color Matrix $C800 + Bitmap $E000
    vic.ctrl1  = 0x3b;           // True 320x200 Hi-Res Bitmap Mode (BMM=1, DEN=1, 25-row)

    // 4. Configure pulsing Radar Hardware Sprites ($CBF8-$CBFF -> $D000 sprites)
    byte *hud_ptrs = SCREEN_HUD + 0x03f8;
    hud_ptrs[0] = SPR_PTR_RADAR_HELI;
    hud_ptrs[1] = SPR_PTR_RADAR_CYC;
    hud_ptrs[2] = SPR_PTR_RADAR_BASE;

    vic.spr_multi    = 0x00;
    vic.spr_expand_x = 0x02; // 2x danger radius ring on Cyclone!
    vic.spr_expand_y = 0x02;
    vic.spr_color[0] = VCOL_WHITE;
    vic.spr_color[1] = VCOL_RED;
    vic.spr_color[2] = VCOL_CYAN;

    // Exact sprite position matching bitmap pixel (bx, by) -> VIC (24 + bx, 50 + by)
    int base_sx = 24 + (int)tile_to_bm_x(324) - 12;
    int base_sy = 50 + (int)tile_to_bm_y(382) - 10;
    vic_sprxy(2, base_sx, base_sy);

    byte tick = 0;
    byte sec_cnt = 0;
    byte fps = is_ntsc ? 60 : 50;
    byte wait_release = 1;

    for (;;) {
        vic_waitFrame();
        tick++;

        // Keep Cyclone moving, timer ticking, and sonar pinging live while viewing radar!
        cyclone_tick_ai();
        audio_tick_radar(cyc_wind_force);
        sec_cnt++;
        if (sec_cnt >= fps) {
            sec_cnt = 0;
            if (game_time_s > 0) game_time_s--;
        }

        // Update pulsing Helicopter & Cyclone hardware sprites at exact 1-pixel bitmap coords
        int hsx = 24 + (int)tile_to_bm_x((unsigned int)(heli_px >> 3)) - 12;
        int hsy = 50 + (int)tile_to_bm_y((unsigned int)(heli_py >> 3)) - 10;
        int csx = 24 + (int)tile_to_bm_x((unsigned int)(cyc_px >> 3))  - 24;
        int csy = 50 + (int)tile_to_bm_y((unsigned int)(cyc_py >> 3))  - 20;

        vic_sprxy(0, hsx, hsy);
        vic_sprxy(1, csx, csy);
        vic.spr_color[0] = ((tick >> 3) & 1) ? VCOL_YELLOW : VCOL_WHITE;
        vic.spr_color[1] = ((tick >> 2) & 1) ? VCOL_RED : VCOL_LT_RED;
        vic.spr_enable   = 0x07;

        // Live crate & wind telemetry stamped cleanly into Bitmap footer row 24
        if ((tick & 7) == 1) {
            bm_putc(24, 4,  (byte)('0' + crates_collected),    VCOL_LT_GREEN, VCOL_BLACK);
            bm_putc(24, 12, (byte)('0' + (cyc_wind_force / 10)), VCOL_LT_RED, VCOL_BLACK);
            bm_putc(24, 13, (byte)('0' + (cyc_wind_force % 10)), VCOL_LT_RED, VCOL_BLACK);
        }

        // Poll M, Space, or Fire to exit back to live 3D flight
        joy_poll(1);
        keyb_poll();
        byte exit_key = (key_pressed(KSCAN_M) || key_pressed(KSCAN_SPACE) || joyb[1]) ? 1 : 0;
        if (wait_release) {
            if (!exit_key) wait_release = 0;
        } else if (exit_key) {
            break;
        }
    }

    for (byte d = 0; d < 8; d++) vic_waitFrame();

    // 5. Blank briefly & restore BOTH charsets cleanly from $C000/$C400 -> $E000/$F000 (100% lossless!)
    vic.spr_enable = 0x00;
    vic.ctrl1      = 0x0b; // Blank cleanly during 1-frame charset restore
    memset(CHARSET_NORTH, 0, 2048);
    memcpy(CHARSET_NORTH, SCREEN_A, 640);
    memset(CHARSET_HUD,   0, 2048);
    memcpy(CHARSET_HUD,   SCREEN_B, 640);

    // 6. Restore Helicopter sprite config, Cockpit HUD, Audio, Playfield & Raster Split!
    vic.spr_expand_x = 0x00;
    vic.spr_expand_y = 0x00;
    vic.spr_multi    = 0x01;
    vic.spr_mcolor0  = VCOL_WHITE;
    vic.spr_mcolor1  = VCOL_BLACK;
    vic.spr_color[0] = VCOL_LT_RED;
    vic.spr_color[1] = VCOL_BLACK;
    vic.spr_color[2] = VCOL_DARK_GREY;

    hud_init();
    audio_set_mode(AUDIO_MODE_GAME);
    raster_resume_split();
    scroller_redraw_full();
}

#pragma code(code)
#pragma data(data)
