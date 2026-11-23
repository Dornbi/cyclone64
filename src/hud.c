#include "memmap.h"
#include "world.h"
#include "helicopter.h"
#include "hud.h"
#include <c64/vic.h>
#include <string.h>

// Custom HUD font screen codes (matches tools/build_assets.py)
#define HUD_CHAR_BAR0   64
#define HUD_CHAR_CRATE  73
#define HUD_CHAR_EMPTY  74
#define HUD_CHAR_HELI   75

static byte ascii_to_hud(char ch) {
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
    return 32; // space
}

static void hud_puts(byte row, byte col, const char *s, byte color) {
    unsigned int off = (unsigned int)row * 40 + col;
    byte *scr = HUD_SCR_BASE + off;
    byte *colr = HUD_COL_BASE + off;
    byte i = 0;
    while (s[i]) {
        scr[i]  = ascii_to_hud(s[i]);
        colr[i] = color;
        i++;
    }
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

    // Row 0: ALT bar, SPD bar, CRATES icons
    hud_puts(0, 1,  "ALT", VCOL_CYAN);
    hud_draw_bar(0, 5, 8, 8, VCOL_LT_GREEN);
    hud_puts(0, 15, "SPD", VCOL_CYAN);
    hud_draw_bar(0, 19, 8, 0, VCOL_YELLOW);
    hud_puts(0, 29, "CRATE", VCOL_WHITE);
    for (byte i = 0; i < 5; i++) {
        HUD_SCR_BASE[35 + i] = HUD_CHAR_CRATE;
        HUD_COL_BASE[35 + i] = VCOL_YELLOW;
    }

    // Row 1: FUEL bar, WIND bar, LIVES icons
    hud_puts(1, 1,  "FUL", VCOL_CYAN);
    hud_draw_bar(1, 5, 8, 64, VCOL_GREEN);
    hud_puts(1, 15, "WND", VCOL_CYAN);
    hud_draw_bar(1, 19, 8, 12, VCOL_LT_RED);
    hud_puts(1, 29, "LIVES", VCOL_WHITE);
    for (byte i = 0; i < 3; i++) {
        HUD_SCR_BASE[40 + 35 + i] = HUD_CHAR_HELI;
        HUD_COL_BASE[40 + 35 + i] = VCOL_CYAN;
    }

    // Row 2: VIEW direction + current Island name + HDG
    hud_puts(2, 1,  "VIEW:", VCOL_LT_GREY);
    hud_puts(2, 6,  "NORTH", VCOL_YELLOW);
    hud_puts(2, 13, "ZONE:", VCOL_LT_GREY);
    hud_puts(2, 18, "BASE ISLAND     ", VCOL_LT_GREEN);
    hud_puts(2, 34, "H:", VCOL_LT_GREY);
    hud_puts(2, 36, "E  ", VCOL_WHITE);

    // Row 3: TIME, Flight Status annunciator & Control mode
    hud_puts(3, 1,  "TIME 05:00", VCOL_WHITE);
    hud_puts(3, 13, "[LANDED:PAD]  ", VCOL_LT_GREEN);
    hud_puts(3, 29, "CTRL:CLSC", VCOL_MED_GREY);
}

void hud_update_flight(byte south_view, byte island_id,
                       byte alt, byte ground_alt, byte speed,
                       byte lives, byte state, byte surface,
                       const char *hdg_str, byte ctrl_mode) {
    // 1. Live ALT bar (0..64 -> 0..64 bar pixels; flashes red/orange when very close to obstacles!)
    byte alt_col = VCOL_LT_GREEN;
    if (state == HELI_STATE_FLYING && alt <= (byte)(ground_alt + 6)) {
        alt_col = VCOL_LT_RED;
    } else if (state == HELI_STATE_LANDED) {
        alt_col = VCOL_CYAN;
    }
    hud_draw_bar(0, 5, 8, alt, alt_col);

    // 2. Live SPD bar (0..48 -> scaled 0..64 via speed + speed/3)
    byte spd_bar = (byte)(speed + (speed >> 2));
    if (spd_bar > 64) spd_bar = 64;
    hud_draw_bar(0, 19, 8, spd_bar, (speed > 34) ? VCOL_ORANGE : VCOL_YELLOW);

    // 3. Lives icons
    for (byte i = 0; i < 3; i++) {
        HUD_SCR_BASE[40 + 35 + i] = (i < lives) ? HUD_CHAR_HELI : 32;
        HUD_COL_BASE[40 + 35 + i] = VCOL_CYAN;
    }

    // 4. View direction & Compass Heading
    hud_puts(2, 6, south_view ? "SOUTH" : "NORTH", south_view ? VCOL_ORANGE : VCOL_YELLOW);
    hud_puts(2, 36, hdg_str, VCOL_WHITE);

    // 5. Current island zone (16 chars padded)
    const char *name = (island_id < NUM_ISLANDS) ? ISLANDS[island_id].name : "OPEN SEA";
    byte *scr = HUD_SCR_BASE + 2 * 40 + 18;
    byte *col = HUD_COL_BASE + 2 * 40 + 18;
    byte text_col = (island_id < NUM_ISLANDS) ? VCOL_LT_GREEN : VCOL_LT_BLUE;
    byte i = 0;
    while (name[i] && i < 15) {
        scr[i] = ascii_to_hud(name[i]);
        col[i] = text_col;
        i++;
    }
    while (i < 15) {
        scr[i] = 32;
        col[i] = VCOL_DARK_GREY;
        i++;
    }

    // 6. Status annunciator
    if (state == HELI_STATE_CRASHING) {
        hud_puts(3, 13, "! CRASHED !   ", VCOL_RED);
    } else if (state == HELI_STATE_LANDED) {
        hud_puts(3, 13, (surface == SURF_HELIPAD) ? "[PAD LANDED]  " : "[GROUNDED]    ", VCOL_LT_GREEN);
    } else if (alt <= (byte)(ground_alt + 6)) {
        hud_puts(3, 13, "! LOW ALT !   ", VCOL_LT_RED);
    } else if (surface == SURF_HELIPAD) {
        hud_puts(3, 13, "OVER HELIPAD  ", VCOL_YELLOW);
    } else {
        hud_puts(3, 13, "AIRBORNE      ", VCOL_CYAN);
    }

    hud_puts(3, 29, (ctrl_mode == CTRL_CLASSIC) ? "CTRL:CLSC" : "CTRL:ARCD", VCOL_MED_GREY);
}
