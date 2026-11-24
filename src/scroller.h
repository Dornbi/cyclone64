#ifndef CYCLONE_SCROLLER_H
#define CYCLONE_SCROLLER_H

#include <c64/types.h>

extern int  sc_cam_px;     // Camera pixel X (top-left of viewport, 0..6143)
extern int  sc_cam_py;     // Camera pixel Y (top-left of viewport, 0..6143)
extern byte sc_south_view; // 0 = North View, 1 = South View (180 deg flip)

// Initialize camera centered on world tile (center_wx, center_wy) and render full viewport
void scroller_init(int center_wx, int center_wy);

// Move camera by pixel delta (dpx, dpy in screen space, -4..+4 per frame)
// Performs double-buffered screen shift + VBlank Color RAM blit seamlessly
void scroller_move(sbyte dpx, sbyte dpy);

// Instantly toggle North <-> South 180-degree perspective around current viewport center
void scroller_toggle_view(void);

// Full viewport redraw (used on init, view flip, or returning from Tactical Map)
void scroller_redraw_full(void);

// Subtle shimmering wave animation on ocean chars 0..3 (call once per frame)
void scroller_animate_water(void);

// Current world tile at the center of the playfield
int scroller_center_wx(void);
int scroller_center_wy(void);

#ifdef TEST_SCENARIO
// On-target test helper: verifies every playfield cell's Color RAM matches CHAR_COLORS[front[i]]
byte scroller_verify_color_ram(void);
#endif

#endif
