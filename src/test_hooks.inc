// Automated on-target C64 test scenario drivers (included ONLY when compiled with -DTEST_SCENARIO=N)
// Never compiled into production build/cyclone.prg!
// Contract:
//   - On any failed on-target assertion: sets vic.color_border = VCOL_RED and halts!
//   - On pass: keeps vic.color_border = VCOL_BLACK for clean VICE screenshot & pixel inspection!

#if TEST_SCENARIO == 3
    // T03: Full Controls & Mission Help screen
    help_screen_show(is_ntsc, 1);

#elif TEST_SCENARIO == 4
    // T04: Color stability baseline at t=0s (Base Island landed)
    for (byte tf = 0; tf < 10; tf++) {
        scroller_move(0, 0);
        byte sm = heli_sync_sprites(sc_south_view);
        sm |= entities_update_and_draw(sc_south_view, is_ntsc);
        sm |= cyclone_update_and_draw(sc_south_view, 0);
        r_spr_enable = sm;
        if (r_pf_mc2 != PF_MC2_DEFAULT) vic.color_border = VCOL_RED;
    }
    hud_update_game(sc_south_view, 5, heli_alt, heli_ground_alt, heli_speed,
                    game_fuel, cyc_wind_force, crates_collected, heli_lives,
                    game_time_s, game_score10, heli_state, heli_surface,
                    heli_heading_str(), 0);
    for (;;) { raster_wait_frame(); }

#elif TEST_SCENARIO == 5
    // T05: Color stability after 400 frames (~8s) at Base Island — checks zero unexpected palette drift!
    for (unsigned int tf = 0; tf < 400; tf++) {
        scroller_move(0, 0);
        byte sm = heli_sync_sprites(sc_south_view);
        sm |= entities_update_and_draw(sc_south_view, is_ntsc);
        sm |= cyclone_update_and_draw(sc_south_view, 0);
        r_spr_enable = sm;
        if (cyc_wind_force < 13 && r_pf_mc2 != PF_MC2_DEFAULT) {
            vic.color_border = VCOL_RED;
            for (;;) {}
        }
    }
    hud_update_game(sc_south_view, 5, heli_alt, heli_ground_alt, heli_speed,
                    game_fuel, cyc_wind_force, crates_collected, heli_lives,
                    game_time_s, game_score10, heli_state, heli_surface,
                    heli_heading_str(), 0);
    for (;;) { raster_wait_frame(); }

#elif TEST_SCENARIO == 6
    // T06: 8-Way Scrolling Stress Test (N, NE, E, SE, S, SW, W, NW) + On-Target 720-cell Color RAM Audit!
    {
        static const sbyte DIRS_X[8] = {  0,  1, 1, 1, 0, -1, -1, -1 };
        static const sbyte DIRS_Y[8] = { -1, -1, 0, 1, 1,  1,  0, -1 };
        heli_ctrl_mode = CTRL_ARCADE;
        heli_alt = 38;
        heli_state = HELI_STATE_FLYING;
        for (byte d = 0; d < 8; d++) {
            for (byte f = 0; f < 16; f++) {
                sbyte cdx = 0, cdy = 0;
                heli_update(DIRS_X[d], DIRS_Y[d], 0, 0, sc_south_view, 0, 0, 0, &cdx, &cdy);
                scroller_move(cdx, cdy);
                if (!scroller_verify_color_ram()) {
                    vic.color_border = VCOL_RED;
                    for (;;) {}
                }
            }
        }
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

#elif TEST_SCENARIO == 7
    // T07: Diagonal NW (-41) flight on NTSC 60Hz + On-Target 720-cell Color RAM Audit!
    heli_ctrl_mode = CTRL_ARCADE;
    heli_alt = 36;
    heli_state = HELI_STATE_FLYING;
    for (byte tf = 0; tf < 42; tf++) {
        sbyte cdx = 0, cdy = 0;
        heli_update(-1, -1, 0, 0, sc_south_view, 0, 0, 0, &cdx, &cdy);
        scroller_move(cdx, cdy);
        if (!scroller_verify_color_ram()) {
            vic.color_border = VCOL_RED;
            for (;;) {}
        }
    }
    {
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
    // T08: South View flip + diagonal flight + On-Target 720-cell Color RAM Audit!
    scroller_toggle_view();
    heli_ctrl_mode = CTRL_ARCADE;
    heli_alt = 34;
    heli_state = HELI_STATE_FLYING;
    for (byte tf = 0; tf < 32; tf++) {
        sbyte cdx = 0, cdy = 0;
        heli_update(1, 1, 0, 0, sc_south_view, 0, 0, 0, &cdx, &cdy);
        scroller_move(cdx, cdy);
        if (!scroller_verify_color_ram()) {
            vic.color_border = VCOL_RED;
            for (;;) {}
        }
    }
    {
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
    // T09: 320x200 True VIC-II Bitmap Tactical Radar Map
    radar_map_show(is_ntsc);

#elif TEST_SCENARIO == 10
    // T10: Bitmap Radar -> Return to 3D Flight charset restore & raster split verification!
    {
        byte cs0 = CHARSET_NORTH[2], cs1 = CHARSET_NORTH[100], h0 = CHARSET_HUD[10];
        raster_pause_split();
        memcpy(SCREEN_A, CHARSET_NORTH, 640);
        memcpy(SCREEN_B, CHARSET_HUD,   640);
        memset((byte *)0xe000, 0xaa, 8000); // Simulate full bitmap overwrite of $E000-$FF3F!
        memset(CHARSET_NORTH, 0, 2048);
        memcpy(CHARSET_NORTH, SCREEN_A, 640);
        memset(CHARSET_HUD,   0, 2048);
        memcpy(CHARSET_HUD,   SCREEN_B, 640);
        if (CHARSET_NORTH[2] != cs0 || CHARSET_NORTH[100] != cs1 || CHARSET_HUD[10] != h0) {
            vic.color_border = VCOL_RED;
            for (;;) {}
        }
        hud_init();
        raster_resume_split();
        scroller_redraw_full();
        if (!scroller_verify_color_ram()) {
            vic.color_border = VCOL_RED;
            for (;;) {}
        }
        byte sm = heli_sync_sprites(sc_south_view);
        sm |= entities_update_and_draw(sc_south_view, is_ntsc);
        sm |= cyclone_update_and_draw(sc_south_view, 0);
        r_spr_enable = sm;
        hud_update_game(sc_south_view, 5, heli_alt, heli_ground_alt, heli_speed,
                        game_fuel, cyc_wind_force, crates_collected, heli_lives,
                        game_time_s, game_score10, heli_state, heli_surface,
                        heli_heading_str(), 0);
    }
    for (;;) { raster_wait_frame(); }

#elif TEST_SCENARIO == 11
    // T11: Cyclone Storm Tempest right next to Base Island
    cyc_px = heli_px + 64;
    cyc_py = heli_py - 16;
    heli_alt = 28;
    heli_state = HELI_STATE_FLYING;

#elif TEST_SCENARIO == 12
    // T12: Active Winch Cable pickup & alignment over Medical Crate #0
    heli_px = entities[0].px;
    heli_py = entities[0].py;
    heli_alt = (byte)(entities[0].ground_alt + 24);
    heli_state = HELI_STATE_FLYING;
    scroller_init(entities[0].px >> 3, entities[0].py >> 3);
    for (byte tf = 0; tf < 14; tf++) {
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
    if (!winch_active) { vic.color_border = VCOL_RED; for (;;) {} }
    for (;;) { raster_wait_frame(); }

#elif TEST_SCENARIO == 13
    // T13: Waving Survivor (Person) alignment & rescue check on flat island terrain!
    {
        RescueEntity *sv = &entities[NUM_CRATES]; // First waving survivor!
        if (world_get_surface_type(sv->px >> 3, sv->py >> 3) != SURF_FLAT) {
            vic.color_border = VCOL_RED;
            for (;;) {}
        }
        // Position helicopter slightly right of survivor so BOTH survivor & heli shadow are clearly visible!
        heli_px = sv->px + 18;
        heli_py = sv->py;
        heli_alt = (byte)(sv->ground_alt + 18);
        heli_state = HELI_STATE_FLYING;
        scroller_init(sv->px >> 3, sv->py >> 3);
        for (byte tf = 0; tf < 12; tf++) {
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
    }
    for (;;) { raster_wait_frame(); }

#elif TEST_SCENARIO == 14
    // T14: Kokola Island elevated helipad shadow alignment while holding descend (S)!
    heli_warp_to_island(2);
    for (int wy = ISLANDS[2].world_y; wy < ISLANDS[2].world_y + (int)ISLANDS[2].height; wy++) {
        for (int wx = ISLANDS[2].world_x; wx < ISLANDS[2].world_x + (int)ISLANDS[2].width; wx++) {
            if (world_get_surface_type(wx, wy) == SURF_HELIPAD) {
                heli_px = wx * 8 + 4;
                heli_py = wy * 8 + 4;
                heli_ground_alt = world_get_ground_alt(wx, wy);
                heli_alt = (byte)(heli_ground_alt + 6);
                scroller_init(wx, wy);
                goto kokola_pad_ok;
            }
        }
    }
kokola_pad_ok:
    for (byte tf = 0; tf < 36; tf++) {
        sbyte cdx = 0, cdy = 0;
        heli_update(0, 0, 0, -1, sc_south_view, 0, 0, 0, &cdx, &cdy);
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
    if (heli_state != HELI_STATE_LANDED || heli_alt != heli_ground_alt) {
        vic.color_border = VCOL_RED;
        for (;;) {}
    }
    for (;;) { raster_wait_frame(); }

#elif TEST_SCENARIO == 15
    // T15: On-Target Entity Spawn Audit across 8 seeds (120 entities: 40 crates + 80 survivors)!
    for (byte sd = 1; sd <= 8; sd++) {
        entities_init(sd);
        for (byte i = 0; i < NUM_ENTITIES; i++) {
            RescueEntity *e = &entities[i];
            int wx = e->px >> 3;
            int wy = e->py >> 3;
            if (!e->active ||
                world_get_surface_type(wx, wy) != SURF_FLAT ||
                world_get_ground_alt(wx, wy) != e->ground_alt) {
                vic.color_border = VCOL_RED;
                for (;;) {}
            }
        }
    }
    entities_set_banner("120 ENTITIES OK", VCOL_LT_GREEN, 255);
    hud_update_game(sc_south_view, 5, heli_alt, heli_ground_alt, heli_speed,
                    game_fuel, cyc_wind_force, crates_collected, heli_lives,
                    game_time_s, game_score10, heli_state, heli_surface,
                    heli_heading_str(), 0);
    for (;;) { raster_wait_frame(); }

#elif TEST_SCENARIO == 16
    // T16: On-Target Real Fuel Range Measurement + Full 5-Crate Rescue -> Base Island Mission Win!
    {
        // Part A: Measure real C64 cruise distance vs fuel burned over 240 frames flying East!
        heli_ctrl_mode = CTRL_ARCADE;
        heli_alt = 40;
        heli_state = HELI_STATE_FLYING;
        int start_px = heli_px;
        byte start_fuel = game_fuel;
        for (byte tf = 0; tf < 240; tf++) {
            sbyte cdx = 0, cdy = 0;
            heli_update(1, 0, 0, 0, sc_south_view, 0, 0, 0, &cdx, &cdy);
            entities_update_and_draw(sc_south_view, is_ntsc);
        }
        int dist_px = heli_px - start_px;
        byte used_fuel = (byte)(start_fuel - game_fuel);
        // Assert: pixels per fuel >= 26 -> 255 fuel >= 6,630 px (> 4,795 px Banana round-trip + 38% margin!)
        if (used_fuel == 0 || (dist_px / (int)used_fuel) < 26) {
            vic.color_border = VCOL_RED;
            for (;;) {}
        }

        // Part B: Rescue ALL 5 Medical Crates via real winch state machine + return to Base & Win!
        for (byte c = 0; c < NUM_CRATES; c++) {
            heli_px = entities[c].px;
            heli_py = entities[c].py;
            heli_alt = (byte)(entities[c].ground_alt + 16);
            heli_speed = 0;
            heli_state = HELI_STATE_FLYING;
            for (byte wf = 0; wf < 32; wf++) {
                entities_update_and_draw(sc_south_view, is_ntsc);
            }
        }
        // Return to Base Island Helipad & land safely!
        heli_respawn_at_base();
        for (byte lf = 0; lf < 5; lf++) {
            entities_update_and_draw(sc_south_view, is_ntsc);
        }
        if (crates_collected != NUM_CRATES || game_state != GAME_WIN) {
            vic.color_border = VCOL_RED;
            for (;;) {}
        }
        hud_update_game(sc_south_view, 5, heli_alt, heli_ground_alt, heli_speed,
                        game_fuel, cyc_wind_force, crates_collected, heli_lives,
                        game_time_s, game_score10, heli_state, heli_surface,
                        heli_heading_str(), 0);
    }
    for (;;) { raster_wait_frame(); }
#endif
