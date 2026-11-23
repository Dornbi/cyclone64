#ifndef CYCLONE_HELICOPTER_H
#define CYCLONE_HELICOPTER_H

#include <c64/types.h>

#define HELI_STATE_LANDED    0
#define HELI_STATE_FLYING    1
#define HELI_STATE_CRASHING  2

#define CTRL_CLASSIC         0   // Rotate + Throttle + Fire+U/D Alt (authentic Cyclone!)
#define CTRL_ARCADE          1   // Direct 8-way steering + Fire+U/D Alt

#define HELI_MAX_ALT         64  // Clears even height-8 spires (56px) with 8px headroom
#define HELI_MAX_SPEED       48  // Smooth inertial cruising speed

// Helicopter world & flight state
extern int  heli_px;          // World pixel X (0..6143)
extern int  heli_py;          // World pixel Y (0..6143)
extern byte heli_alt;         // Altitude above sea level in pixels (0..64)
extern byte heli_ground_alt;  // Ground surface altitude directly beneath chopper (0..56)
extern byte heli_heading;     // 0..15 (CW from North, 22.5 deg steps)
extern byte heli_speed;       // 0..HELI_MAX_SPEED
extern byte heli_state;       // HELI_STATE_*
extern byte heli_lives;       // Remaining helicopters (starts at 3)
extern byte heli_ctrl_mode;   // CTRL_CLASSIC or CTRL_ARCADE
extern byte heli_surface;     // SURF_* directly beneath helicopter

// Initialize helicopter hardware sprites & place on Base Island's western helipad
void heli_init(void);

// Respawn helicopter safely on Base Island helipad (keeping remaining lives)
void heli_respawn_at_base(void);

// Warp helicopter safely above a given island (Phase 2 inspection shortcut)
void heli_warp_to_island(byte island_id);

// Per-frame physics, input processing, 3D collision & smooth camera follow
// Returns pixel deltas (out_dpx, out_dpy) for scroller_move()
void heli_update(sbyte joy_dx, sbyte joy_dy, byte fire_held,
                 sbyte direct_alt_cmd, byte south_view,
                 sbyte *out_dpx, sbyte *out_dpy);

// Update hardware sprite positions & animation pointers (Sprites 0, 1, 2)
void heli_sync_sprites(byte south_view);

// Short 3-char compass heading label ("N  ", "NNE", "NE ", ...)
const char *heli_heading_str(void);

#endif
