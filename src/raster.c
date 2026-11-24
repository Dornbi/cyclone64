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
volatile byte r_spr_enable   = 0x00;
volatile byte r_border_prof  = 0;

volatile byte r_playfield_done = 0;
volatile byte r_frame_count    = 0;

// 5-stage raster IRQ chain for rock-solid Multicolor Scroll <-> Hi-Res HUD split
static RIRQCode10 irq_top;     // Line 40: Apply playfield scroll, mode, screen/charset, palette & sprites
static RIRQCode   irq_bezel1;  // Line 200: Enter pure-black ECM+BMM bezel, hide sprites, signal playfield_done
static RIRQCode   irq_bezel2;  // Line 207: Set yscroll=7 to let row 19 finish cleanly (VCBASE=800)
static RIRQCode   irq_bezel3;  // Line 214: Set yscroll=3 & pre-blacken $D021
static RIRQCode10 irq_hud;     // Line 218: Switch to 40-col Hi-Res Cockpit HUD (starts line 219)

static void on_playfield_done(void) {
    r_playfield_done = 1;
}

static void on_hud_start(void) {
    r_frame_count++;
}

// Exact FLD bezel timing tables for 18-row playfield (lines 50..191):
// Note: Oscar64 rirq_set(slot, ROW, ...) loops `cmp $d012 / bcs -5`, so its register writes
// execute at the start of line (ROW + 1). Every entry below writes at least 1 FULL scanline
// BEFORE the next unwanted badline, guaranteeing exact VCBASE == 800, RC == 0 at line 219
// (all 4 HUD rows 219..250, full 8px height!) for ALL yscroll = 0..7 on BOTH PAL and NTSC!
//   ys 0..3: Row 19 = 200..203 -> L2=206 (writes 207: $76), L3=212 (writes 213: $73+BLK) -> 219!
//   ys 4..5: Row 19 = 204..205 -> L2=210 (writes 211: $72), L3=213 (writes 214: $73+BLK) -> 219!
//   ys 6..7: Row 19 = 206..207 -> L2=211 (writes 212: $73), L3=214 (writes 215: $73+BLK) -> 219!
static const byte FLD_L2[8] = { 206, 206, 206, 206, 210, 210, 211, 211 };
static const byte FLD_V2[8] = { 0x76, 0x76, 0x76, 0x76, 0x72, 0x72, 0x73, 0x73 };
static const byte FLD_L3[8] = { 212, 212, 212, 212, 213, 213, 214, 214 };

static byte last_fld_ys = 0xff;

void raster_commit_playfield(void) {
    byte ys = r_pf_yscroll & 7;
    // Always wait until the last IRQ of the frame (line 218) has finished before touching rirq_sort!
    rirq_wait_done();

    rirq_data(&irq_top.c, 0, (byte)(0x10 | ys));                 // 24-row, DEN=1, Y-scroll
    rirq_data(&irq_top.c, 1, (byte)(0x10 | (r_pf_xscroll & 7))); // 38-col, MCM=1, X-scroll
    rirq_data(&irq_top.c, 2, r_pf_memptr);
    rirq_data(&irq_top.c, 3, r_pf_bg);
    rirq_data(&irq_top.c, 4, r_pf_mc1);
    rirq_data(&irq_top.c, 5, r_pf_mc2);
    rirq_data(&irq_top.c, 6, r_spr_enable);

    // Bezel1 preserves playfield yscroll while engaging ECM+BMM black mask ($70 | ys)
    rirq_data(&irq_bezel1, 0, (byte)(0x70 | ys));

    // Only re-sort FLD IRQ positions when yscroll actually changed!
    if (ys != last_fld_ys) {
        last_fld_ys = ys;
        rirq_data(&irq_bezel2, 0, FLD_V2[ys]);
        rirq_move(2, FLD_L2[ys]);
        rirq_move(3, FLD_L3[ys]);
        rirq_sort();
    }
}

void raster_setup_bank3(void) {
    // Disable CIA timer interrupts (also ensures SEI before MMAP_RAM sprite unpacking!)
    cia_init();
    mmap_set(MMAP_NO_ROM);

    // Select VIC-II Bank 3 ($C000 - $FFFF)
    vic_setbank(3);
    vic.spr_enable = 0x00;
}

void raster_start_irqs(void) {
    // Initialize Oscar64's direct-RAM-vector raster IRQ manager ($FFFE/$FFFF)
    rirq_init_io();

    // IRQ 0 @ line 46 (well before playfield at line 50, maximizes VBlank time on PAL & NTSC!)
    rirq_build(&irq_top.c, 7);
    rirq_write(&irq_top.c, 0, (void *)&vic.ctrl1,       0x10);
    rirq_write(&irq_top.c, 1, (void *)&vic.ctrl2,       0x10);
    rirq_write(&irq_top.c, 2, (void *)&vic.memptr,      MEMPTR_A_NORTH);
    rirq_write(&irq_top.c, 3, (void *)&vic.color_back,  PF_BG_DEFAULT);
    rirq_write(&irq_top.c, 4, (void *)&vic.color_back1, PF_MC1_DEFAULT);
    rirq_write(&irq_top.c, 5, (void *)&vic.color_back2, PF_MC2_DEFAULT);
    rirq_write(&irq_top.c, 6, (void *)&vic.spr_enable,  0x00);

    // IRQ 1 @ line 200 (fires at start of line 200): black ECM+BMM bezel, clip sprites, signal main
    rirq_build(&irq_bezel1, 3);
    rirq_write(&irq_bezel1, 0, (void *)&vic.ctrl1,      0x70);
    rirq_write(&irq_bezel1, 1, (void *)&vic.spr_enable, 0x00);
    rirq_call(&irq_bezel1,  2, (void *)on_playfield_done);

    // IRQ 2 @ line 208 (fires at start of line 208, before cycle 14!): set yscroll=7 so row 19
    // finishes RC=7 cleanly across 208..214 -> VCBASE=800 for ALL playfield yscroll 0..7!
    rirq_build(&irq_bezel2, 1);
    rirq_write(&irq_bezel2, 0, (void *)&vic.ctrl1, 0x77);

    // IRQ 3 @ line 215 (fires at start of line 215, before cycle 14!): set yscroll=3 & black $D021
    rirq_build(&irq_bezel3, 2);
    rirq_write(&irq_bezel3, 0, (void *)&vic.ctrl1,      0x73);
    rirq_write(&irq_bezel3, 1, (void *)&vic.color_back, VCOL_BLACK);

    // IRQ 4 @ line 217 (2 lines before exact HUD badline at 219!):
    // Switch cleanly to 40-col Hi-Res Cockpit HUD at exact VCBASE = 800 ($CB20), pure black BG
    rirq_build(&irq_hud.c, 5);
    rirq_write(&irq_hud.c, 0, (void *)&vic.ctrl2,      0x08);       // 40-col, Hi-Res, xs=0
    rirq_write(&irq_hud.c, 1, (void *)&vic.memptr,     MEMPTR_HUD); // $C800 + $F000
    rirq_write(&irq_hud.c, 2, (void *)&vic.color_back, VCOL_BLACK); // Always pure black HUD
    rirq_write(&irq_hud.c, 3, (void *)&vic.ctrl1,      0x1b);       // 25-row, Hi-Res text, ys=3
    rirq_call(&irq_hud.c,  4, (void *)on_hud_start);

    rirq_set(0, 45,  &irq_top.c);
    rirq_set(1, 191, &irq_bezel1);
    rirq_set(2, 206, &irq_bezel2);
    rirq_set(3, 212, &irq_bezel3);
    rirq_set(4, 216, &irq_hud.c);

    last_fld_ys = 0xff;
    byte ys = r_pf_yscroll & 7;
    rirq_data(&irq_top.c, 0, (byte)(0x10 | ys));
    rirq_data(&irq_bezel1, 0, (byte)(0x70 | ys));
    rirq_data(&irq_bezel2, 0, FLD_V2[ys]);
    rirq_move(2, FLD_L2[ys]);
    rirq_move(3, FLD_L3[ys]);
    last_fld_ys = ys;
    // Wait for bottom border (>= 256) before enabling IRQs so first irq_top fires cleanly at line 46!
    vic_waitFrame();
    vic.ctrl1 &= 0x7f;
    rirq_sort();
    __asm {
        asl $d019
        cli
    }
}

void raster_pause_split(void) {
    rirq_stop();
    vic.spr_enable = 0x00;
    vic.ctrl1      = 0x1b;        // Standard 25-row Hi-Res text
    vic.ctrl2      = 0x08;        // 40-col Hi-Res
    vic.memptr     = MEMPTR_HUD;  // Screen $C800 + Charset $F000
    vic.color_border = VCOL_BLACK;
    vic.color_back   = VCOL_BLUE;
}

void raster_resume_split(void) {
    vic.spr_enable = 0x00;
    last_fld_ys = 0xff;
    byte ys = r_pf_yscroll & 7;
    rirq_data(&irq_top.c, 0, (byte)(0x10 | ys));
    rirq_data(&irq_top.c, 1, (byte)(0x10 | (r_pf_xscroll & 7)));
    rirq_data(&irq_top.c, 2, r_pf_memptr);
    rirq_data(&irq_top.c, 3, r_pf_bg);
    rirq_data(&irq_top.c, 4, r_pf_mc1);
    rirq_data(&irq_top.c, 5, r_pf_mc2);
    rirq_data(&irq_top.c, 6, r_spr_enable);
    rirq_data(&irq_bezel1, 0, (byte)(0x70 | ys));
    rirq_data(&irq_bezel2, 0, FLD_V2[ys]);
    rirq_move(2, FLD_L2[ys]);
    rirq_move(3, FLD_L3[ys]);
    last_fld_ys = ys;
    // Wait cleanly for bottom border (> 255), clear $D011 bit 7, set exact line 45 via rirq_sort(),
    // and enable IRQs WITHOUT calling rirq_start() (which overwrites $D012 = 100)!
    vic_waitFrame();
    vic.ctrl1 &= 0x7f;
    rirq_sort();
    __asm {
        asl $d019
        cli
    }
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
