#include "memmap.h"
#include "raster.h"
#include <c64/vic.h>
#include <c64/cia.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>

volatile byte r_pf_xscroll   = 0;
volatile byte r_pf_yscroll   = 0;
volatile byte r_pf_memptr    = MEMPTR_A_NORTH;
volatile byte r_pf_bg        = PF_BG_DEFAULT;
volatile byte r_pf_mc1       = PF_MC1_DEFAULT;
volatile byte r_pf_mc2       = PF_MC2_DEFAULT;
volatile byte r_border_prof  = 0;

volatile byte r_playfield_done = 0;
volatile byte r_frame_count    = 0;

// 5-stage raster IRQ chain for rock-solid Multicolor Scroll <-> Hi-Res HUD split
static RIRQCode10 irq_top;     // Line 40: Apply playfield scroll, mode, screen/charset & palette
static RIRQCode   irq_bezel1;  // Line 200: Enter pure-black ECM+BMM bezel, signal playfield_done
static RIRQCode   irq_bezel2;  // Line 208: Set yscroll=7 to let row 19 finish cleanly (VCBASE=800)
static RIRQCode   irq_bezel3;  // Line 214: Set yscroll=3, metallic bezel trim bar
static RIRQCode10 irq_hud;     // Line 218: Switch to 40-col Hi-Res Cockpit HUD (starts line 219)

static void on_playfield_done(void) {
    r_playfield_done = 1;
}

static void on_hud_start(void) {
    r_frame_count++;
}

void raster_commit_playfield(void) {
    // Update the self-modifying IRQ write values atomically for next frame
    rirq_data(&irq_top.c, 0, (byte)(0x10 | (r_pf_yscroll & 7))); // 24-row, DEN=1, Y-scroll
    rirq_data(&irq_top.c, 1, (byte)(0x10 | (r_pf_xscroll & 7))); // 38-col, MCM=1, X-scroll
    rirq_data(&irq_top.c, 2, r_pf_memptr);
    rirq_data(&irq_top.c, 3, r_pf_bg);
    rirq_data(&irq_top.c, 4, r_pf_mc1);
    rirq_data(&irq_top.c, 5, r_pf_mc2);

    // Bezel1 preserves playfield yscroll while engaging ECM+BMM black mask ($70 | ys)
    rirq_data(&irq_bezel1, 0, (byte)(0x70 | (r_pf_yscroll & 7)));
}

void raster_init(void) {
    // Disable CIA timer interrupts and bank out BASIC + KERNAL (keeping I/O at $D000-$DFFF)
    cia_init();
    mmap_set(MMAP_NO_ROM);

    // Select VIC-II Bank 3 ($C000 - $FFFF)
    vic_setbank(3);

    // Initialize Oscar64's direct-RAM-vector raster IRQ manager ($FFFE/$FFFF)
    rirq_init_io();

    // IRQ 0 @ line 40 (fires on line 41, well before line 50)
    rirq_build(&irq_top.c, 6);
    rirq_write(&irq_top.c, 0, (void *)&vic.ctrl1,       0x10);
    rirq_write(&irq_top.c, 1, (void *)&vic.ctrl2,       0x10);
    rirq_write(&irq_top.c, 2, (void *)&vic.memptr,      MEMPTR_A_NORTH);
    rirq_write(&irq_top.c, 3, (void *)&vic.color_back,  PF_BG_DEFAULT);
    rirq_write(&irq_top.c, 4, (void *)&vic.color_back1, PF_MC1_DEFAULT);
    rirq_write(&irq_top.c, 5, (void *)&vic.color_back2, PF_MC2_DEFAULT);

    // IRQ 1 @ line 199 (fires on line 200): engage black ECM+BMM bezel + signal main loop
    rirq_build(&irq_bezel1, 2);
    rirq_write(&irq_bezel1, 0, (void *)&vic.ctrl1, 0x70);
    rirq_call(&irq_bezel1,  1, (void *)on_playfield_done);

    // IRQ 2 @ line 206 (fires ~cycle 30 of line 207): set yscroll=7 so row 19 finishes
    // RC=7 cleanly without triggering any badline on 208..214 for all playfield yscroll 0..7!
    rirq_build(&irq_bezel2, 1);
    rirq_write(&irq_bezel2, 0, (void *)&vic.ctrl1, 0x77);

    // IRQ 3 @ line 213 (fires ~cycle 30 of line 214): set yscroll=3 & pre-blacken $D021
    // (guarantees pure black idle lines before HUD badline at 219 even with Bank 3 $FFFF IRQ vector!)
    rirq_build(&irq_bezel3, 2);
    rirq_write(&irq_bezel3, 0, (void *)&vic.ctrl1,      0x73);
    rirq_write(&irq_bezel3, 1, (void *)&vic.color_back, VCOL_BLACK);

    // IRQ 4 @ line 217 (fires on line 218, right before HUD badline at 219!):
    // Switch cleanly to 40-col Hi-Res Cockpit HUD at exact VCBASE = 800 ($CB20)
    rirq_build(&irq_hud.c, 4);
    rirq_write(&irq_hud.c, 0, (void *)&vic.ctrl2,  0x08);       // 40-col, Hi-Res, xs=0
    rirq_write(&irq_hud.c, 1, (void *)&vic.memptr, MEMPTR_HUD); // $C800 + $F000
    rirq_write(&irq_hud.c, 2, (void *)&vic.ctrl1,  0x1b);       // 25-row, Hi-Res text, ys=3
    rirq_call(&irq_hud.c,  3, (void *)on_hud_start);

    rirq_set(0, 40,  &irq_top.c);
    rirq_set(1, 199, &irq_bezel1);
    rirq_set(2, 206, &irq_bezel2);
    rirq_set(3, 213, &irq_bezel3);
    rirq_set(4, 217, &irq_hud.c);

    raster_commit_playfield();
    rirq_sort();
    rirq_start();
}

void raster_wait_playfield_done(void) {
    r_playfield_done = 0;
    while (!r_playfield_done) {
    }
}

void raster_wait_frame(void) {
    byte cur = r_frame_count;
    while (r_frame_count == cur) {
    }
}
