#include "memmap.h"
#include "world.h"
#include "helicopter.h"
#include "entities.h"
#include "hud.h"
#include <c64/vic.h>
#include <string.h>

#define HUD_CHAR_BAR0   64
#define HUD_CHAR_CRATE  73
#define HUD_CHAR_EMPTY  74
#define HUD_CHAR_HELI   75

byte hud_ascii_to_code(char ch) {
    if (ch >= 'A' && ch <= 'Z') return (byte)(ch - 'A' + 1);
    if (ch >= 'a' && ch <= 'z') return (byte)(ch - 'a' + 1);
    if (ch >= '0' && ch <= '9') return (byte)ch;
    if (ch == ':') return 58;
    if (ch == '-') return 45;
    if (ch == '/') return 47;
    if (ch == '.') return 46;
    if (ch == '[') return 27;
    if (ch == ']') return 29;
    if (ch == '!') return 33;
    if (ch == '#') return 72;
    return 32;
}

static void hud_puts(byte row, byte col, const char *s, byte color) {
    unsigned int off = (unsigned int)row * 40 + col;
    byte *scr = HUD_SCR_BASE + off;
    byte *colr = HUD_COL_BASE + off;
    byte i = 0;
    while (s[i]) {
        scr[i]  = hud_ascii_to_code(s[i]);
        colr[i] = color;
        i++;
    }
}

static void hud_put_time(byte row, byte col, unsigned int sec, byte color) {
    byte mins = 0;
    while (sec >= 60) { sec -= 60; mins++; }
    byte s10 = 0;
    while (sec >= 10) { sec -= 10; s10++; }

    unsigned int off = (unsigned int)row * 40 + col;
    byte *scr = HUD_SCR_BASE + off;
    byte *colr = HUD_COL_BASE + off;
    scr[0] = (byte)('0' + (mins / 10)); colr[0] = color;
    scr[1] = (byte)('0' + (mins % 10)); colr[1] = color;
    scr[2] = 58;                        colr[2] = color; // ':'
    scr[3] = (byte)('0' + s10);         colr[3] = color;
    scr[4] = (byte)('0' + sec);         colr[4] = color;
}

static void hud_put_score(byte row, byte col, unsigned int score10, byte color) {
    unsigned int off = (unsigned int)row * 40 + col;
    byte *scr = HUD_SCR_BASE + off;
    byte *colr = HUD_COL_BASE + off;
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

void hud_draw_bar(byte row, byte col, byte width, byte val, byte color) {
    unsigned int off = (unsigned int)row * 40 + col;
    byte *scr = HUD_SCR_BASE + off;
    byte *colr = HUD_COL_BASE + off;
    for (byte i = 0; i < width; i++) {
        byte fill;
        if (val >= 8) {
            fill = 8;
            val -= 8;
        } else {
            fill = val;
            val = 0;
        }
        scr[i]  = (byte)(HUD_CHAR_BAR0 + fill);
        colr[i] = fill ? color : VCOL_DARK_GREY;
    }
}

void hud_init(void) {
    memset(SCREEN_HUD, 32, 1000);
    memset(HUD_COL_BASE, VCOL_LT_GREY, 160);

    // Row 0: ALT bar, SPD bar, CRATE progress icons
    hud_puts(0, 1,  "ALT", VCOL_CYAN);
    hud_draw_bar(0, 5, 8, 8, VCOL_LT_GREEN);
    hud_puts(0, 15, "SPD", VCOL_CYAN);
    hud_draw_bar(0, 19, 8, 0, VCOL_YELLOW);
    hud_puts(0, 29, "CRATE", VCOL_WHITE);

    // Row 1: FUEL bar, WIND bar, LIVES icons
    hud_puts(1, 1,  "FUL", VCOL_CYAN);
    hud_draw_bar(1, 5, 8, 64, VCOL_GREEN);
    hud_puts(1, 15, "WND", VCOL_CYAN);
    hud_draw_bar(1, 19, 8, 8, VCOL_LT_RED);
    hud_puts(1, 29, "LIVES", VCOL_WHITE);

    // Row 2: VIEW direction + current Island Zone + Compass Heading
    hud_puts(2, 1,  "VIEW:", VCOL_LT_GREY);
    hud_puts(2, 6,  "NORTH", VCOL_YELLOW);
    hud_puts(2, 13, "ZONE:", VCOL_LT_GREY);
    hud_puts(2, 34, "H:", VCOL_LT_GREY);

    // Row 3: TIME countdown, Status/Warning Banner, 6-digit SCORE
    hud_puts(3, 1,  "TIME ", VCOL_LT_GREY);
    hud_put_time(3, 6, 480, VCOL_WHITE);
    hud_puts(3, 29, "SC:", VCOL_LT_GREY);
    hud_put_score(3, 32, 0, VCOL_YELLOW);
}

void hud_update_game(byte south_view, byte island_id,
                     byte alt, byte ground_alt, byte speed,
                     byte fuel, byte wind_force,
                     byte crates_got, byte lives,
                     unsigned int time_sec, unsigned int score10,
                     byte state, byte surface,
                     const char *hdg_str, byte flash_bit) {
    // 1. Live ALT bar (0..64)
    byte alt_col = VCOL_LT_GREEN;
    if (state == HELI_STATE_FLYING && alt <= (byte)(ground_alt + 6)) {
        alt_col = VCOL_LT_RED;
    } else if (state == HELI_STATE_LANDED) {
        alt_col = VCOL_CYAN;
    }
    hud_draw_bar(0, 5, 8, alt, alt_col);

    // 2. Live SPD bar (0..48 -> 0..64)
    byte spd_bar = (byte)(speed + (speed >> 2));
    if (spd_bar > 64) spd_bar = 64;
    hud_draw_bar(0, 19, 8, spd_bar, (speed > 34) ? VCOL_ORANGE : VCOL_YELLOW);

    // 3. Live FUEL bar (0..255 -> 0..64, flashes red when critically low!)
    byte fuel_bar = (byte)(fuel >> 2);
    byte fuel_col = (fuel < 64) ? (flash_bit ? VCOL_RED : VCOL_ORANGE)
                  : ((fuel < 128) ? VCOL_YELLOW : VCOL_GREEN);
    hud_draw_bar(1, 5, 8, fuel_bar, fuel_col);

    // 4. Live WIND DANGER bar (0..16 -> 0..64)
    byte wnd_bar = (byte)(wind_force << 2);
    byte wnd_col = (wind_force >= 11) ? (flash_bit ? VCOL_RED : VCOL_YELLOW)
                 : ((wind_force >= 6) ? VCOL_ORANGE : VCOL_LT_RED);
    hud_draw_bar(1, 19, 8, wnd_bar, wnd_col);

    // 5. 5 Crate icons (collected = bright green filled, remaining = yellow outline)
    for (byte i = 0; i < 5; i++) {
        if (i < crates_got) {
            HUD_SCR_BASE[35 + i] = HUD_CHAR_CRATE;
            HUD_COL_BASE[35 + i] = VCOL_LT_GREEN;
        } else {
            HUD_SCR_BASE[35 + i] = HUD_CHAR_EMPTY;
            HUD_COL_BASE[35 + i] = VCOL_YELLOW;
        }
    }

    // 6. Lives icons
    for (byte i = 0; i < 3; i++) {
        HUD_SCR_BASE[40 + 35 + i] = (i < lives) ? HUD_CHAR_HELI : 32;
        HUD_COL_BASE[40 + 35 + i] = VCOL_CYAN;
    }

    // 7. View direction & Compass Heading
    hud_puts(2, 6, south_view ? "SOUTH" : "NORTH", south_view ? VCOL_ORANGE : VCOL_YELLOW);
    hud_puts(2, 36, hdg_str, VCOL_WHITE);

    // 8. Current Island Zone (15 chars padded)
    const char *name = (island_id < NUM_ISLANDS) ? ISLANDS[island_id].name : "OPEN SEA";
    byte *scr = HUD_SCR_BASE + 2 * 40 + 18;
    byte *col = HUD_COL_BASE + 2 * 40 + 18;
    byte text_col = (island_id < NUM_ISLANDS) ? VCOL_LT_GREEN : VCOL_LT_BLUE;
    byte i = 0;
    while (name[i] && i < 15) {
        scr[i] = hud_ascii_to_code(name[i]);
        col[i] = text_col;
        i++;
    }
    while (i < 15) {
        scr[i] = 32;
        col[i] = VCOL_DARK_GREY;
        i++;
    }

    // 9. Countdown Clock & 6-Digit Score
    byte time_col = (time_sec < 60) ? (flash_bit ? VCOL_RED : VCOL_YELLOW) : VCOL_WHITE;
    hud_put_time(3, 6, time_sec, time_col);
    hud_put_score(3, 32, score10, VCOL_YELLOW);

    // 10. Status / Warning Banner (14 chars at col 13..26)
    const char *msg = "AIRBORNE      ";
    byte mcol = VCOL_CYAN;
    if (game_state == GAME_WIN) {
        msg = "MISSION CLEAR!";
        mcol = flash_bit ? VCOL_LT_GREEN : VCOL_YELLOW;
    } else if (game_state == GAME_OVER_LIVES) {
        msg = "GAME OVER!    ";
        mcol = flash_bit ? VCOL_RED : VCOL_ORANGE;
    } else if (game_state == GAME_OVER_TIME) {
        msg = "! NO TIME !   ";
        mcol = flash_bit ? VCOL_RED : VCOL_YELLOW;
    } else if (banner_timer > 0) {
        msg  = banner_text;
        mcol = banner_color;
    } else if (state == HELI_STATE_CRASHING) {
        msg  = "! CRASHED !   ";
        mcol = VCOL_RED;
    } else if (state == HELI_STATE_LANDED) {
        msg  = (surface == SURF_HELIPAD) ? "[PAD LANDED]  " : "[GROUNDED]    ";
        mcol = VCOL_LT_GREEN;
    } else if (wind_force >= 11) {
        msg  = "!CYCLONE NEAR!";
        mcol = flash_bit ? VCOL_RED : VCOL_YELLOW;
    } else if (alt <= (byte)(ground_alt + 6)) {
        msg  = "! LOW ALT !   ";
        mcol = VCOL_LT_RED;
    } else if (crates_got >= NUM_CRATES) {
        msg  = "RETURN TO BASE";
        mcol = flash_bit ? VCOL_LT_GREEN : VCOL_WHITE;
    } else if (surface == SURF_HELIPAD) {
        msg  = "OVER HELIPAD  ";
        mcol = VCOL_YELLOW;
    }
    hud_puts(3, 13, msg, mcol);
}
