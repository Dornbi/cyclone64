#ifndef CYCLONE_STORM_H
#define CYCLONE_STORM_H

#include <c64/types.h>

extern int   cyc_px;         // Cyclone eye world pixel X (0..6143)
extern int   cyc_py;         // Cyclone eye world pixel Y (0..6143)
extern byte  cyc_wind_force; // Authentic Cyclone wind gauge (1..16, >=11 = DANGER!)
extern sbyte cyc_wind_vx;    // Aerodynamic drift X applied to helicopter
extern sbyte cyc_wind_vy;    // Aerodynamic drift Y applied to helicopter
extern byte  jet_active;     // 1 while low-flying aircraft is crossing screen

// Initialize Cyclone storm & low-flying jet hazard for a new mission
void cyclone_init(byte seed);

// Advance Cyclone AI only (called live even while viewing Tactical Radar Map!)
void cyclone_tick_ai(void);

// Per-frame Cyclone weather FX, wind drift, funnel/rain sprites, and low-flying jets
// Returns hardware sprite enable bits for Sprites 4 & 5
byte cyclone_update_and_draw(byte south_view, byte over_sea);

#endif
