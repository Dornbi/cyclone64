#ifndef CYCLONE_HUD_H
#define CYCLONE_HUD_H

#include <c64/types.h>

// Convert ASCII character to HUD/Title/Map screen code
byte hud_ascii_to_code(char ch);

// Initialize the 4-row Hi-Res Cockpit Instrument Panel
void hud_init(void);

// Draw a smooth 8-step horizontal bar gauge (width chars, val 0..width*8)
void hud_draw_bar(byte row, byte col, byte width, byte val, byte color);

// Full live gameplay Cockpit HUD update (ALT, SPD, FUEL, WIND, CRATES, LIVES, TIME, SCORE, STATUS)
void hud_update_game(byte south_view, byte island_id,
                     byte alt, byte ground_alt, byte speed,
                     byte fuel, byte wind_force,
                     byte crates_got, byte lives,
                     unsigned int time_sec, unsigned int score10,
                     byte state, byte surface,
                     const char *hdg_str, byte flash_bit);

#endif
