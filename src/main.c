#include "memmap.h"
#include "world.h"
#include "raster.h"
#include "scroller.h"
#include "hud.h"

#include <c64/vic.h>
#include <c64/joystick.h>
#include <c64/keyboard.h>

// Phase 1: VIC-II Bank 3 Engine, Double-Buffered 8-Way 3D Scroller & Hi-Res Cockpit HUD Split
// Controls:
//   Joystick Port 2 or WASD : Smooth 8-way 50/60 FPS scrolling across all 14 islands
//   Fire / V / Return       : Instant North <-> South 180-degree 3D perspective flip
//   B                       : Toggle $D020 border raster-time profiling bars
//   1..7                    : Quick-warp to helipad islands for rapid visual inspection

static const byte WARP_ISLANDS[7] = { 5, 2, 4, 6, 7, 8, 13 };

int main(void) {
    vic.color_border = VCOL_BLACK;
    vic.color_back   = VCOL_BLACK;

    // 1. Setup Bank 3, disable ROMs ($01=$35), and start the Playfield <-> HUD raster IRQ chain
    raster_init();

    // 2. Unpack LZO-compressed tropical North/South & HUD charsets into $E000-$F7FF
    world_unpack_assets();

    // 3. Draw the 4-row Hi-Res Cockpit HUD
    hud_init();

    // 4. Center camera directly over BASE ISLAND's iconic western helipad & buildings!
    // Base Island world origin = (306, 364), helipad at local (+16, +17) -> world (322, 381)
    scroller_init(324, 376);

    byte prev_fire = 0;
    byte prev_v    = 0;
    byte prev_b    = 0;
    byte hud_tick  = 0;
#ifdef PHASE1_AUTOTEST
    unsigned int auto_frame = 0;
    r_border_prof = 1;
#endif

    for (;;) {
        // Poll Joystick Port 2 (index 1) and Keyboard
        joy_poll(1);
        keyb_poll();

        sbyte dx = (sbyte)(joyx[1] * 2);
        sbyte dy = (sbyte)(joyy[1] * 2);

        if (key_pressed(KSCAN_A)) dx = -2;
        if (key_pressed(KSCAN_D)) dx =  2;
        if (key_pressed(KSCAN_W)) dy = -2;
        if (key_pressed(KSCAN_S)) dy =  2;

#ifdef PHASE1_AUTOTEST
        auto_frame++;
        if (auto_frame <= 28) {
            // Diagonal scroll across multiple tile boundaries + test non-zero yscroll!
            dx = 2;
            dy = 1;
        } else if (auto_frame == 29) {
            // Flip to South View and scroll back diagonally
            scroller_toggle_view();
            hud_update_demo(sc_south_view, world_find_island(scroller_center_wx(), scroller_center_wy()),
                            scroller_center_wx(), scroller_center_wy(), r_border_prof);
            continue;
        } else if (auto_frame <= 45) {
            dx = -1;
            dy = -1;
        }
#endif

        // Scroll direction tracks screen space (flip world deltas when facing South)
        if (sc_south_view) {
            dx = (sbyte)(-dx);
            dy = (sbyte)(-dy);
        }

        // Edge-triggered North <-> South view flip (Fire or V or Return)
        byte fire_now = joyb[1] ? 1 : 0;
        byte v_now    = (key_pressed(KSCAN_V) || key_pressed(KSCAN_RETURN)) ? 1 : 0;
        byte b_now    = key_pressed(KSCAN_B) ? 1 : 0;

        if ((fire_now && !prev_fire) || (v_now && !prev_v)) {
            scroller_toggle_view();
            hud_tick = 15; // Force immediate HUD refresh
        } else {
            // Smooth 50/60 FPS 8-way hardware scroll (waits internally for scanline 200)
            scroller_move(dx, dy);
        }

        if (b_now && !prev_b) {
            r_border_prof ^= 1;
            if (!r_border_prof) vic.color_border = VCOL_BLACK;
        }

        // Quick-warp keys 1..7 to inspect helipad islands across the archipelago
        for (byte k = 0; k < 7; k++) {
            static const KeyScanCode num_keys[7] = {
                KSCAN_1, KSCAN_2, KSCAN_3, KSCAN_4, KSCAN_5, KSCAN_6, KSCAN_7
            };
            if (key_pressed(num_keys[k])) {
                const IslandDesc *isl = &ISLANDS[WARP_ISLANDS[k]];
                scroller_init(isl->world_x + (isl->width >> 1), isl->world_y + (isl->height >> 1));
                hud_tick = 15;
                break;
            }
        }

        prev_fire = fire_now;
        prev_v    = v_now;
        prev_b    = b_now;

        // Animate ocean shimmer & refresh HUD zone/coords periodically
        scroller_animate_water();

        hud_tick++;
        if ((hud_tick & 7) == 0) {
            int cx = scroller_center_wx();
            int cy = scroller_center_wy();
            byte isl_id = world_find_island(cx, cy);
            hud_update_demo(sc_south_view, isl_id, cx, cy, r_border_prof);
        }
    }

    return 0;
}
