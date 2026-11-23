#ifndef CYCLONE_RASTER_H
#define CYCLONE_RASTER_H

#include <c64/types.h>

// Shared volatile state latched cleanly into the Top-of-Frame Raster IRQ
extern volatile byte r_pf_xscroll;   // 0..7
extern volatile byte r_pf_yscroll;   // 0..7
extern volatile byte r_pf_memptr;    // MEMPTR_A_NORTH / B_NORTH / A_SOUTH / B_SOUTH
extern volatile byte r_pf_bg;        // Playfield background color ($D021)
extern volatile byte r_pf_mc1;       // Playfield MC1 ($D022)
extern volatile byte r_pf_mc2;       // Playfield MC2 ($D023)
extern volatile byte r_border_prof;  // Debug raster-time border bar toggle

// Synchronisation flags set by IRQ chain
extern volatile byte r_playfield_done; // Set at scanline 200 (safe to blit Color RAM!)
extern volatile byte r_frame_count;    // Incremented every frame at scanline 218

void raster_init(void);
void raster_commit_playfield(void);
void raster_wait_playfield_done(void);
void raster_wait_frame(void);

#endif
