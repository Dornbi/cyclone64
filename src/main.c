#include "memmap.h"
#include "world.h"
#include "raster.h"
#include "scroller.h"
#include "helicopter.h"
#include "entities.h"
#include "hud.h"

#include <c64/vic.h>
#include <c64/joystick.h>
#include <c64/keyboard.h>

// Auto-detect PAL (50Hz, 312 lines) vs NTSC (60Hz, 263 lines) via VIC-II max rasterline
static byte detect_ntsc(void) {
    // Wait for raster bit 8 ($D011 bit 7) to clear, then track max line when bit 8 is set
    while (vic.ctrl1 & 0x80) {}
    while (!(vic.ctrl1 & 0x80)) {}
    byte max_hi = 0;
    while (vic.ctrl1 & 0x80) {
        byte r = vic.raster;
        if (r > max_hi) max_hi = r;
    }
    // PAL reaches line 311 (max_hi ~ 55); NTSC reaches line 262 (max_hi ~ 6)
    return (max_hi < 20) ? 1 : 0;
}

static const byte WARP_ISLANDS[7] = { 5, 2, 4, 6, 7, 8, 13 };

int main(void) {
    vic.color_border = VCOL_BLACK;
    vic.color_back   = VCOL_BLACK;

    byte is_ntsc = detect_ntsc();

    // 1. Setup Bank 3 & disable CIA interrupts (safe for MMAP_RAM sprite unpacking!)
    raster_setup_bank3();

    // 2. Unpack LZO charsets ($E000-$F7FF) AND 4 KB sprite bank ($D000-$DFFF under I/O!)
    world_unpack_assets();

    // 3. Start the 5-stage Playfield <-> Cockpit HUD raster IRQ chain
    raster_start_irqs();

    // 4. Initialize HUD, Rescue Mission (5 crates + 10 survivors), and Helicopter on Base pad
    byte mission_seed = 1;
    hud_init();
    entities_init(mission_seed);
    heli_init();

    byte prev_fire     = 0;
    byte fire_frames   = 0;
    byte fire_used_alt = 0;
    byte prev_v        = 0;
    byte prev_c        = 0;
    byte hud_tick      = 0;

#ifdef PHASE3_AUTOTEST
    // Automated verification: position right over Crate #0 on Giants Gateway and winch it aboard!
    heli_px = entities[0].px;
    heli_py = entities[0].py;
    heli_ground_alt = entities[0].ground_alt;
    heli_alt = (byte)(entities[0].ground_alt + 18);
    heli_state = HELI_STATE_FLYING;
    heli_speed = 0;
    scroller_init(heli_px >> 3, (heli_py >> 3) - 2);
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
        byte sp_now   = key_pressed(KSCAN_SPACE) ? 1 : 0;

        // Restart cleanly on Fire or Space after Mission Complete / Game Over
        if (game_state != GAME_PLAYING) {
            if ((fire_now && !prev_fire) || sp_now) {
                mission_seed++;
                hud_init();
                entities_init(mission_seed);
                heli_init();
            }
            prev_fire = fire_now;
            raster_wait_frame();
            hud_tick++;
            if ((hud_tick & 7) == 0) {
                hud_update_game(sc_south_view, world_find_island(heli_px >> 3, heli_py >> 3),
                                heli_alt, heli_ground_alt, heli_speed,
                                game_fuel, 2, crates_collected, heli_lives,
                                game_time_s, game_score10, heli_state, heli_surface,
                                heli_heading_str(), (hud_tick >> 4) & 1);
            }
            continue;
        }

        // Quick-tap Fire (without using altitude) flips North <-> South 3D view
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
            entities_set_banner(
                (heli_ctrl_mode == CTRL_CLASSIC) ? "CTRL: CLASSIC " : "CTRL: ARCADE  ",
                VCOL_WHITE, 50
            );
        }

        // Quick-warp keys 1..7 for testing helipad islands
        for (byte k = 0; k < 7; k++) {
            static const KeyScanCode num_keys[7] = {
                KSCAN_1, KSCAN_2, KSCAN_3, KSCAN_4, KSCAN_5, KSCAN_6, KSCAN_7
            };
            if (key_pressed(num_keys[k])) {
                heli_warp_to_island(WARP_ISLANDS[k]);
                break;
            }
        }

        if (do_flip) {
            scroller_toggle_view();
        } else {
            sbyte cam_dpx = 0;
            sbyte cam_dpy = 0;
            heli_update(jx, jy, fire_now, direct_alt, sc_south_view,
                        (game_fuel == 0) ? 1 : 0, 0, 0, &cam_dpx, &cam_dpy);
            scroller_move(cam_dpx, cam_dpy);
        }

        // Update & combine all hardware sprite layers (Heli 0..2 + Winch 3 + Crates/Survivors 6..7)
        byte smask = heli_sync_sprites(sc_south_view);
        smask |= entities_update_and_draw(sc_south_view, is_ntsc);
        r_spr_enable = smask;

        prev_fire = fire_now;
        prev_v    = v_now;
        prev_c    = c_now;

        scroller_animate_water();

        hud_tick++;
        if ((hud_tick & 3) == 0) {
            byte isl_id = world_find_island(heli_px >> 3, heli_py >> 3);
            hud_update_game(sc_south_view, isl_id,
                            heli_alt, heli_ground_alt, heli_speed,
                            game_fuel, 2, crates_collected, heli_lives,
                            game_time_s, game_score10, heli_state, heli_surface,
                            heli_heading_str(), (hud_tick >> 4) & 1);
        }
    }

    return 0;
}
