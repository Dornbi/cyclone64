#ifndef CYCLONE_HUD_H
#define CYCLONE_HUD_H

#include <c64/types.h>

// Initialize the 4-row Hi-Res Cockpit Instrument Panel ($CB20-$CBBF)
void hud_init(void);

// Draw a smooth 8-step horizontal bar gauge (width chars, val 0..width*8)
void hud_draw_bar(byte row, byte col, byte width, byte val, byte color);

// Update dynamic HUD indicators (Phase 1: view direction, active island, coords & sample gauges)
void hud_update_demo(byte south_view, byte island_id, int wx, int wy, byte prof_on);

#endif
