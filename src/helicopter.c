#include "memmap.h"
#include "world.h"
#include "raster.h"
#include "scroller.h"
#include "helicopter.h"

#include <c64/vic.h>

// Base Island western 4x3 white helipad center:
// Base Island world_x=306, world_y=364; pad local (16..19, 17..19) -> world tile (323, 382)
#define BASE_PAD_WX   324
#define BASE_PAD_WY   382
#define BASE_PAD_PX   (BASE_PAD_WX * 8)
#define BASE_PAD_PY   (BASE_PAD_WY * 8)

// Camera target offset so helicopter rests comfortably centered with ample climb headroom
#define CAM_OFFSET_X  156
#define CAM_OFFSET_Y  96

#define SAFE_PAD_SPEED   18
#define SAFE_FLAT_SPEED  10

int  heli_px         = BASE_PAD_PX;
int  heli_py         = BASE_PAD_PY;
byte heli_alt        = 8;
byte heli_ground_alt = 8;
byte heli_heading    = 4; // Facing East on takeoff
byte heli_speed      = 0;
byte heli_state      = HELI_STATE_LANDED;
byte heli_lives      = 3;
byte heli_ctrl_mode  = CTRL_CLASSIC;
byte heli_surface    = SURF_HELIPAD;

// Sub-pixel fixed-point accumulators (1/64th pixel resolution for ultra-smooth 16-angle flight!)
static sbyte sub_x       = 0;
static sbyte sub_y       = 0;
static sbyte vel_x       = 0; // Inertial world velocity X (-64..+64 in 1/32 px/frame)
static sbyte vel_y       = 0; // Inertial world velocity Y (-64..+64 in 1/32 px/frame)
static byte  turn_cd     = 0;
static byte  rotor_phase = 0;
static byte  rotor_tick  = 0;
static byte  crash_timer = 0;

// 16-direction unit vectors (scaled by 32): 0=North(0,-32), 4=East(+32,0), 8=South(0,+32), 12=West(-32,0)
static const sbyte DIR_DX[16] = {
      0,  12,  23,  30,  32,  30,  23,  12,
      0, -12, -23, -30, -32, -30, -23, -12
};
static const sbyte DIR_DY[16] = {
    -32, -30, -23, -12,   0,  12,  23,  30,
     32,  30,  23,  12,   0, -12, -23, -30
};

static const char * const HEADING_NAMES[16] = {
    "N  ", "NNE", "NE ", "ENE",
    "E  ", "ESE", "SE ", "SSE",
    "S  ", "SSW", "SW ", "WSW",
    "W  ", "WNW", "NW ", "NNW"
};

// Map 8-way screen joystick input (-1/0/+1) to target heading (0..15)
static const byte JOY_TO_HEADING[9] = {
    14, 0, 2,   // dy = -1: NW, N, NE
    12, 0, 4,   // dy =  0: W,  -, E
    10, 8, 6    // dy = +1: SW, S, SE
};

static void set_spr_ptrs(byte s0, byte s1, byte s2) {
    // Write to both double-buffered screen pointer tables so buffer swaps stay 100% glitch-free
    SPR_PTRS_A[0] = s0; SPR_PTRS_B[0] = s0;
    SPR_PTRS_A[1] = s1; SPR_PTRS_B[1] = s1;
    SPR_PTRS_A[2] = s2; SPR_PTRS_B[2] = s2;
}

static void setup_vic_sprites(void) {
    // Sprite 0: Multicolor Rescue Helicopter (Red/LtRed body, White stripes, Black canopy)
    // Sprite 1: Hi-Res Spinning Rotor Overlay (Black crisp 1px blades)
    // Sprite 2: Hi-Res Checkerboard Ground Shadow (Black translucent dither)
    vic.spr_multi    = 0x01;          // Only Sprite 0 is Multicolor (Sprites 1 & 2 are Hi-Res)
    vic.spr_expand_x = 0x00;
    vic.spr_expand_y = 0x00;
    vic.spr_priority = 0x00;          // Sprites draw in front of terrain
    vic.spr_mcolor0  = VCOL_WHITE;    // %01 = White rescue stripes & tail fin
    vic.spr_mcolor1  = VCOL_BLACK;    // %11 = Black cockpit glass
    vic.spr_color[0] = VCOL_LT_RED;   // %10 = Bright Coastguard Rescue Red!
    vic.spr_color[1] = VCOL_BLACK;    // Hi-Res spinning rotor blades
    vic.spr_color[2] = VCOL_DARK_GREY;// Hi-Res dithered ground shadow (soft dark grey)
}

void heli_respawn_at_base(void) {
    heli_px         = BASE_PAD_PX;
    heli_py         = BASE_PAD_PY;
    heli_ground_alt = world_get_ground_alt(BASE_PAD_WX, BASE_PAD_WY);
    heli_alt        = heli_ground_alt;
    heli_surface    = SURF_HELIPAD;
    heli_heading    = 4;
    heli_speed      = 0;
    vel_x           = 0;
    vel_y           = 0;
    sub_x           = 0;
    sub_y           = 0;
    heli_state      = HELI_STATE_LANDED;
    crash_timer     = 0;

    setup_vic_sprites();
    r_spr_enable = 0x07; // Enable Sprites 0, 1, 2
    scroller_init(BASE_PAD_WX, BASE_PAD_WY - 3);
    heli_sync_sprites(sc_south_view);
}

void heli_init(void) {
    heli_lives     = 3;
    heli_ctrl_mode = CTRL_CLASSIC;
    heli_respawn_at_base();
}

void heli_warp_to_island(byte island_id) {
    if (island_id >= NUM_ISLANDS) return;
    const IslandDesc *isl = &ISLANDS[island_id];
    int wx = isl->world_x + (isl->width >> 1);
    int wy = isl->world_y + (isl->height >> 1);
    heli_px         = wx * 8;
    heli_py         = wy * 8;
    heli_ground_alt = world_get_ground_alt(wx, wy);
    heli_alt        = (byte)(heli_ground_alt + 24);
    if (heli_alt > HELI_MAX_ALT) heli_alt = HELI_MAX_ALT;
    heli_speed      = 0;
    vel_x           = 0;
    vel_y           = 0;
    sub_x           = 0;
    sub_y           = 0;
    heli_state      = HELI_STATE_FLYING;
    setup_vic_sprites();
    r_spr_enable = 0x07;
    scroller_init(wx, wy);
    heli_sync_sprites(sc_south_view);
}

static void trigger_crash(void) {
    heli_state   = HELI_STATE_CRASHING;
    crash_timer  = 0;
    heli_speed   = 0;
    vel_x        = 0;
    vel_y        = 0;
    vic.spr_color[0] = VCOL_ORANGE; // Fiery explosion palette!
    r_spr_enable = 0x01;            // Show only explosion sprite 0
}

void heli_update(sbyte joy_dx, sbyte joy_dy, byte fire_held,
                 sbyte direct_alt_cmd, byte south_view,
                 sbyte *out_dpx, sbyte *out_dpy) {
    *out_dpx = 0;
    *out_dpy = 0;

    // Handle active crash explosion animation
    if (heli_state == HELI_STATE_CRASHING) {
        crash_timer++;
        if (crash_timer >= 42) {
            if (heli_lives > 1) {
                heli_lives--;
            } else {
                // Phase 2 demo: restore 3 lives on empty so you can keep playtesting!
                heli_lives = 3;
            }
            heli_respawn_at_base();
        }
        return;
    }

    // 1. Altitude Control: Hold Fire + Up/Down OR direct Q/Z keys!
    sbyte alt_cmd = direct_alt_cmd;
    if (fire_held && joy_dy != 0) {
        alt_cmd = (sbyte)(-joy_dy); // Stick Up (-1) -> Climb (+1), Stick Down (+1) -> Descend (-1)
    }

    if (alt_cmd > 0) {
        if (heli_alt < HELI_MAX_ALT) heli_alt++;
        if (heli_state == HELI_STATE_LANDED && heli_alt > heli_ground_alt) {
            heli_state = HELI_STATE_FLYING;
        }
    } else if (alt_cmd < 0) {
        if (heli_alt > 0) heli_alt--;
    }

    // 2. Steering & Thrust (only when airborne; when landed, climbing lifts off cleanly!)
    if (heli_state == HELI_STATE_FLYING) {
        if (heli_ctrl_mode == CTRL_CLASSIC) {
            // Classic Cyclone flight: Left/Right rotates heading, Up/Down adjusts throttle
            if (!fire_held) {
                if (joy_dx != 0) {
                    if (turn_cd == 0) {
                        heli_heading = (byte)((heli_heading + joy_dx) & 15);
                        turn_cd = 3;
                    } else {
                        turn_cd--;
                    }
                } else {
                    turn_cd = 0;
                }

                if (joy_dy < 0) {
                    if (heli_speed < HELI_MAX_SPEED) heli_speed += 2;
                } else if (joy_dy > 0) {
                    if (heli_speed >= 3) heli_speed -= 3;
                    else heli_speed = 0;
                }
            }
        } else {
            // Direct 8-way Arcade Mode: stick sets desired heading + cruises automatically
            if (!fire_held && (joy_dx != 0 || joy_dy != 0)) {
                byte target_h = JOY_TO_HEADING[(joy_dy + 1) * 3 + (joy_dx + 1)];
                if (south_view) target_h = (byte)((target_h + 8) & 15);
                sbyte diff = (sbyte)((target_h - heli_heading) & 15);
                if (diff != 0 && turn_cd == 0) {
                    if (diff <= 8) heli_heading = (byte)((heli_heading + 1) & 15);
                    else           heli_heading = (byte)((heli_heading - 1) & 15);
                    turn_cd = 1;
                } else if (turn_cd > 0) {
                    turn_cd--;
                }
                if (heli_speed < 36) heli_speed += 2;
            } else if (!fire_held) {
                if (heli_speed >= 2) heli_speed -= 2;
                else heli_speed = 0;
            }
        }

        // Compute target velocity from heading & speed, and blend smoothly for helicopter inertia!
        // Note: target_vx = (DIR_DX[h] * speed) >> 4  -> range [-96..+96] (in 1/32 px/frame = max ~3 px/f)
        int tvx = ((int)DIR_DX[heli_heading] * (int)heli_speed) >> 4;
        int tvy = ((int)DIR_DY[heli_heading] * (int)heli_speed) >> 4;
        if (tvx >  72) tvx =  72;
        if (tvx < -72) tvx = -72;
        if (tvy >  72) tvy =  72;
        if (tvy < -72) tvy = -72;

        // Smooth inertial approach (gives genuine helicopter momentum & drift!)
        if (vel_x < (sbyte)tvx) vel_x += (vel_x + 4 <= (sbyte)tvx) ? 4 : 1;
        else if (vel_x > (sbyte)tvx) vel_x -= (vel_x - 4 >= (sbyte)tvx) ? 4 : 1;

        if (vel_y < (sbyte)tvy) vel_y += (vel_y + 4 <= (sbyte)tvy) ? 4 : 1;
        else if (vel_y > (sbyte)tvy) vel_y -= (vel_y - 4 >= (sbyte)tvy) ? 4 : 1;

        // Integrate fixed-point sub-pixel motion (divide by 32 -> 1 pixel)
        int ax = (int)sub_x + (int)vel_x;
        int ay = (int)sub_y + (int)vel_y;
        sbyte move_x = 0;
        sbyte move_y = 0;
        while (ax >= 32)  { ax -= 32; move_x++; }
        while (ax <= -32) { ax += 32; move_x--; }
        while (ay >= 32)  { ay -= 32; move_y++; }
        while (ay <= -32) { ay += 32; move_y--; }
        sub_x = (sbyte)ax;
        sub_y = (sbyte)ay;

        // Apply world bounds (keep comfortably inside 768x768 archipelago)
        int nx = heli_px + (int)move_x;
        int ny = heli_py + (int)move_y;
        if (nx < 160)  nx = 160;
        if (nx > 5980) nx = 5980;
        if (ny < 120)  ny = 120;
        if (ny > 6000) ny = 6000;
        heli_px = nx;
        heli_py = ny;
    }

    // 3. Query 3D Terrain Elevation & Surface Type beneath helicopter footprint
    int wx = heli_px >> 3;
    int wy = heli_py >> 3;
    heli_ground_alt = world_get_ground_alt(wx, wy);
    heli_surface    = world_get_surface_type(wx, wy);

    // 4. 3D Collision & Safe Landing Check
    if (heli_state == HELI_STATE_FLYING) {
        if (heli_surface == SURF_SEA) {
            if (heli_alt <= 2) {
                trigger_crash();
                return;
            }
        } else if (heli_alt <= heli_ground_alt) {
            if (heli_surface == SURF_HELIPAD && heli_speed <= SAFE_PAD_SPEED) {
                // Smooth touchdown on Helipad!
                heli_alt   = heli_ground_alt;
                heli_speed = 0;
                vel_x      = 0;
                vel_y      = 0;
                heli_state = HELI_STATE_LANDED;
            } else if (heli_surface == SURF_FLAT && heli_speed <= SAFE_FLAT_SPEED) {
                // Gentle emergency touchdown on flat ground/beach
                heli_alt   = heli_ground_alt;
                heli_speed = 0;
                vel_x      = 0;
                vel_y      = 0;
                heli_state = HELI_STATE_LANDED;
            } else {
                // Hard impact or collision with cliff wall / tree / building / spire!
                trigger_crash();
                return;
            }
        }
    }

    // 5. Smooth Camera Follow (keeps helicopter nicely centered in BOTH North & South views!)
    int target_cam_x = heli_px - CAM_OFFSET_X;
    int target_cam_y = south_view ? (heli_py - (PF_ROWS * 8 - 1 - CAM_OFFSET_Y)) : (heli_py - CAM_OFFSET_Y);
    int cdx = target_cam_x - sc_cam_px;
    int cdy = target_cam_y - sc_cam_py;
    if (cdx >  3) cdx =  3;
    if (cdx < -3) cdx = -3;
    if (cdy >  3) cdy =  3;
    if (cdy < -3) cdy = -3;

    *out_dpx = (sbyte)cdx;
    *out_dpy = (sbyte)cdy;

    // 6. Animate Spinning Rotor (faster when airborne, gentle idle chop when landed!)
    rotor_tick++;
    byte rotor_div = (heli_state == HELI_STATE_LANDED) ? 3 : 1;
    if (rotor_tick >= rotor_div) {
        rotor_tick = 0;
        rotor_phase = (byte)((rotor_phase + 1) & 3);
    }
}

void heli_sync_sprites(byte south_view) {
    // Convert world pixel (heli_px, heli_py) to hardware VIC-II sprite coordinates
    int rel_x = heli_px - sc_cam_px;
    int rel_y = heli_py - sc_cam_py;

    int screen_x, ground_y;
    if (!south_view) {
        screen_x = 31 + rel_x - 12;
        ground_y = 54 + rel_y;
    } else {
        screen_x = 31 + (PF_COLS * 8 - 1 - rel_x) - 12;
        ground_y = 54 + (PF_ROWS * 8 - 1 - rel_y);
    }

    int heli_y = ground_y - (int)heli_alt - 12;
    int shad_y = ground_y - (int)heli_ground_alt - 10;

    // Clamp sprite Y cleanly inside playfield top/bottom margins
    if (heli_y < 42)  heli_y = 42;
    if (heli_y > 186) heli_y = 186;
    if (shad_y < 44)  shad_y = 44;
    if (shad_y > 188) shad_y = 188;

    if (heli_state == HELI_STATE_CRASHING) {
        byte frame = crash_timer >> 3;
        if (frame > 5) frame = 5;
        set_spr_ptrs((byte)(SPR_PTR_EXPL_BASE + frame), SPR_PTR_ROTOR_BASE, SPR_PTR_SHAD_BASE);
        vic_sprxy(0, screen_x, heli_y);
        r_spr_enable = 0x01;
        return;
    }

    // In South View (180-deg rotated world), visual heading rotates by +8 (180 deg)
    byte vis_heading = south_view ? (byte)((heli_heading + 8) & 15) : heli_heading;

    set_spr_ptrs(
        (byte)(SPR_PTR_HELI_BASE  + vis_heading),
        (byte)(SPR_PTR_ROTOR_BASE + rotor_phase),
        (byte)(SPR_PTR_SHAD_BASE  + vis_heading)
    );

    vic_sprxy(0, screen_x, heli_y);
    vic_sprxy(1, screen_x, heli_y);
    vic_sprxy(2, screen_x, shad_y);
    r_spr_enable = 0x07;
}

const char *heli_heading_str(void) {
    return HEADING_NAMES[heli_heading & 15];
}
