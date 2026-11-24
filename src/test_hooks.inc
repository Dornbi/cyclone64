// Automated test scenario drivers (included ONLY when compiled with -DTEST_SCENARIO=N)
// Never compiled into production build/cyclone.prg!
#if TEST_SCENARIO == 3
    // Help screen capture
    help_screen_show(is_ntsc, 1);
#elif TEST_SCENARIO == 4
    // Exact Title -> Flight transition: pause split, draw title-like blue screen, resume split & hover
    raster_pause_split();
    vic_waitFrame();
    vic_waitFrame();
    hud_init();
    raster_resume_split();
    scroller_redraw_full();
    heli_alt = 24;
    heli_state = HELI_STATE_FLYING;
    for (byte tf = 0; tf < 18; tf++) {
        sbyte cdx = 0, cdy = 0;
        heli_update(1, 0, 0, 0, sc_south_view, 0, 0, 0, &cdx, &cdy);
        scroller_move(cdx, cdy);
        byte isl = world_find_island(heli_px >> 3, heli_py >> 3);
        byte sm = heli_sync_sprites(sc_south_view);
        sm |= entities_update_and_draw(sc_south_view, is_ntsc);
        sm |= cyclone_update_and_draw(sc_south_view, (isl == 0xff) ? 1 : 0);
        r_spr_enable = sm;
        hud_update_game(sc_south_view, isl, heli_alt, heli_ground_alt, heli_speed,
                        game_fuel, cyc_wind_force, crates_collected, heli_lives,
                        game_time_s, game_score10, heli_state, heli_surface,
                        heli_heading_str(), 0);
    }
    for (;;) { raster_wait_frame(); }
#elif TEST_SCENARIO == 5
    // Diagonal North-West flight (off = -41) crossing multiple tile boundaries & yscroll 0..7!
    heli_ctrl_mode = CTRL_ARCADE;
    heli_alt = 36;
    heli_state = HELI_STATE_FLYING;
    for (byte tf = 0; tf < 42; tf++) {
        sbyte cdx = 0, cdy = 0;
        heli_update(-1, -1, 0, 0, sc_south_view, 0, 0, 0, &cdx, &cdy);
        scroller_move(cdx, cdy);
        byte isl = world_find_island(heli_px >> 3, heli_py >> 3);
        byte sm = heli_sync_sprites(sc_south_view);
        sm |= entities_update_and_draw(sc_south_view, is_ntsc);
        sm |= cyclone_update_and_draw(sc_south_view, (isl == 0xff) ? 1 : 0);
        r_spr_enable = sm;
        hud_update_game(sc_south_view, isl, heli_alt, heli_ground_alt, heli_speed,
                        game_fuel, cyc_wind_force, crates_collected, heli_lives,
                        game_time_s, game_score10, heli_state, heli_surface,
                        heli_heading_str(), 0);
    }
    for (;;) { raster_wait_frame(); }
#elif TEST_SCENARIO == 6
    // Diagonal South-West flight (off = +39) crossing multiple tile boundaries & yscroll 0..7!
    heli_ctrl_mode = CTRL_ARCADE;
    heli_alt = 36;
    heli_state = HELI_STATE_FLYING;
    for (byte tf = 0; tf < 42; tf++) {
        sbyte cdx = 0, cdy = 0;
        heli_update(-1, 1, 0, 0, sc_south_view, 0, 0, 0, &cdx, &cdy);
        scroller_move(cdx, cdy);
        byte isl = world_find_island(heli_px >> 3, heli_py >> 3);
        byte sm = heli_sync_sprites(sc_south_view);
        sm |= entities_update_and_draw(sc_south_view, is_ntsc);
        sm |= cyclone_update_and_draw(sc_south_view, (isl == 0xff) ? 1 : 0);
        r_spr_enable = sm;
        hud_update_game(sc_south_view, isl, heli_alt, heli_ground_alt, heli_speed,
                        game_fuel, cyc_wind_force, crates_collected, heli_lives,
                        game_time_s, game_score10, heli_state, heli_surface,
                        heli_heading_str(), 0);
    }
    for (;;) { raster_wait_frame(); }
#elif TEST_SCENARIO == 8
    // South View flip + diagonal flight!
    scroller_toggle_view();
    heli_ctrl_mode = CTRL_ARCADE;
    heli_alt = 34;
    heli_state = HELI_STATE_FLYING;
    for (byte tf = 0; tf < 32; tf++) {
        sbyte cdx = 0, cdy = 0;
        heli_update(1, 1, 0, 0, sc_south_view, 0, 0, 0, &cdx, &cdy);
        scroller_move(cdx, cdy);
        byte isl = world_find_island(heli_px >> 3, heli_py >> 3);
        byte sm = heli_sync_sprites(sc_south_view);
        sm |= entities_update_and_draw(sc_south_view, is_ntsc);
        sm |= cyclone_update_and_draw(sc_south_view, (isl == 0xff) ? 1 : 0);
        r_spr_enable = sm;
        hud_update_game(sc_south_view, isl, heli_alt, heli_ground_alt, heli_speed,
                        game_fuel, cyc_wind_force, crates_collected, heli_lives,
                        game_time_s, game_score10, heli_state, heli_surface,
                        heli_heading_str(), 0);
    }
    for (;;) { raster_wait_frame(); }
#elif TEST_SCENARIO == 9
    // Tactical Radar Map capture
    radar_map_show(is_ntsc);
#elif TEST_SCENARIO == 10
    // Cyclone Storm Tempest right next to Base Island
    cyc_px = heli_px + 64;
    cyc_py = heli_py - 16;
    heli_alt = 28;
    heli_state = HELI_STATE_FLYING;
#elif TEST_SCENARIO == 11
    // Active Winch Cable pickup directly over first Medical Crate!
    heli_px = entities[0].px;
    heli_py = entities[0].py;
    heli_alt = (byte)(entities[0].ground_alt + 24);
    heli_state = HELI_STATE_FLYING;
    scroller_init(entities[0].px >> 3, entities[0].py >> 3);
    for (byte tf = 0; tf < 28; tf++) {
        scroller_move(0, 0);
        byte isl = world_find_island(heli_px >> 3, heli_py >> 3);
        byte sm = heli_sync_sprites(sc_south_view);
        sm |= entities_update_and_draw(sc_south_view, is_ntsc);
        sm |= cyclone_update_and_draw(sc_south_view, (isl == 0xff) ? 1 : 0);
        r_spr_enable = sm;
        hud_update_game(sc_south_view, isl, heli_alt, heli_ground_alt, heli_speed,
                        game_fuel, cyc_wind_force, crates_collected, heli_lives,
                        game_time_s, game_score10, heli_state, heli_surface,
                        heli_heading_str(), 0);
    }
    for (;;) { raster_wait_frame(); }
#elif TEST_SCENARIO == 12
    // Land on elevated Kokola Island helipad & hold S (descend) for 30 frames:
    // Verifies helicopter stays locked at heli_ground_alt with shadow directly underneath!
    heli_warp_to_island(2);
    // Find Kokola's helipad tile
    for (int wy = ISLANDS[2].world_y; wy < ISLANDS[2].world_y + (int)ISLANDS[2].height; wy++) {
        for (int wx = ISLANDS[2].world_x; wx < ISLANDS[2].world_x + (int)ISLANDS[2].width; wx++) {
            if (world_get_surface_type(wx, wy) == SURF_HELIPAD) {
                heli_px = wx * 8 + 4;
                heli_py = wy * 8 + 4;
                heli_ground_alt = world_get_ground_alt(wx, wy);
                heli_alt = (byte)(heli_ground_alt + 6);
                scroller_init(wx, wy);
                goto kokola_found;
            }
        }
    }
kokola_found:
    for (byte tf = 0; tf < 36; tf++) {
        sbyte cdx = 0, cdy = 0;
        heli_update(0, 0, 0, -1, sc_south_view, 0, 0, 0, &cdx, &cdy); // Hold descend!
        scroller_move(cdx, cdy);
        byte isl = world_find_island(heli_px >> 3, heli_py >> 3);
        byte sm = heli_sync_sprites(sc_south_view);
        sm |= entities_update_and_draw(sc_south_view, is_ntsc);
        sm |= cyclone_update_and_draw(sc_south_view, (isl == 0xff) ? 1 : 0);
        r_spr_enable = sm;
        hud_update_game(sc_south_view, isl, heli_alt, heli_ground_alt, heli_speed,
                        game_fuel, cyc_wind_force, crates_collected, heli_lives,
                        game_time_s, game_score10, heli_state, heli_surface,
                        heli_heading_str(), 0);
    }
    for (;;) { raster_wait_frame(); }
#endif
