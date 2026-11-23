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

// Place Tactical Radar Map code & compact label data cleanly in upper RAM ($9B00-$BFFF)
#pragma code(code_hi)
#pragma data(data_hi)

static const char * const SHORT_NAMES[NUM_ISLANDS] = {
    "BANANA", "ORTE", "KOKOLA", "LAGOON",
    "PEAK", "BASE", "GILLIGAN", "RED",
    "SKEG", "BONE", "GIANTS", "CLAW",
    "LUKELAND", "ENTERPRS"
};

static byte to_scr(char ch) {
    if (ch >= 'A' && ch <= 'Z') return (byte)(ch - 'A' + 1);
    if (ch >= '0' && ch <= '9') return (byte)ch;
    if (ch == ':') return 58;
    if (ch == '-') return 45;
    if (ch == '/') return 47;
    if (ch == '[') return 27;
    if (ch == ']') return 29;
    return 32;
}

static void map_puts(byte row, byte col, const char *s, byte color) {
    unsigned int off = (unsigned int)row * 40 + col;
    byte *scr = SCREEN_HUD + off;
    byte *colr = COLOR_RAM + off;
    byte i = 0;
    while (s[i] && (byte)(col + i) < 40) {
        scr[i]  = to_scr(s[i]);
        colr[i] = color;
        i++;
    }
}

static void map_put_block(byte row, byte col, byte ch, byte color) {
    if (row >= 25 || col >= 40) return;
    unsigned int off = (unsigned int)row * 40 + col;
    SCREEN_HUD[off] = ch;
    COLOR_RAM[off]  = color;
}

// Convert world pixel (0..6143) -> Radar screen pixel (X: 24+8..24+311, Y: 50+16..50+175)
static int world_to_radar_x(int px) {
    // px / 20 -> 0..307, + 24 + 4
    unsigned int u = (unsigned int)px;
    return 28 + (int)(u / 20) - 12;
}

static int world_to_radar_y(int py) {
    // py / 38 -> 0..161, + 50 + 16
    unsigned int u = (unsigned int)py;
    return 66 + (int)(u / 38) - 10;
}

static void draw_radar_background(void) {
    memset(SCREEN_HUD, 32, 1000);
    memset(COLOR_RAM, VCOL_LT_BLUE, 1000);

    // Header bar (Row 0)
    map_puts(0, 1, "CYCLONE TACTICAL ARCHIPELAGO RADAR", VCOL_YELLOW);
    for (byte x = 0; x < 40; x++) {
        map_put_block(1, x, 76, VCOL_CYAN);
        map_put_block(22, x, 76, VCOL_CYAN);
    }

    // Subtle 3x3 sector grid dots
    for (byte r = 2; r < 22; r++) {
        map_put_block(r, 13, 46, VCOL_LT_BLUE);
        map_put_block(r, 26, 46, VCOL_LT_BLUE);
    }
    for (byte c = 0; c < 40; c += 2) {
        map_put_block(8,  c, 46, VCOL_LT_BLUE);
        map_put_block(15, c, 46, VCOL_LT_BLUE);
    }

    // Render all 14 islands accurately scaled from ISLANDS[] + helipad 'H' + labels!
    for (byte i = 0; i < NUM_ISLANDS; i++) {
        const IslandDesc *isl = &ISLANDS[i];
        // Scale world tile (0..767) -> char col (0..38), char row (2..21)
        byte cx = (byte)((unsigned int)(isl->world_x + (isl->width >> 1)) / 20);
        byte cy = (byte)(2 + ((unsigned int)(isl->world_y + (isl->height >> 1)) / 38));
        if (cx < 1) cx = 1;
        if (cx > 37) cx = 37;
        if (cy < 2) cy = 2;
        if (cy > 21) cy = 21;

        byte bw = (byte)(1 + (isl->width >> 5));
        byte bh = (byte)(1 + (isl->height >> 5));
        byte isl_col = (i == 5) ? VCOL_YELLOW : VCOL_LT_GREEN;

        for (byte dy = 0; dy < bh; dy++) {
            for (byte dx = 0; dx < bw; dx++) {
                map_put_block((byte)(cy + dy), (byte)(cx + dx), 72, isl_col); // Solid filled block char
            }
        }
        if (isl->has_helipad) {
            map_put_block(cy, cx, to_scr('H'), (i == 5) ? VCOL_WHITE : VCOL_YELLOW);
        }

        // Compact crisp label avoiding overlap (SKEG=8 sits between ENTERPRS=13 and LUKELAND=12)
        byte lrow, lcol;
        if (i == 8) {
            lrow = (byte)(cy - 1);
            lcol = (byte)(cx + bw + 1);
        } else {
            lrow = (cy > 2) ? (byte)(cy - 1) : (byte)(cy + bh);
            lcol = (cx > 2) ? (byte)(cx - 2) : 0;
        }
        if (lcol > 31) lcol = 31;
        map_puts(lrow, lcol, SHORT_NAMES[i], (i == 5) ? VCOL_WHITE : VCOL_CYAN);
    }

    // Bottom legend (Row 23-24)
    map_puts(23, 1, "CRATES: /5  WIND:   H:HELIPAD", VCOL_WHITE);
    map_puts(24, 1, " :HELI  O:CYCLONE  [M/SPACE: FLY]", VCOL_YELLOW);
    map_put_block(24, 1, 75, VCOL_CYAN); // Mini-helicopter icon in legend!
}

void radar_map_show(byte is_ntsc) {
    // 1. Pause playfield split & switch cleanly to full-screen 40x25 Hi-Res Radar ($C800 + $F000)
    raster_pause_split();
    audio_set_mode(AUDIO_MODE_RADAR);
    draw_radar_background();

    // Configure Radar hardware sprites on SCREEN_HUD sprite pointers ($CBF8-$CBFF)
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

    int bx = world_to_radar_x(324 * 8);
    int by = world_to_radar_y(382 * 8);
    vic_sprxy(2, bx, by);

    byte tick = 0;
    byte sec_cnt = 0;
    byte fps = is_ntsc ? 60 : 50;
    byte wait_release = 1;

    for (;;) {
        vic_waitFrame();
        tick++;

        // Keep Cyclone moving, mission timer ticking, and Radar Sonar pinging live!
        cyclone_tick_ai();
        audio_tick_radar(cyc_wind_force);
        sec_cnt++;
        if (sec_cnt >= fps) {
            sec_cnt = 0;
            if (game_time_s > 0) game_time_s--;
        }

        // Update pulsing Radar Sprites
        vic_sprxy(0, world_to_radar_x(heli_px), world_to_radar_y(heli_py));
        vic_sprxy(1, world_to_radar_x(cyc_px) - 12, world_to_radar_y(cyc_py) - 10);
        vic.spr_color[0] = ((tick >> 3) & 1) ? VCOL_YELLOW : VCOL_WHITE;
        vic.spr_color[1] = ((tick >> 2) & 1) ? VCOL_RED : VCOL_LT_RED;
        vic.spr_enable   = 0x07;

        // Live crate count & wind force readout on Row 23
        SCREEN_HUD[23 * 40 + 8]  = (byte)('0' + crates_collected);
        SCREEN_HUD[23 * 40 + 18] = (byte)('0' + (cyc_wind_force / 10));
        SCREEN_HUD[23 * 40 + 19] = (byte)('0' + (cyc_wind_force % 10));

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

    // Wait briefly for key release so we don't immediately re-trigger M/Fire on return
    for (byte d = 0; d < 8; d++) vic_waitFrame();

    // Restore Cockpit HUD, Helicopter sprite config, Playfield & Raster Split cleanly!
    vic.spr_enable   = 0x00;
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
