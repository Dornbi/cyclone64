#include "memmap.h"
#include "world.h"
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

static void hud_put_u16_3(byte row, byte col, unsigned int val, byte color) {
    unsigned int off = (unsigned int)row * 40 + col;
    byte *scr = HUD_SCR_BASE + off;
    byte *colr = HUD_COL_BASE + off;
    byte d2 = 0, d1 = 0;
    while (val >= 100) { val -= 100; d2++; }
    while (val >= 10)  { val -= 10;  d1++; }
    scr[0] = (byte)('0' + d2); colr[0] = color;
    scr[1] = (byte)('0' + d1); colr[1] = color;
    scr[2] = (byte)('0' + val); colr[2] = color;
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
    hud_draw_bar(0, 5, 8, 28, VCOL_LT_GREEN);
    hud_puts(0, 15, "SPD", VCOL_CYAN);
    hud_draw_bar(0, 19, 8, 36, VCOL_YELLOW);
    hud_puts(0, 29, "CRATE", VCOL_WHITE);
    for (byte i = 0; i < 5; i++) {
        HUD_SCR_BASE[35 + i] = HUD_CHAR_CRATE;
        HUD_COL_BASE[35 + i] = VCOL_YELLOW;
    }

    // Row 1: FUEL bar, WIND bar, LIVES icons
    hud_puts(1, 1,  "FUL", VCOL_CYAN);
    hud_draw_bar(1, 5, 8, 56, VCOL_GREEN);
    hud_puts(1, 15, "WND", VCOL_CYAN);
    hud_draw_bar(1, 19, 8, 18, VCOL_LT_RED);
    hud_puts(1, 29, "LIVES", VCOL_WHITE);
    for (byte i = 0; i < 3; i++) {
        HUD_SCR_BASE[40 + 35 + i] = HUD_CHAR_HELI;
        HUD_COL_BASE[40 + 35 + i] = VCOL_CYAN;
    }

    // Row 2: VIEW direction + current Island name
    hud_puts(2, 1,  "VIEW:", VCOL_LT_GREY);
    hud_puts(2, 6,  "NORTH", VCOL_YELLOW);
    hud_puts(2, 13, "ZONE:", VCOL_LT_GREY);
    hud_puts(2, 18, "BASE ISLAND       ", VCOL_LT_GREEN);

    // Row 3: TIME, Coordinates & Controls hint
    hud_puts(3, 1,  "TIME 05:00", VCOL_WHITE);
    hud_puts(3, 13, "POS:", VCOL_LT_GREY);
    hud_puts(3, 20, "/", VCOL_DARK_GREY);
    hud_puts(3, 26, "V:VIEW B:PROF", VCOL_MED_GREY);
}

void hud_update_demo(byte south_view, byte island_id, int wx, int wy, byte prof_on) {
    hud_puts(2, 6, south_view ? "SOUTH" : "NORTH", south_view ? VCOL_ORANGE : VCOL_YELLOW);

    // Display current island name (padded to 18 chars)
    const char *name = (island_id < NUM_ISLANDS) ? ISLANDS[island_id].name : "OPEN SEA";
    byte *scr = HUD_SCR_BASE + 2 * 40 + 18;
    byte *col = HUD_COL_BASE + 2 * 40 + 18;
    byte text_col = (island_id < NUM_ISLANDS) ? VCOL_LT_GREEN : VCOL_LT_BLUE;
    byte i = 0;
    while (name[i] && i < 18) {
        scr[i] = ascii_to_hud(name[i]);
        col[i] = text_col;
        i++;
    }
    while (i < 18) {
        scr[i] = 32;
        col[i] = VCOL_DARK_GREY;
        i++;
    }

    hud_put_u16_3(3, 17, (unsigned int)wx, VCOL_WHITE);
    hud_put_u16_3(3, 21, (unsigned int)wy, VCOL_WHITE);
    hud_puts(3, 35, prof_on ? "PROF!" : "PROF ", prof_on ? VCOL_RED : VCOL_MED_GREY);
}
