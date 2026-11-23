#include "memmap.h"
#include "world.h"
#include "scroller.h"
#include "helicopter.h"
#include "entities.h"

#include <c64/vic.h>

RescueEntity entities[NUM_ENTITIES];
byte game_state       = GAME_PLAYING;
byte game_fuel        = 255;
unsigned int game_time_s  = 480;
unsigned int game_score10 = 0;
byte crates_collected = 0;
byte survivors_saved  = 0;
byte winch_active     = 0;
byte banner_timer     = 0;
const char *banner_text = "";
byte banner_color     = VCOL_YELLOW;
byte last_sfx_event   = EVT_NONE;

static byte winch_target   = 0xff;
static byte winch_progress = 0;
static byte anim_tick      = 0;
static byte fuel_acc       = 0;
static byte sec_frame_cnt  = 0;
static byte refuel_sound_cd = 0;

// 5 outer islands hosting the 5 primary Medical Supply Crates (one in each region!)
static const byte CRATE_ISLANDS[NUM_CRATES] = {
    10, // GIANTS GATEWAY (NE of Base - great first crate!)
     6, // GILLIGANS ISLAND (North)
     0, // BANANA ISLAND (East)
     7, // RED ISLAND (South)
    13  // ENTERPRISE ISLAND (SW)
};

// 10 islands hosting optional Stranded Survivors (starting with Base Island's eastern ridge!)
static const byte SURV_ISLANDS[NUM_SURVIVORS] = {
    5,  // BASE ISLAND (east side - immediate rescue sightline!)
    2,  // KOKOLA ISLAND
    3,  // LAGOON ISLAND
    4,  // PEAK ISLAND
    9,  // BONE ISLAND
    11, // CLAW ISLAND
    1,  // ORTE ROCKS
    8,  // SKEG ISLAND
    12, // LUKELAND ISLES
    13  // ENTERPRISE ISLAND
};

void entities_set_banner(const char *msg, byte color, byte frames) {
    banner_text  = msg;
    banner_color = color;
    banner_timer = frames;
}

// Find a genuine SURF_FLAT landing/pickup tile on island `isl_id`, biased by `slot_bias`
static void place_on_island(RescueEntity *e, byte isl_id, byte kind, byte slot_bias) {
    const IslandDesc *isl = &ISLANDS[isl_id];
    byte w = isl->width;
    byte h = isl->height;

    // For Base Island survivor, search eastern half so it sits across the channel from Base pad
    byte x_start = (isl_id == 5) ? (byte)(w >> 1) : 2;
    byte y_start = 3;

    // Pseudo-varied scan for a clear flat grass/beach tile with flat neighbors
    byte best_dx = (byte)(w >> 1);
    byte best_dy = (byte)(h >> 1);
    byte found   = 0;
    byte skip    = (byte)(slot_bias & 7);

    for (byte dy = y_start; dy < (byte)(h - 3) && !found; dy += 2) {
        for (byte dx = x_start; dx < (byte)(w - 3) && !found; dx += 2) {
            int wx = isl->world_x + (int)dx;
            int wy = isl->world_y + (int)dy;
            if (world_get_surface_type(wx, wy) == SURF_FLAT) {
                if (skip > 0) {
                    skip--;
                } else {
                    best_dx = dx;
                    best_dy = dy;
                    found   = 1;
                }
            }
        }
    }

    int wx = isl->world_x + (int)best_dx;
    int wy = isl->world_y + (int)best_dy;
    e->px         = (wx << 3) + 4;
    e->py         = (wy << 3) + 4;
    e->ground_alt = world_get_ground_alt(wx, wy);
    e->kind       = kind;
    e->island_id  = isl_id;
    e->active     = 1;
}

void entities_init(byte seed) {
    game_state       = GAME_PLAYING;
    game_fuel        = 255;
    game_time_s      = 480; // 08:00 mission clock
    game_score10     = 0;
    crates_collected = 0;
    survivors_saved  = 0;
    winch_active     = 0;
    winch_target     = 0xff;
    winch_progress   = 0;
    fuel_acc         = 0;
    sec_frame_cnt    = 0;
    banner_timer     = 0;
    last_sfx_event   = EVT_NONE;

    for (byte i = 0; i < NUM_CRATES; i++) {
        place_on_island(&entities[i], CRATE_ISLANDS[i], ENT_CRATE, (byte)(seed + i * 3));
    }
    for (byte i = 0; i < NUM_SURVIVORS; i++) {
        place_on_island(&entities[NUM_CRATES + i], SURV_ISLANDS[i], ENT_SURVIVOR, (byte)(seed + i * 5 + 2));
    }

    entities_set_banner("FIND 5 CRATES!", VCOL_YELLOW, 120);
}

static inline void set_spr_ptr_both(byte slot, byte ptr) {
    SPR_PTRS_A[slot] = ptr;
    SPR_PTRS_B[slot] = ptr;
}

// Convert entity world (px, py, alt) to VIC-II screen coordinates; returns 1 if inside playfield
static byte project_world_sprite(int px, int py, byte alt, byte south_view, int *out_x, int *out_y) {
    int rel_x = px - sc_cam_px;
    int rel_y = py - sc_cam_py;
    if (rel_x < 4 || rel_x > (PF_COLS * 8 - 12)) return 0;
    if (rel_y < -8 || rel_y > (PF_ROWS * 8 + 48)) return 0;

    int sx, gy;
    if (!south_view) {
        sx = 31 + rel_x - 12;
        gy = 54 + rel_y;
    } else {
        sx = 31 + (PF_COLS * 8 - 1 - rel_x) - 12;
        gy = 54 + (PF_ROWS * 8 - 1 - rel_y);
    }
    int sy = gy - (int)alt - 12;
    if (sy < 44 || sy > 184) return 0;

    *out_x = sx;
    *out_y = sy;
    return 1;
}

byte entities_update_and_draw(byte south_view, byte is_ntsc) {
    byte spr_mask = 0;
    anim_tick++;
    last_sfx_event = EVT_NONE;

    if (banner_timer > 0) banner_timer--;

    if (game_state != GAME_PLAYING) {
        return 0;
    }

    // 1. Mission Countdown Timer (1 Hz: 50 frames PAL / 60 frames NTSC)
    sec_frame_cnt++;
    byte fps = is_ntsc ? 60 : 50;
    if (sec_frame_cnt >= fps) {
        sec_frame_cnt = 0;
        if (game_time_s > 0) {
            game_time_s--;
        }
        if (game_time_s == 0) {
            game_state = GAME_OVER_TIME;
            last_sfx_event = EVT_GAME_OVER;
            entities_set_banner("! NO TIME !   ", VCOL_RED, 255);
            return 0;
        }
    }

    // 2. Fuel Drain (airborne) & Helipad Refueling (landed on pad)
    if (heli_state == HELI_STATE_FLYING) {
        byte burn = (byte)(1 + (heli_speed >> 4));
        fuel_acc += burn;
        if (fuel_acc >= 18) {
            fuel_acc -= 18;
            if (game_fuel > 0) game_fuel--;
            if (game_fuel == 60 || game_fuel == 30) {
                last_sfx_event = EVT_LOW_FUEL;
                entities_set_banner("! LOW FUEL !  ", VCOL_LT_RED, 75);
            } else if (game_fuel == 0) {
                last_sfx_event = EVT_LOW_FUEL;
                entities_set_banner("! NO FUEL !   ", VCOL_RED, 120);
            }
        }
    } else if (heli_state == HELI_STATE_LANDED && heli_surface == SURF_HELIPAD) {
        // Check Base Island victory drop-off first!
        byte cur_isl = world_find_island(heli_px >> 3, heli_py >> 3);
        if (cur_isl == 5 && crates_collected >= NUM_CRATES) {
            // Mission Complete!! Award time bonus + lives bonus
            game_score10 += 200; // +2000 mission clear
            game_score10 += game_time_s; // +10 per remaining second
            game_score10 += (unsigned int)heli_lives * 50; // +500 per spare chopper
            game_state = GAME_WIN;
            last_sfx_event = EVT_WIN;
            entities_set_banner("MISSION CLEAR!", VCOL_LT_GREEN, 255);
            return 0;
        }

        // Rapid helipad refueling
        if (game_fuel < 255) {
            if (game_fuel > 251) game_fuel = 255;
            else game_fuel += 4;
            if (refuel_sound_cd == 0) {
                last_sfx_event = EVT_REFUEL_TICK;
                refuel_sound_cd = 6;
            } else {
                refuel_sound_cd--;
            }
            entities_set_banner("REFUELING...  ", VCOL_LT_GREEN, 10);
        }
    }

    // 3. Automatic Winch Detection & Hoisting
    if (heli_state == HELI_STATE_CRASHING) {
        winch_active = 0;
        winch_target = 0xff;
    } else if (winch_active && winch_target < NUM_ENTITIES) {
        RescueEntity *e = &entities[winch_target];
        int dx = heli_px - e->px;
        int dy = heli_py - e->py;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;

        // Keep hovering near target to complete hoist; drifting too far aborts cleanly
        if (dx > 20 || dy > 20 || heli_alt > (byte)(e->ground_alt + 36)) {
            winch_active = 0;
            winch_target = 0xff;
            winch_progress = 0;
        } else {
            winch_progress++;
            if (winch_progress >= 24) {
                // Hoisted aboard!!
                e->active = 0;
                winch_active = 0;
                winch_target = 0xff;
                winch_progress = 0;
                if (e->kind == ENT_CRATE) {
                    crates_collected++;
                    game_score10 += 50; // +500 pts
                    last_sfx_event = EVT_CRATE_GET;
                    if (crates_collected >= NUM_CRATES) {
                        entities_set_banner("RETURN TO BASE", VCOL_LT_GREEN, 180);
                    } else {
                        entities_set_banner("CRATE HOISTED!", VCOL_YELLOW, 90);
                    }
                } else {
                    survivors_saved++;
                    game_score10 += 25; // +250 pts
                    last_sfx_event = EVT_SURV_GET;
                    entities_set_banner("SURVIVOR +250!", VCOL_CYAN, 90);
                }
            }
        }
    } else if (heli_state == HELI_STATE_FLYING && heli_speed <= 18) {
        // Scan for any nearby uncollected crate or survivor underneath helicopter
        for (byte i = 0; i < NUM_ENTITIES; i++) {
            RescueEntity *e = &entities[i];
            if (!e->active) continue;
            int dx = heli_px - e->px;
            if (dx < -12 || dx > 12) continue;
            int dy = heli_py - e->py;
            if (dy < -12 || dy > 12) continue;
            if (heli_alt <= (byte)(e->ground_alt + 28) && heli_alt >= e->ground_alt) {
                winch_active   = 1;
                winch_target   = i;
                winch_progress = 0;
                last_sfx_event = EVT_WINCH_START;
                entities_set_banner("WINCHING...   ", VCOL_WHITE, 30);
                break;
            }
        }
    }

    // 4. Draw Animated Winch Cable on Sprite 3 (Hi-Res White) when hoisting!
    if (winch_active && winch_target < NUM_ENTITIES) {
        int hx, hy;
        if (project_world_sprite(heli_px, heli_py, heli_alt, south_view, &hx, &hy)) {
            byte len_idx = (winch_progress < 8) ? 2 : ((winch_progress < 16) ? 1 : 0);
            set_spr_ptr_both(3, (byte)(SPR_PTR_WINCH_BASE + len_idx));
            vic.spr_multi &= (byte)~0x08;     // Sprite 3 = Hi-Res
            vic.spr_color[3] = VCOL_WHITE;
            vic_sprxy(3, hx, hy + 10);
            spr_mask |= 0x08;
        }
    }

    // 5. Render up to 2 Visible World Crates / Survivors on Sprites 6 & 7 (Multicolor!)
    byte next_slot = 6;
    byte blink = (anim_tick >> 4) & 1;

    // Prioritize active winch target first so it visibly rises up the cable!
    for (byte pass = 0; pass < 2 && next_slot <= 7; pass++) {
        for (byte i = 0; i < NUM_ENTITIES && next_slot <= 7; i++) {
            RescueEntity *e = &entities[i];
            if (!e->active) continue;
            byte is_winched = (winch_active && winch_target == i) ? 1 : 0;
            if ((pass == 0 && !is_winched) || (pass == 1 && is_winched)) continue;

            byte draw_alt = e->ground_alt;
            if (is_winched && heli_alt > e->ground_alt) {
                // Smoothly interpolate target rising up the winch cable toward helicopter!
                byte lift = (byte)(((unsigned int)(heli_alt - e->ground_alt) * winch_progress) / 24);
                draw_alt = (byte)(e->ground_alt + lift);
            }

            int sx, sy;
            if (project_world_sprite(e->px, e->py, draw_alt, south_view, &sx, &sy)) {
                byte ptr = (e->kind == ENT_CRATE)
                    ? (byte)(SPR_PTR_CRATE_BASE + blink)
                    : (byte)(SPR_PTR_SURV_BASE  + blink);
                set_spr_ptr_both(next_slot, ptr);
                vic.spr_multi |= (byte)(1 << next_slot); // Multicolor sprite!
                vic.spr_color[next_slot] = (e->kind == ENT_CRATE) ? VCOL_YELLOW : VCOL_ORANGE;
                vic_sprxy(next_slot, sx, sy);
                spr_mask |= (byte)(1 << next_slot);
                next_slot++;
            }
        }
    }

    return spr_mask;
}
