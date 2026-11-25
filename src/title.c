#include "memmap.h"
#include "raster.h"
#include "scroller.h"
#include "helicopter.h"
#include "hud.h"
#include "audio.h"
#include "title.h"

#include <c64/vic.h>
#include <c64/joystick.h>
#include <c64/keyboard.h>
#include <string.h>

#pragma code(code_hi)
#pragma data(data_hi)

unsigned int title_high_score10 = 0;

void title_record_score(unsigned int score10) {
    if (score10 > title_high_score10) {
        title_high_score10 = score10;
    }
}

static void t_puts(byte row, byte col, const char *s, byte color) {
    unsigned int off = (unsigned int)row * 40 + col;
    byte *scr  = SCREEN_HUD + off;
    byte *colr = COLOR_RAM + off;
    byte i = 0;
    while (s[i] && (byte)(col + i) < 40) {
        scr[i]  = hud_ascii_to_code(s[i]);
        colr[i] = color;
        i++;
    }
}

static void t_put_score(byte row, byte col, unsigned int score10, byte color) {
    unsigned int off = (unsigned int)row * 40 + col;
    byte *scr  = SCREEN_HUD + off;
    byte *colr = COLOR_RAM + off;
    byte d4 = 0, d3 = 0, d2 = 0, d1 = 0;
    while (score10 >= 10000) { score10 -= 10000; d4++; }
    while (score10 >= 1000)  { score10 -= 1000;  d3++; }
    while (score10 >= 100)   { score10 -= 100;   d2++; }
    while (score10 >= 10)    { score10 -= 10;    d1++; }
    scr[0] = (byte)('0' + d4);      colr[0] = color;
    scr[1] = (byte)('0' + d3);      colr[1] = color;
    scr[2] = (byte)('0' + d2);      colr[2] = color;
    scr[3] = (byte)('0' + d1);      colr[3] = color;
    scr[4] = (byte)('0' + score10); colr[4] = color;
    scr[5] = '0';                   colr[5] = color;
}

// Giant 5-row bold pixel-block CYCLONE banner (34 chars wide, centered at col 3)
static const char * const LOGO_ROWS[5] = {
    " ###  #   #  ###  #     ###  #  # ####",
    "#   #  # #  #   # #    #   # ## # #   ",
    "#       #   #     #    #   # #### ### ",
    "#   #   #   #   # #    #   # # ## #   ",
    " ###    #    ###  ####  ###  #  # ####"
};

static const byte LOGO_COLS[5] = {
    VCOL_WHITE, VCOL_YELLOW, VCOL_ORANGE, VCOL_LT_RED, VCOL_RED
};

#pragma code(code_hi)

static void draw_options_status(byte is_ntsc) {
    t_puts(15, 5, "[C] CONTROLS :", VCOL_CYAN);
    t_puts(15, 20, (heli_ctrl_mode == CTRL_CLASSIC) ? "CLASSIC " : "ARCADE  ", VCOL_WHITE);

    t_puts(16, 5, "[V] AUDIO    :", VCOL_CYAN);
    if (audio_muted) {
        t_puts(16, 20, "MUTED     ", VCOL_LT_RED);
    } else {
        t_puts(16, 20, audio_is_8580 ? "SID 8580  " : "SID 6581  ", VCOL_LT_GREEN);
    }

    t_puts(17, 5, "    SYSTEM   :", VCOL_CYAN);
    t_puts(17, 20, is_ntsc ? "NTSC 60HZ" : "PAL 50HZ ", VCOL_YELLOW);

    t_puts(18, 5, "    HI-SCORE :", VCOL_CYAN);
    t_put_score(18, 20, title_high_score10, VCOL_YELLOW);
}

static void draw_title_screen(byte is_ntsc) {
    memset(SCREEN_HUD, 32, 1000);
    memset(COLOR_RAM, VCOL_CYAN, 1000);

    // Top & bottom storm horizon dividers
    for (byte x = 0; x < 40; x++) {
        SCREEN_HUD[0 * 40 + x]  = 76; COLOR_RAM[0 * 40 + x]  = VCOL_CYAN;
        SCREEN_HUD[12 * 40 + x] = 76; COLOR_RAM[12 * 40 + x] = VCOL_LT_BLUE;
        SCREEN_HUD[24 * 40 + x] = 76; COLOR_RAM[24 * 40 + x] = VCOL_CYAN;
    }

    // 5-row sunset-gradient CYCLONE block logo (rows 1..5)
    for (byte r = 0; r < 5; r++) {
        t_puts((byte)(1 + r), 1, LOGO_ROWS[r], LOGO_COLS[r]);
    }

    // Edition subtitle (row 7 — cleanly above animated sky!)
    t_puts(7, 10, "COMMODORE 64 EDITION", VCOL_LT_GREY);

    // Tropical island silhouette & helipad along row 11 (above divider)
    for (byte x = 2; x < 12; x++) {
        SCREEN_HUD[11 * 40 + x] = 72;
        COLOR_RAM[11 * 40 + x]  = VCOL_LT_GREEN;
    }
    SCREEN_HUD[11 * 40 + 6] = 1 + ('H' - 'A');
    COLOR_RAM[11 * 40 + 6]  = VCOL_YELLOW;

    // Mission brief
    t_puts(13, 2, "RESCUE 5 MEDICAL CRATES BEFORE STORM!", VCOL_YELLOW);

    // Interactive Options & Hardware readout (rows 15..18)
    draw_options_status(is_ntsc);

    // Controls quick reference (rows 20..21)
    t_puts(20, 1, "IJKL/JOY2:FLY  W/S:ALT  M/SPC:RADAR", VCOL_LT_GREY);
    t_puts(21, 1, "RET:VIEW  V:SID  C:MODE  [H:HELP]", VCOL_CYAN);

    // Launch prompt (row 23)
    t_puts(23, 4, "PRESS FIRE OR SPACE TO LAUNCH!", VCOL_WHITE);
}

static void draw_help_screen(void) {
    memset(SCREEN_HUD, 32, 1000);
    memset(COLOR_RAM, VCOL_WHITE, 1000);

    for (byte x = 0; x < 40; x++) {
        SCREEN_HUD[1 * 40 + x]  = 76; COLOR_RAM[1 * 40 + x]  = VCOL_CYAN;
        SCREEN_HUD[17 * 40 + x] = 76; COLOR_RAM[17 * 40 + x] = VCOL_LT_BLUE;
        SCREEN_HUD[22 * 40 + x] = 76; COLOR_RAM[22 * 40 + x] = VCOL_CYAN;
    }

    t_puts(0,  5, "CYCLONE - PILOT CONTROLS / HELP", VCOL_YELLOW);

    t_puts(3,  2, "I / J / K / L", VCOL_YELLOW); t_puts(3,  17, ": FLY DIRECTION",  VCOL_WHITE);
    t_puts(4,  2, "JOYSTICK 2",    VCOL_YELLOW); t_puts(4,  17, ": FLY DIRECTION",  VCOL_WHITE);
    t_puts(6,  2, "W / S",         VCOL_YELLOW); t_puts(6,  17, ": ALTITUDE UP/DN", VCOL_WHITE);
    t_puts(7,  2, "FIRE - UP/DN",  VCOL_YELLOW); t_puts(7,  17, ": ALTITUDE UP/DN", VCOL_WHITE);
    t_puts(9,  2, "RETURN / TAP",  VCOL_YELLOW); t_puts(9,  17, ": FLIP N/S VIEW",  VCOL_WHITE);
    t_puts(10, 2, "M  OR  SPACE",  VCOL_YELLOW); t_puts(10, 17, ": TACTICAL RADAR", VCOL_WHITE);
    t_puts(11, 2, "C",             VCOL_YELLOW); t_puts(11, 17, ": CLASSIC/ARCADE", VCOL_WHITE);
    t_puts(12, 2, "V",             VCOL_YELLOW); t_puts(12, 17, ": SID AUDIO MUTE", VCOL_WHITE);
    t_puts(13, 2, "H",             VCOL_YELLOW); t_puts(13, 17, ": THIS HELP",      VCOL_WHITE);
    t_puts(14, 2, "1 - 7",         VCOL_YELLOW); t_puts(14, 17, ": HELIPAD WARP",   VCOL_WHITE);
    t_puts(15, 2, "T / RUN-STOP",  VCOL_YELLOW); t_puts(15, 17, ": TITLE [AT END]", VCOL_WHITE);

    t_puts(18, 2, "HOVER LOW TO WINCH CRATES / PEOPLE",   VCOL_LT_GREEN);
    t_puts(19, 2, "LAND ON WHITE HELIPADS TO REFUEL",     VCOL_CYAN);
    t_puts(20, 2, "RETURN ALL 5 CRATES TO BASE ISLAND!",  VCOL_YELLOW);

    t_puts(23, 4, "PRESS H / SPACE / FIRE TO RETURN", VCOL_WHITE);
}

void help_screen_show(byte is_ntsc, byte in_flight) {
    (void)is_ntsc;
    if (in_flight) {
        raster_pause_split();
        audio_set_mode(AUDIO_MODE_SILENT);
    } else {
        vic.spr_enable = 0x00;
    }

    vic.color_border = VCOL_BLACK;
    vic.color_back   = VCOL_BLUE;
    draw_help_screen();

    byte wait_rel = 1;
    byte tick = 0;

    for (;;) {
        vic_waitFrame();
        tick++;
        if (!in_flight) audio_tick_title();

        // Pulse bottom prompt
        byte pcol = ((tick >> 4) & 1) ? VCOL_YELLOW : VCOL_WHITE;
        for (byte i = 4; i < 36; i++) {
            COLOR_RAM[23 * 40 + i] = pcol;
        }

        joy_poll(1);
        keyb_poll();
        byte exit_k = (key_pressed(KSCAN_H) || key_pressed(KSCAN_SPACE) ||
                       key_pressed(KSCAN_RETURN) || joyb[1]) ? 1 : 0;
        if (wait_rel) {
            if (!exit_k) wait_rel = 0;
        } else if (exit_k) {
            break;
        }
    }

    for (byte d = 0; d < 8; d++) {
        vic_waitFrame();
        if (!in_flight) audio_tick_title();
    }

    if (in_flight) {
        // Restore HUD, sprite config, audio, raster split & full multicolor playfield cleanly!
        vic.spr_enable   = 0x00;
        vic.spr_expand_x = 0x00;
        vic.spr_expand_y = 0x00;
        vic.spr_multi    = 0x02;
        vic.spr_mcolor0  = VCOL_WHITE;
        vic.spr_mcolor1  = VCOL_BLACK;
        vic.spr_color[0] = VCOL_BLACK;
        vic.spr_color[1] = VCOL_LT_RED;
        vic.spr_color[2] = VCOL_DARK_GREY;

        hud_init();
        audio_set_mode(AUDIO_MODE_GAME);
        raster_resume_split();
        scroller_redraw_full();
    }
}

void title_screen_show(byte is_ntsc) {
    raster_pause_split();
    vic.color_border = VCOL_BLACK;
    vic.color_back   = VCOL_BLUE;

    draw_title_screen(is_ntsc);
    audio_set_mode(AUDIO_MODE_TITLE);

    // Configure animated Title Screen Hardware Sprites on SCREEN_HUD pointers ($CBF8-$CBFF):
    //   0 = Spinning Rotor Overlay (Hi-Res, drawn IN FRONT of fuselage!)
    //   1 = Rescue Helicopter (MC)
    //   2 = 2x Swirling Red Cyclone Funnel (MC)
    //   3 = Pulsing Medical Crate Beacon (MC)
    byte *ptrs = SCREEN_HUD + 0x03f8;
    ptrs[0] = SPR_PTR_ROTOR_BASE;
    ptrs[1] = (byte)(SPR_PTR_HELI_BASE + 4);
    ptrs[2] = SPR_PTR_CYC_BASE;
    ptrs[3] = SPR_PTR_CRATE_BASE;

    vic.spr_mcolor0  = VCOL_WHITE;
    vic.spr_mcolor1  = VCOL_BLACK;
    vic.spr_multi    = 0x0e; // Sprites 1, 2, 3 multicolor; 0 hi-res rotor
    vic.spr_expand_x = 0x04; // 2x expanded Cyclone funnel on Sprite 2!
    vic.spr_expand_y = 0x04;
    vic.spr_color[0] = VCOL_BLACK;
    vic.spr_color[1] = VCOL_LT_RED;
    vic.spr_color[2] = VCOL_RED;
    vic.spr_color[3] = VCOL_YELLOW;

    int  hx   = 64;
    sbyte hdx = 1;
    byte tick = 0;
    byte wait_rel = 1;
    byte prev_c   = 0;
    byte prev_v   = 0;
    byte prev_h   = 0;

    for (;;) {
        vic_waitFrame();
        tick++;
        audio_tick_title();

        // Patrol helicopter smoothly across tropical sky above the island silhouette!
        hx += hdx;
        if (hx > 215) hdx = -1;
        if (hx < 48)  hdx =  1;
        int hy = 122 + (int)((tick & 16) ? 1 : 0);

        ptrs[0] = (byte)(SPR_PTR_ROTOR_BASE + ((tick >> 1) & 3));
        ptrs[1] = (byte)(SPR_PTR_HELI_BASE + ((hdx > 0) ? 4 : 12));
        ptrs[2] = (byte)(SPR_PTR_CYC_BASE + ((tick >> 2) & 3));
        ptrs[3] = (byte)(SPR_PTR_CRATE_BASE + ((tick >> 4) & 1));

        vic_sprxy(0, hx, hy - 2);
        vic_sprxy(1, hx, hy);
        vic_sprxy(2, 266 + (int)((tick & 31) < 16 ? (tick & 15) >> 2 : (15 - (tick & 15)) >> 2), 111);
        vic_sprxy(3, 66, 134);
        vic.spr_enable = 0x0f;

        // Pulse launch prompt color smoothly
        byte pcol = ((tick >> 4) & 1) ? VCOL_YELLOW : VCOL_WHITE;
        for (byte i = 4; i < 34; i++) {
            COLOR_RAM[23 * 40 + i] = pcol;
        }

        // Poll Joystick Port 2 & Keyboard for Options, Help + Launch
        joy_poll(1);
        keyb_poll();

        byte c_now  = key_pressed(KSCAN_C) ? 1 : 0;
        byte v_now  = key_pressed(KSCAN_V) ? 1 : 0;
        byte h_now  = key_pressed(KSCAN_H) ? 1 : 0;
        byte go_now = (joyb[1] || key_pressed(KSCAN_SPACE) || key_pressed(KSCAN_RETURN)) ? 1 : 0;

        if (c_now && !prev_c) {
            heli_ctrl_mode ^= 1;
            draw_options_status(is_ntsc);
        }
        if (v_now && !prev_v) {
            audio_muted ^= 1;
            draw_options_status(is_ntsc);
        }
        if (h_now && !prev_h) {
            help_screen_show(is_ntsc, 0);
            draw_title_screen(is_ntsc);
            wait_rel = 1;
            prev_h = 1;
            continue;
        }
        prev_c = c_now;
        prev_v = v_now;
        prev_h = h_now;

        if (wait_rel) {
            if (!go_now) wait_rel = 0;
        } else if (go_now) {
            break;
        }
    }

    // Brief debounce & restore clean VIC sprite config for live 3D playfield
    for (byte d = 0; d < 8; d++) {
        vic_waitFrame();
        audio_tick_title();
    }

    vic.spr_enable   = 0x00;
    vic.spr_expand_x = 0x00;
    vic.spr_expand_y = 0x00;
    vic.spr_multi    = 0x02;
    vic.spr_mcolor0  = VCOL_WHITE;
    vic.spr_mcolor1  = VCOL_BLACK;
    vic.spr_color[0] = VCOL_BLACK;
    vic.spr_color[1] = VCOL_LT_RED;
    vic.spr_color[2] = VCOL_DARK_GREY;

    raster_resume_split();
}

#pragma code(code)
#pragma data(data)
