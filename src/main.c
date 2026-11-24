#include "memmap.h"
#include "world.h"
#include "raster.h"
#include "scroller.h"
#include "helicopter.h"
#include "entities.h"
#include "cyclone.h"
#include "radar_map.h"
#include "hud.h"
#include "audio.h"
#include "title.h"

#include <c64/vic.h>
#include <c64/joystick.h>
#include <c64/keyboard.h>
#ifdef TEST_SCENARIO
#include <string.h>
#endif

// Auto-detect PAL (50Hz, 312 lines) vs NTSC (60Hz, 263 lines) via VIC-II max rasterline
static byte detect_ntsc(void) {
    while (vic.ctrl1 & 0x80) {}
    while (!(vic.ctrl1 & 0x80)) {}
    byte max_hi = 0;
    while (vic.ctrl1 & 0x80) {
        byte r = vic.raster;
        if (r > max_hi) max_hi = r;
    }
    return (max_hi < 20) ? 1 : 0;
}

static const byte WARP_ISLANDS[7] = { 5, 2, 4, 6, 7, 8, 13 };

int main(void) {
    vic.color_border = VCOL_BLACK;
    vic.color_back   = VCOL_BLACK;

    byte is_ntsc = detect_ntsc();

    // 1. Setup Bank 3 & disable CIA interrupts (safe for MMAP_RAM sprite unpacking!)
    raster_setup_bank3();

    // 2. Unpack LZO charsets ($E000/$F000) AND 4 KB sprite bank ($D000-$DFFF under I/O!)
    world_unpack_assets();

    // 3. Detect SID 6581/8580 & initialize 3-voice filtered SID audio engine
    audio_init(is_ntsc);

    // 4. Start the 5-stage Playfield <-> Cockpit HUD raster IRQ chain
    raster_start_irqs();

    byte mission_seed = 1;

#ifndef SKIP_TITLE
    title_screen_show(is_ntsc);
#endif

    // 5. Initialize HUD, Rescue Mission, Cyclone Storm & Helicopter on Base Island pad
    hud_init();
    entities_init(mission_seed);
    cyclone_init(mission_seed);
    heli_init();
    audio_set_mode(AUDIO_MODE_GAME);

    byte prev_fire     = 0;
    byte fire_frames   = 0;
    byte fire_used_alt = 1;
    byte prev_ret      = 0;
    byte prev_c        = 0;
    byte prev_m        = 1;
    byte prev_v        = 0;
    byte prev_h        = 0;
    byte hud_tick      = 0;

#ifdef TEST_SCENARIO
    // Automated test harness hooks (compiled ONLY when running tools/run_tests.py!)
    #include "test_hooks.inc"
#endif

    for (;;) {
        joy_poll(1);
        keyb_poll();

        // Direction: Joystick Port 2 OR I / J / K / L
        sbyte jx = joyx[1];
        sbyte jy = joyy[1];
        if (key_pressed(KSCAN_J)) jx = -1;
        if (key_pressed(KSCAN_L)) jx =  1;
        if (key_pressed(KSCAN_I)) jy = -1;
        if (key_pressed(KSCAN_K)) jy =  1;

        // Altitude: Fire + Up/Down OR W (up) / S (down)
        sbyte direct_alt = 0;
        if (key_pressed(KSCAN_W)) direct_alt =  1;
        if (key_pressed(KSCAN_S)) direct_alt = -1;

        byte fire_now = joyb[1] ? 1 : 0;
        byte ret_now  = key_pressed(KSCAN_RETURN) ? 1 : 0;
        byte c_now    = key_pressed(KSCAN_C) ? 1 : 0;
        byte m_now    = (key_pressed(KSCAN_M) || key_pressed(KSCAN_SPACE)) ? 1 : 0;
        byte v_now    = key_pressed(KSCAN_V) ? 1 : 0;
        byte h_now    = key_pressed(KSCAN_H) ? 1 : 0;

        // V toggles SID audio mute cleanly
        if (v_now && !prev_v) {
            audio_muted ^= 1;
            entities_set_banner(audio_muted ? "AUDIO: MUTED  " : "AUDIO: ON     ", VCOL_CYAN, 45);
        }
        prev_v = v_now;

        // H opens full-screen Help overlay at any point during flight
        if (h_now && !prev_h) {
            help_screen_show(is_ntsc, 1);
            prev_h = 1;
            hud_tick = 3;
            continue;
        }
        prev_h = h_now;

        // Record high score & handle Mission Complete / Game Over restart or return to Title
        if (game_state != GAME_PLAYING) {
            title_record_score(game_score10);
            byte to_title = (key_pressed(KSCAN_T) || key_pressed(KSCAN_STOP)) ? 1 : 0;
            if (to_title || (fire_now && !prev_fire) || (m_now && !prev_m)) {
                mission_seed++;
                if (to_title) {
                    title_screen_show(is_ntsc);
                }
                hud_init();
                entities_init(mission_seed);
                cyclone_init(mission_seed);
                heli_init();
                audio_set_mode(AUDIO_MODE_GAME);
                prev_fire = 1;
                prev_m    = 1;
                continue;
            }
            prev_fire = fire_now;
            prev_m    = m_now;
            raster_wait_frame();
            audio_tick_game(heli_state, 0, 2, last_sfx_event);
            last_sfx_event = EVT_NONE;
            hud_tick++;
            if ((hud_tick & 7) == 0) {
                hud_update_game(sc_south_view, world_find_island(heli_px >> 3, heli_py >> 3),
                                heli_alt, heli_ground_alt, heli_speed,
                                game_fuel, cyc_wind_force, crates_collected, heli_lives,
                                game_time_s, game_score10, heli_state, heli_surface,
                                heli_heading_str(), (hud_tick >> 4) & 1);
            }
            continue;
        }

        // Open full-screen Live Tactical Radar Map on M or Space!
        if (m_now && !prev_m) {
            radar_map_show(is_ntsc);
            prev_m = 1;
            hud_tick = 3;
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
        if (ret_now && !prev_ret) do_flip = 1;

        if (c_now && !prev_c) {
            heli_ctrl_mode ^= 1;
            entities_set_banner(
                (heli_ctrl_mode == CTRL_CLASSIC) ? "CTRL: CLASSIC " : "CTRL: ARCADE  ",
                VCOL_WHITE, 50
            );
        }

        // Quick-warp keys 1..7 for inspecting helipad islands
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
                        (game_fuel == 0) ? 1 : 0, cyc_wind_vx, cyc_wind_vy,
                        &cam_dpx, &cam_dpy);
            scroller_move(cam_dpx, cam_dpy);
        }

        // Combine all 8 hardware sprite layers:
        //   0..2 = Helicopter + Rotor + Shadow
        //   3,6,7 = Winch + Crates/Survivors
        //   4..5 = Swirling Cyclone Funnel / Storm Rain / Low-Flying Jet + Shadow
        byte cur_isl = world_find_island(heli_px >> 3, heli_py >> 3);
        byte smask = heli_sync_sprites(sc_south_view);
        smask |= entities_update_and_draw(sc_south_view, is_ntsc);
        smask |= cyclone_update_and_draw(sc_south_view, (cur_isl == 0xff) ? 1 : 0);
        r_spr_enable = smask;

        // Step 3-voice filtered SID audio (Rotor V1 + Storm/Thunder V2 + Priority SFX V3)
        audio_tick_game(heli_state, heli_speed, cyc_wind_force, last_sfx_event);
        last_sfx_event = EVT_NONE;

        prev_fire = fire_now;
        prev_ret  = ret_now;
        prev_c    = c_now;
        prev_m    = m_now;

        scroller_animate_water();

        hud_tick++;
        if ((hud_tick & 3) == 0) {
            hud_update_game(sc_south_view, cur_isl,
                            heli_alt, heli_ground_alt, heli_speed,
                            game_fuel, cyc_wind_force, crates_collected, heli_lives,
                            game_time_s, game_score10, heli_state, heli_surface,
                            heli_heading_str(), (hud_tick >> 4) & 1);
        }
    }

    return 0;
}
