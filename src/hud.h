#ifndef CYCLONE_HUD_H
#define CYCLONE_HUD_H

#include <c64/types.h>

// Initialize the 4-row Hi-Res Cockpit Instrument Panel ($CB20-$CBBF)
void hud_init(void);

// Draw a smooth 8-step horizontal bar gauge (width chars, val 0..width*8)
void hud_draw_bar(byte row, byte col, byte width, byte val, byte color);

// Live flight instrument update (Phase 2: ALT, SPD, LIVES, HDG, VIEW, ZONE, STATUS)
void hud_update_flight(byte south_view, byte island_id,
                       byte alt, byte ground_alt, byte speed,
                       byte lives, byte state, byte surface,
                       const char *hdg_str, byte ctrl_mode);

#endif
