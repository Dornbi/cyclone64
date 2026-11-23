#include "memmap.h"
#include "world.h"
#include "raster.h"
#include "scroller.h"
#include "helicopter.h"
#include "hud.h"

#include <c64/vic.h>
#include <c64/joystick.h>
#include <c64/keyboard.h>

// Phase 2: Helicopter Flight, 16-Angle Layered Sprites, Altitude-Offset Shadow & 3D Collision
// Controls:
//   Joystick Port 2 or W/A/S/D : Rotate + Throttle (Classic) or Direct 8-Way (Arcade)
//   Hold Fire + Up/Down OR Q/Z : Climb / Descend altitude (0..64 px)
//   Quick-Tap Fire OR V/Return : Instant North <-> South 180-degree 3D perspective flip
//   C                          : Toggle Classic Flight vs Direct 8-Way Arcade controls
//   B                          : Toggle $D020 border raster-time profiling bars
//   1..7                       : Quick-warp above any of the 7 helipad islands

static const byte WARP_ISLANDS[7] = { 5, 2, 4, 6, 7, 8, 13 };

int main(void) {
    vic.color_border = VCOL_BLACK;
    vic.color_back   = VCOL_BLACK;

    // 1. Setup Bank 3 & disable CIA interrupts (safe for MMAP_RAM sprite unpacking!)
    raster_setup_bank3();

    // 2. Unpack LZO charsets ($E000-$F7FF) AND 4 KB sprite bank ($D000-$DFFF under I/O!)
    world_unpack_assets();

    // 3. Start the 5-stage Playfield <-> Cockpit HUD raster IRQ chain
    raster_start_irqs();

    // 4. Draw Cockpit HUD and spawn rescue helicopter on Base Island's western helipad!
    hud_init();
    heli_init();

    byte prev_fire   = 0;
    byte fire_frames = 0;
    byte fire_used_alt = 0;
    byte prev_v      = 0;
    byte prev_c      = 0;
    byte prev_b      = 0;
    byte hud_tick    = 0;

#ifdef PHASE2_AUTOTEST
    unsigned int auto_frame = 0;
#endif

    for (;;) {
        joy_poll(1);
        keyb_poll();

        sbyte jx = joyx[1];
        sbyte jy = joyy[1];
        if (key_pressed(KSCAN_A)) jx = -1;
        if (key_pressed(KSCAN_D)) jx =  1;
        if (key_pressed(KSCAN_W)) jy = -1;
        if (key_pressed(KSCAN_S)) jy =  1;

        sbyte direct_alt = 0;
        if (key_pressed(KSCAN_Q)) direct_alt =  1;
        if (key_pressed(KSCAN_Z)) direct_alt = -1;

        byte fire_now = joyb[1] ? 1 : 0;
        byte v_now    = (key_pressed(KSCAN_V) || key_pressed(KSCAN_RETURN)) ? 1 : 0;
        byte c_now    = key_pressed(KSCAN_C) ? 1 : 0;
        byte b_now    = key_pressed(KSCAN_B) ? 1 : 0;

#ifdef PHASE2_AUTOTEST
        // Automated flight test: climb off Base helipad, fly NE over hills & buildings!
        auto_frame++;
        if (auto_frame <= 32) {
            direct_alt = 1; // Climb from alt=8 up to alt=40 so shadow separates dramatically!
        }
        if (auto_frame >= 16 && auto_frame <= 48) {
            jy = -1;        // Forward throttle!
        }
        if (auto_frame >= 24 && auto_frame <= 34) {
            jx = -1;        // Bank gently toward NNE
        }
#endif

        // Detect quick-tap Fire (without using altitude) to flip North <-> South view
        byte do_flip = 0;
        if (fire_now) {
            if (fire_frames < 250) fire_frames++;
            if (jy != 0) fire_used_alt = 1;
        } else {
            if (prev_fire && !fire_used_alt && fire_frames < 14) {
                do_flip = 1;
            }
            fire_frames   = 0;
            fire_used_alt = 0;
        }
        if (v_now && !prev_v) do_flip = 1;

        if (c_now && !prev_c) {
            heli_ctrl_mode ^= 1;
            hud_tick = 7;
        }
        if (b_now && !prev_b) {
            r_border_prof ^= 1;
            if (!r_border_prof) vic.color_border = VCOL_BLACK;
        }

        // Quick-warp keys 1..7 for testing all 7 helipad islands
        for (byte k = 0; k < 7; k++) {
            static const KeyScanCode num_keys[7] = {
                KSCAN_1, KSCAN_2, KSCAN_3, KSCAN_4, KSCAN_5, KSCAN_6, KSCAN_7
            };
            if (key_pressed(num_keys[k])) {
                heli_warp_to_island(WARP_ISLANDS[k]);
                hud_tick = 7;
                break;
            }
        }

        if (do_flip) {
            scroller_toggle_view();
            heli_sync_sprites(sc_south_view);
            hud_tick = 7;
        } else {
            sbyte cam_dpx = 0;
            sbyte cam_dpy = 0;
            heli_update(jx, jy, fire_now, direct_alt, sc_south_view, &cam_dpx, &cam_dpy);
            scroller_move(cam_dpx, cam_dpy);
            heli_sync_sprites(sc_south_view);
        }

        prev_fire = fire_now;
        prev_v    = v_now;
        prev_c    = c_now;
        prev_b    = b_now;

        scroller_animate_water();

        hud_tick++;
        if ((hud_tick & 3) == 0) {
            int hwx = heli_px >> 3;
            int hwy = heli_py >> 3;
            byte isl_id = world_find_island(hwx, hwy);
            hud_update_flight(sc_south_view, isl_id,
                              heli_alt, heli_ground_alt, heli_speed,
                              heli_lives, heli_state, heli_surface,
                              heli_heading_str(), heli_ctrl_mode);
        }
    }

    return 0;
}
