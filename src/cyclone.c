#include "memmap.h"
#include "world.h"
#include "raster.h"
#include "scroller.h"
#include "helicopter.h"
#include "entities.h"
#include "cyclone.h"

#include <c64/vic.h>

#pragma code(code_hi)
#pragma data(data_hi)

int   cyc_px         = 1400;
int   cyc_py         = 1200;
byte  cyc_wind_force = 2;
sbyte cyc_wind_vx    = 0;
sbyte cyc_wind_vy    = 0;
byte  jet_active     = 0;

static byte  cyc_dir        = 5;   // 0..7
static byte  cyc_step_timer = 0;
static byte  cyc_turn_cnt   = 0;
static byte  rng_state      = 0x5a;
static byte  storm_tick     = 0;
static byte  lightning_tmr  = 0;
static byte  lightning_cd   = 90;

// Low-flying aircraft state
static int   jet_px         = 0;
static int   jet_py         = 0;
static byte  jet_alt        = 32;
static byte  jet_dir        = 0;   // 0=E, 1=W, 2=N, 3=S
static byte  jet_timer      = 0;
static byte  jet_cooldown   = 180;

// 8-direction patrol vectors (matches original Spectrum $9198)
static const sbyte CYC_DX[8] = {  0,  2,  2,  2,  0, -2, -2, -2 };
static const sbyte CYC_DY[8] = { -2, -2,  0,  2,  2,  2,  0, -2 };

static byte next_rng(void) {
    rng_state = (byte)((rng_state << 1) ^ ((rng_state & 0x80) ? 0x1d : 0x00) ^ 0x37);
    return rng_state;
}

static inline void set_spr_ptr_both(byte slot, byte ptr) {
    SPR_PTRS_A[slot] = ptr;
    SPR_PTRS_B[slot] = ptr;
}

void cyclone_init(byte seed) {
    rng_state      = (byte)(seed * 37 + 0x53);
    // Start Cyclone in one of the outer quadrants away from Base Island (2592, 3056)
    byte q = next_rng() & 3;
    cyc_px = (q & 1) ? 4600 : 1200;
    cyc_py = (q & 2) ? 4700 : 1100;
    cyc_dir        = next_rng() & 7;
    cyc_step_timer = 0;
    cyc_turn_cnt   = 0;
    cyc_wind_force = 2;
    cyc_wind_vx    = 0;
    cyc_wind_vy    = 0;
    lightning_tmr  = 0;
    lightning_cd   = 80;

    jet_active     = 0;
    jet_cooldown   = 150;
}

void cyclone_tick_ai(void) {
    storm_tick++;

    // Smooth continuous patrol across the 768x768 world (steps every 2 frames -> menacing drift!)
    cyc_step_timer++;
    if (cyc_step_timer >= 2) {
        cyc_step_timer = 0;
        cyc_px += (int)CYC_DX[cyc_dir];
        cyc_py += (int)CYC_DY[cyc_dir];

        // Bounce cleanly off archipelago edges (keep inside playable interior)
        byte bounced = 0;
        if (cyc_px < 320)  { cyc_px = 320;  bounced = 1; }
        if (cyc_px > 5800) { cyc_px = 5800; bounced = 1; }
        if (cyc_py < 320)  { cyc_py = 320;  bounced = 1; }
        if (cyc_py > 5800) { cyc_py = 5800; bounced = 1; }

        cyc_turn_cnt++;
        if (bounced || cyc_turn_cnt >= 96) {
            cyc_turn_cnt = 0;
            // Slight bias to sweep generally across the archipelago toward player sector
            if ((next_rng() & 3) == 0) {
                sbyte sx = (heli_px > cyc_px) ? 1 : -1;
                sbyte sy = (heli_py > cyc_py) ? 1 : -1;
                if (sx > 0 && sy < 0)      cyc_dir = 1;
                else if (sx > 0 && sy > 0) cyc_dir = 3;
                else if (sx < 0 && sy > 0) cyc_dir = 5;
                else                       cyc_dir = 7;
            } else {
                cyc_dir = next_rng() & 7;
            }
        }
    }

    // Authentic Cyclone wind force formula (Spectrum $9111-$916E):
    // 24x24 tactical grid (px >> 8 -> 0..23), Chebyshev distance clamped 0..15, wind = 16 - dist
    int hx_g = heli_px >> 8;
    int hy_g = heli_py >> 8;
    int cx_g = cyc_px  >> 8;
    int cy_g = cyc_py  >> 8;
    int dx_g = hx_g - cx_g; if (dx_g < 0) dx_g = -dx_g;
    int dy_g = hy_g - cy_g; if (dy_g < 0) dy_g = -dy_g;
    int dist = (dx_g > dy_g) ? dx_g : dy_g;
    if (dist > 15) dist = 15;
    cyc_wind_force = (byte)(16 - dist);
}

static void spawn_jet(void) {
    jet_active = 1;
    jet_timer  = 110;
    jet_dir    = next_rng() & 3;
    // Match cruising altitude band (20..44 px) so pilot can dodge vertically or horizontally
    jet_alt    = (byte)(20 + (next_rng() & 24));

    if (jet_dir == 0) {        // Eastbound across viewport
        jet_px = heli_px - 180;
        jet_py = heli_py + ((sbyte)(next_rng() & 63) - 32);
    } else if (jet_dir == 1) { // Westbound
        jet_px = heli_px + 180;
        jet_py = heli_py + ((sbyte)(next_rng() & 63) - 32);
    } else if (jet_dir == 2) { // Northbound
        jet_px = heli_px + ((sbyte)(next_rng() & 63) - 32);
        jet_py = heli_py + 120;
    } else {                   // Southbound
        jet_px = heli_px + ((sbyte)(next_rng() & 63) - 32);
        jet_py = heli_py - 120;
    }

    last_sfx_event = EVT_AIRCRAFT;
    entities_set_banner("!BEWARE JET!  ", VCOL_YELLOW, 75);
}

static byte project_hazard(int px, int py, byte alt, byte south_view, int *ox, int *oy) {
    int rel_x = px - sc_cam_px;
    int rel_y = py - sc_cam_py;
    if (rel_x < -16 || rel_x > (PF_COLS * 8 + 8)) return 0;
    if (rel_y < -16 || rel_y > (PF_ROWS * 8 + 56)) return 0;

    int sx, gy;
    if (!south_view) {
        sx = 31 + rel_x - 12;
        gy = 54 + rel_y;
    } else {
        sx = 24 + (PF_COLS * 8 - rel_x) - 12;
        gy = 47 + (PF_ROWS * 8 - rel_y);
    }
    int sy = gy - (int)alt - 12;
    if (sy < 42 || sy > 180) return 0;

    *ox = sx;
    *oy = sy;
    return 1;
}

byte cyclone_update_and_draw(byte south_view, byte over_sea) {
    cyclone_tick_ai();

    // 1. Dynamic Aerodynamic Wind Drift & Cyclone Eye Downdraft
    if (cyc_wind_force >= 7 && heli_state == HELI_STATE_FLYING) {
        sbyte str = (sbyte)((cyc_wind_force - 6) * 3); // 3..30
        // Swirling tangential + slight gust oscillation
        byte gust = (storm_tick >> 3) & 7;
        cyc_wind_vx = (sbyte)(((int)CYC_DX[gust] * str) >> 2);
        cyc_wind_vy = (sbyte)(((int)CYC_DY[gust] * str) >> 2);

        // Extreme proximity (inside Cyclone Eye!) slams chopper downward!
        if (cyc_wind_force >= 15 && (storm_tick & 3) == 0) {
            if (heli_alt > 4) heli_alt--;
            if (heli_alt <= (byte)(heli_ground_alt + 2)) {
                heli_force_crash();
            }
        }
    } else {
        cyc_wind_vx = 0;
        cyc_wind_vy = 0;
    }

    // 2. Dynamic C64 Storm Atmosphere (Palette Darkening + Lightning Flashes!)
    if (lightning_tmr > 0) {
        lightning_tmr--;
        r_pf_bg  = (lightning_tmr & 1) ? VCOL_WHITE : VCOL_LT_GREY;
        r_pf_mc1 = VCOL_WHITE;
        r_pf_mc2 = VCOL_LT_GREY;
    } else if (cyc_wind_force >= 13) {
        // Raging Cyclone Tempest: dark stormy sea & overcast terrain!
        r_pf_bg  = VCOL_BLACK;
        r_pf_mc1 = VCOL_GREEN;
        r_pf_mc2 = VCOL_DARK_GREY;
        if (lightning_cd == 0) {
            lightning_tmr = 3;
            lightning_cd  = (byte)(65 + (next_rng() & 63));
            last_sfx_event = EVT_THUNDER;
        } else {
            lightning_cd--;
        }
    } else if (cyc_wind_force >= 9) {
        // Approaching Tropical Squall: slightly deeper sky & darker cliff shadows
        r_pf_bg  = PF_BG_DEFAULT;
        r_pf_mc1 = PF_MC1_DEFAULT;
        r_pf_mc2 = VCOL_DARK_GREY;
    } else {
        // Clear sunny tropical weather
        r_pf_bg  = PF_BG_DEFAULT;
        r_pf_mc1 = PF_MC1_DEFAULT;
        r_pf_mc2 = PF_MC2_DEFAULT;
    }

    // 3. Hardware Sprites 4 & 5: Swirling Cyclone Funnel, Driving Rain, or Low-Flying Jet!
    byte spr_mask = 0;
    vic.spr_expand_x &= 0xcf; // Clear expand on sprites 4 & 5 by default
    vic.spr_expand_y &= 0xcf;

    int cx_scr, cy_scr;
    if (project_hazard(cyc_px, cyc_py, 16, south_view, &cx_scr, &cy_scr)) {
        // Massive 2x-expanded swirling Red/White/Black Cyclone Funnel on Sprite 4!
        byte f = (storm_tick >> 2) & 3;
        set_spr_ptr_both(4, (byte)(SPR_PTR_CYC_BASE + f));
        vic.spr_multi    |= 0x10;
        vic.spr_expand_x |= 0x10;
        vic.spr_expand_y |= 0x10;
        vic.spr_color[4]  = VCOL_RED;
        vic_sprxy(4, cx_scr - 12, cy_scr - 10);
        spr_mask |= 0x10;
    }

    // Driving diagonal storm rain streaks on Sprite 5 when wind_force >= 10!
    if (cyc_wind_force >= 10 && !jet_active) {
        byte rf = (storm_tick >> 2) & 1;
        set_spr_ptr_both(5, (byte)(SPR_PTR_RAIN_BASE + rf));
        vic.spr_multi    &= (byte)~0x20;
        vic.spr_expand_x &= (byte)~0x20; // Crisp 1px-thin diagonal streaks!
        vic.spr_expand_y |= 0x20;        // Stretched vertically for sweeping rain
        vic.spr_color[5]  = VCOL_CYAN;
        int rx = 60 + (int)((storm_tick * 5) & 0x7f);
        int ry = 70 + (int)((storm_tick * 3) & 0x3f);
        vic_sprxy(5, rx, ry);
        spr_mask |= 0x20;
    }

    // 4. Low-Flying Aircraft Hazard over Open Water (uses Sprites 4 & 5 when funnel not visible)
    if (jet_active) {
        // Move fast (3 px/frame) along heading
        if (jet_dir == 0)      jet_px += 3;
        else if (jet_dir == 1) jet_px -= 3;
        else if (jet_dir == 2) jet_py -= 3;
        else                   jet_py += 3;

        if (jet_timer > 0) jet_timer--;
        if (jet_timer == 0) {
            jet_active   = 0;
            jet_cooldown = (byte)(140 + (next_rng() & 63));
        } else {
            // 3D mid-air collision check with helicopter!
            if (heli_state == HELI_STATE_FLYING) {
                int jdx = heli_px - jet_px; if (jdx < 0) jdx = -jdx;
                int jdy = heli_py - jet_py; if (jdy < 0) jdy = -jdy;
                int jda = (int)heli_alt - (int)jet_alt; if (jda < 0) jda = -jda;
                if (jdx < 14 && jdy < 14 && jda < 10) {
                    jet_active = 0;
                    heli_force_crash();
                    return spr_mask;
                }
            }

            // Render Low-Flying Jet (Sprite 4) + Ground Shadow (Sprite 5) if Cyclone funnel isn't on 4
            int jx, jy, sx, sy;
            if (!(spr_mask & 0x10) && project_hazard(jet_px, jet_py, jet_alt, south_view, &jx, &jy)) {
                byte vdir = jet_dir;
                if (south_view) vdir ^= 1; // Flip E<->W, N<->S in South View!
                set_spr_ptr_both(4, (byte)(SPR_PTR_JET_BASE + vdir));
                vic.spr_multi    |= 0x10;
                vic.spr_expand_x &= (byte)~0x10;
                vic.spr_expand_y &= (byte)~0x10;
                vic.spr_color[4]  = VCOL_CYAN;
                vic_sprxy(4, jx, jy);
                spr_mask |= 0x10;

                if (project_hazard(jet_px, jet_py, 0, south_view, &sx, &sy)) {
                    set_spr_ptr_both(5, (byte)(SPR_PTR_SHAD_BASE + (vdir == 0 ? 4 : (vdir == 1 ? 12 : (vdir == 2 ? 0 : 8)))));
                    vic.spr_multi    &= (byte)~0x20;
                    vic.spr_expand_x &= (byte)~0x20;
                    vic.spr_expand_y &= (byte)~0x20;
                    vic.spr_color[5]  = VCOL_DARK_GREY;
                    vic_sprxy(5, sx, sy);
                    spr_mask |= 0x20;
                }
            }
        }
    } else if (over_sea && game_state == GAME_PLAYING && heli_state == HELI_STATE_FLYING && cyc_wind_force < 12) {
        if (jet_cooldown > 0) {
            jet_cooldown--;
        } else {
            spawn_jet();
        }
    }

    return spr_mask;
}

#pragma code(code)
#pragma data(data)
