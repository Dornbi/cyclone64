#include "memmap.h"
#include "world.h"
#include "raster.h"
#include "scroller.h"
#include <c64/vic.h>
#include <string.h>

int  sc_cam_px     = 0;
int  sc_cam_py     = 0;
byte sc_south_view = 0;

static int  cur_tile_x = 0;
static int  cur_tile_y = 0;
static byte active_buf = 0; // 0 = SCREEN_A front, 1 = SCREEN_B front
static byte water_tick = 0;

static inline byte *front_screen(void) {
    return active_buf ? SCREEN_B : SCREEN_A;
}

static inline byte *back_screen(void) {
    return active_buf ? SCREEN_A : SCREEN_B;
}

static inline byte make_memptr(byte buf, byte south) {
    if (!south) return buf ? MEMPTR_B_NORTH : MEMPTR_A_NORTH;
    return buf ? MEMPTR_B_SOUTH : MEMPTR_A_SOUTH;
}

static byte calc_xscroll(void) {
    return sc_south_view ? (byte)(sc_cam_px & 7) : (byte)(7 - (sc_cam_px & 7));
}

static byte calc_yscroll(void) {
    return sc_south_view ? (byte)(sc_cam_py & 7) : (byte)(7 - (sc_cam_py & 7));
}

// Fast 760-byte directional copy from src -> dst with offset (dx, dy in -1..+1)
static void shift_screen_buf(byte *dst, const byte *src, sbyte dx, sbyte dy) {
    // Source offset = dy*40 + dx
    // Copy overlapping region cleanly; edges will be overwritten right after.
    int off = (int)dy * PF_COLS + (int)dx;
    if (off > 0) {
        // Shift left / up: forward copy
        unsigned int count = (unsigned int)(PF_CELLS - off);
        memcpy(dst, src + off, count);
    } else if (off < 0) {
        // Shift right / down: backward-safe copy (different buffers so memcpy is always safe!)
        unsigned int uoff = (unsigned int)(-off);
        memcpy(dst + uoff, src, (unsigned int)(PF_CELLS - uoff));
    } else {
        memcpy(dst, src, PF_CELLS);
    }
}

// Blazing-fast 18-row unrolled Color RAM shift ($D800..$DACF, 720 bytes)
// Unrolling all 18 rows inside a single 40-column loop runs in ~6,600 cycles (~103 scanlines),
// finishing cleanly inside the scanline 192..46 VBlank window on BOTH PAL and NTSC!
// CRITICAL: When vdx < 0 (off == 39 or -41, shifting pixels RIGHT so src_x = x - 1),
// x MUST iterate BACKWARD (39 down to 1) so (row, x-1) is read BEFORE it is overwritten!
// When vdx > 0 (off == 41 or -39, shifting pixels LEFT so src_x = x + 1),
// x MUST iterate FORWARD (0 up to 38) so (row, x+1) is read BEFORE it is overwritten!
#define SHIFT_UP_FWD(OFF, XEND) \
    do { \
        for (byte x = 0; x < (XEND); x++) { \
            COLOR_RAM[ 0*40 + x] = COLOR_RAM[ 0*40 + (OFF) + x]; \
            COLOR_RAM[ 1*40 + x] = COLOR_RAM[ 1*40 + (OFF) + x]; \
            COLOR_RAM[ 2*40 + x] = COLOR_RAM[ 2*40 + (OFF) + x]; \
            COLOR_RAM[ 3*40 + x] = COLOR_RAM[ 3*40 + (OFF) + x]; \
            COLOR_RAM[ 4*40 + x] = COLOR_RAM[ 4*40 + (OFF) + x]; \
            COLOR_RAM[ 5*40 + x] = COLOR_RAM[ 5*40 + (OFF) + x]; \
            COLOR_RAM[ 6*40 + x] = COLOR_RAM[ 6*40 + (OFF) + x]; \
            COLOR_RAM[ 7*40 + x] = COLOR_RAM[ 7*40 + (OFF) + x]; \
            COLOR_RAM[ 8*40 + x] = COLOR_RAM[ 8*40 + (OFF) + x]; \
            COLOR_RAM[ 9*40 + x] = COLOR_RAM[ 9*40 + (OFF) + x]; \
            COLOR_RAM[10*40 + x] = COLOR_RAM[10*40 + (OFF) + x]; \
            COLOR_RAM[11*40 + x] = COLOR_RAM[11*40 + (OFF) + x]; \
            COLOR_RAM[12*40 + x] = COLOR_RAM[12*40 + (OFF) + x]; \
            COLOR_RAM[13*40 + x] = COLOR_RAM[13*40 + (OFF) + x]; \
            COLOR_RAM[14*40 + x] = COLOR_RAM[14*40 + (OFF) + x]; \
            COLOR_RAM[15*40 + x] = COLOR_RAM[15*40 + (OFF) + x]; \
            COLOR_RAM[16*40 + x] = COLOR_RAM[16*40 + (OFF) + x]; \
        } \
    } while (0)

#define SHIFT_UP_BWD(OFF) \
    do { \
        for (byte x = 39; x >= 1; x--) { \
            COLOR_RAM[ 0*40 + x] = COLOR_RAM[ 0*40 + (OFF) + x]; \
            COLOR_RAM[ 1*40 + x] = COLOR_RAM[ 1*40 + (OFF) + x]; \
            COLOR_RAM[ 2*40 + x] = COLOR_RAM[ 2*40 + (OFF) + x]; \
            COLOR_RAM[ 3*40 + x] = COLOR_RAM[ 3*40 + (OFF) + x]; \
            COLOR_RAM[ 4*40 + x] = COLOR_RAM[ 4*40 + (OFF) + x]; \
            COLOR_RAM[ 5*40 + x] = COLOR_RAM[ 5*40 + (OFF) + x]; \
            COLOR_RAM[ 6*40 + x] = COLOR_RAM[ 6*40 + (OFF) + x]; \
            COLOR_RAM[ 7*40 + x] = COLOR_RAM[ 7*40 + (OFF) + x]; \
            COLOR_RAM[ 8*40 + x] = COLOR_RAM[ 8*40 + (OFF) + x]; \
            COLOR_RAM[ 9*40 + x] = COLOR_RAM[ 9*40 + (OFF) + x]; \
            COLOR_RAM[10*40 + x] = COLOR_RAM[10*40 + (OFF) + x]; \
            COLOR_RAM[11*40 + x] = COLOR_RAM[11*40 + (OFF) + x]; \
            COLOR_RAM[12*40 + x] = COLOR_RAM[12*40 + (OFF) + x]; \
            COLOR_RAM[13*40 + x] = COLOR_RAM[13*40 + (OFF) + x]; \
            COLOR_RAM[14*40 + x] = COLOR_RAM[14*40 + (OFF) + x]; \
            COLOR_RAM[15*40 + x] = COLOR_RAM[15*40 + (OFF) + x]; \
            COLOR_RAM[16*40 + x] = COLOR_RAM[16*40 + (OFF) + x]; \
        } \
    } while (0)

#define SHIFT_DN_FWD(UOFF, XEND) \
    do { \
        for (byte x = 0; x < (XEND); x++) { \
            COLOR_RAM[17*40 + x] = COLOR_RAM[17*40 - (UOFF) + x]; \
            COLOR_RAM[16*40 + x] = COLOR_RAM[16*40 - (UOFF) + x]; \
            COLOR_RAM[15*40 + x] = COLOR_RAM[15*40 - (UOFF) + x]; \
            COLOR_RAM[14*40 + x] = COLOR_RAM[14*40 - (UOFF) + x]; \
            COLOR_RAM[13*40 + x] = COLOR_RAM[13*40 - (UOFF) + x]; \
            COLOR_RAM[12*40 + x] = COLOR_RAM[12*40 - (UOFF) + x]; \
            COLOR_RAM[11*40 + x] = COLOR_RAM[11*40 - (UOFF) + x]; \
            COLOR_RAM[10*40 + x] = COLOR_RAM[10*40 - (UOFF) + x]; \
            COLOR_RAM[ 9*40 + x] = COLOR_RAM[ 9*40 - (UOFF) + x]; \
            COLOR_RAM[ 8*40 + x] = COLOR_RAM[ 8*40 - (UOFF) + x]; \
            COLOR_RAM[ 7*40 + x] = COLOR_RAM[ 7*40 - (UOFF) + x]; \
            COLOR_RAM[ 6*40 + x] = COLOR_RAM[ 6*40 - (UOFF) + x]; \
            COLOR_RAM[ 5*40 + x] = COLOR_RAM[ 5*40 - (UOFF) + x]; \
            COLOR_RAM[ 4*40 + x] = COLOR_RAM[ 4*40 - (UOFF) + x]; \
            COLOR_RAM[ 3*40 + x] = COLOR_RAM[ 3*40 - (UOFF) + x]; \
            COLOR_RAM[ 2*40 + x] = COLOR_RAM[ 2*40 - (UOFF) + x]; \
            COLOR_RAM[ 1*40 + x] = COLOR_RAM[ 1*40 - (UOFF) + x]; \
        } \
    } while (0)

#define SHIFT_DN_BWD(UOFF) \
    do { \
        for (byte x = 39; x >= 1; x--) { \
            COLOR_RAM[17*40 + x] = COLOR_RAM[17*40 - (UOFF) + x]; \
            COLOR_RAM[16*40 + x] = COLOR_RAM[16*40 - (UOFF) + x]; \
            COLOR_RAM[15*40 + x] = COLOR_RAM[15*40 - (UOFF) + x]; \
            COLOR_RAM[14*40 + x] = COLOR_RAM[14*40 - (UOFF) + x]; \
            COLOR_RAM[13*40 + x] = COLOR_RAM[13*40 - (UOFF) + x]; \
            COLOR_RAM[12*40 + x] = COLOR_RAM[12*40 - (UOFF) + x]; \
            COLOR_RAM[11*40 + x] = COLOR_RAM[11*40 - (UOFF) + x]; \
            COLOR_RAM[10*40 + x] = COLOR_RAM[10*40 - (UOFF) + x]; \
            COLOR_RAM[ 9*40 + x] = COLOR_RAM[ 9*40 - (UOFF) + x]; \
            COLOR_RAM[ 8*40 + x] = COLOR_RAM[ 8*40 - (UOFF) + x]; \
            COLOR_RAM[ 7*40 + x] = COLOR_RAM[ 7*40 - (UOFF) + x]; \
            COLOR_RAM[ 6*40 + x] = COLOR_RAM[ 6*40 - (UOFF) + x]; \
            COLOR_RAM[ 5*40 + x] = COLOR_RAM[ 5*40 - (UOFF) + x]; \
            COLOR_RAM[ 4*40 + x] = COLOR_RAM[ 4*40 - (UOFF) + x]; \
            COLOR_RAM[ 3*40 + x] = COLOR_RAM[ 3*40 - (UOFF) + x]; \
            COLOR_RAM[ 2*40 + x] = COLOR_RAM[ 2*40 - (UOFF) + x]; \
            COLOR_RAM[ 1*40 + x] = COLOR_RAM[ 1*40 - (UOFF) + x]; \
        } \
    } while (0)

static void shift_color_ram(sbyte dx, sbyte dy) {
    sbyte off = (sbyte)(dy * PF_COLS + dx);
    if (off == 1) {
        for (byte x = 0; x < 39; x++) {
            COLOR_RAM[ 0*40 + x] = COLOR_RAM[ 0*40 + 1 + x];
            COLOR_RAM[ 1*40 + x] = COLOR_RAM[ 1*40 + 1 + x];
            COLOR_RAM[ 2*40 + x] = COLOR_RAM[ 2*40 + 1 + x];
            COLOR_RAM[ 3*40 + x] = COLOR_RAM[ 3*40 + 1 + x];
            COLOR_RAM[ 4*40 + x] = COLOR_RAM[ 4*40 + 1 + x];
            COLOR_RAM[ 5*40 + x] = COLOR_RAM[ 5*40 + 1 + x];
            COLOR_RAM[ 6*40 + x] = COLOR_RAM[ 6*40 + 1 + x];
            COLOR_RAM[ 7*40 + x] = COLOR_RAM[ 7*40 + 1 + x];
            COLOR_RAM[ 8*40 + x] = COLOR_RAM[ 8*40 + 1 + x];
            COLOR_RAM[ 9*40 + x] = COLOR_RAM[ 9*40 + 1 + x];
            COLOR_RAM[10*40 + x] = COLOR_RAM[10*40 + 1 + x];
            COLOR_RAM[11*40 + x] = COLOR_RAM[11*40 + 1 + x];
            COLOR_RAM[12*40 + x] = COLOR_RAM[12*40 + 1 + x];
            COLOR_RAM[13*40 + x] = COLOR_RAM[13*40 + 1 + x];
            COLOR_RAM[14*40 + x] = COLOR_RAM[14*40 + 1 + x];
            COLOR_RAM[15*40 + x] = COLOR_RAM[15*40 + 1 + x];
            COLOR_RAM[16*40 + x] = COLOR_RAM[16*40 + 1 + x];
            COLOR_RAM[17*40 + x] = COLOR_RAM[17*40 + 1 + x];
        }
    } else if (off == -1) {
        for (sbyte x = 38; x >= 0; x--) {
            COLOR_RAM[ 0*40 + 1 + x] = COLOR_RAM[ 0*40 + x];
            COLOR_RAM[ 1*40 + 1 + x] = COLOR_RAM[ 1*40 + x];
            COLOR_RAM[ 2*40 + 1 + x] = COLOR_RAM[ 2*40 + x];
            COLOR_RAM[ 3*40 + 1 + x] = COLOR_RAM[ 3*40 + x];
            COLOR_RAM[ 4*40 + 1 + x] = COLOR_RAM[ 4*40 + x];
            COLOR_RAM[ 5*40 + 1 + x] = COLOR_RAM[ 5*40 + x];
            COLOR_RAM[ 6*40 + 1 + x] = COLOR_RAM[ 6*40 + x];
            COLOR_RAM[ 7*40 + 1 + x] = COLOR_RAM[ 7*40 + x];
            COLOR_RAM[ 8*40 + 1 + x] = COLOR_RAM[ 8*40 + x];
            COLOR_RAM[ 9*40 + 1 + x] = COLOR_RAM[ 9*40 + x];
            COLOR_RAM[10*40 + 1 + x] = COLOR_RAM[10*40 + x];
            COLOR_RAM[11*40 + 1 + x] = COLOR_RAM[11*40 + x];
            COLOR_RAM[12*40 + 1 + x] = COLOR_RAM[12*40 + x];
            COLOR_RAM[13*40 + 1 + x] = COLOR_RAM[13*40 + x];
            COLOR_RAM[14*40 + 1 + x] = COLOR_RAM[14*40 + x];
            COLOR_RAM[15*40 + 1 + x] = COLOR_RAM[15*40 + x];
            COLOR_RAM[16*40 + 1 + x] = COLOR_RAM[16*40 + x];
            COLOR_RAM[17*40 + 1 + x] = COLOR_RAM[17*40 + x];
        }
    } else if (off == 39) {
        SHIFT_UP_BWD(39);
    } else if (off == 40) {
        SHIFT_UP_FWD(40, 40);
    } else if (off == 41) {
        SHIFT_UP_FWD(41, 39);
    } else if (off == -39) {
        SHIFT_DN_FWD(39, 39);
    } else if (off == -40) {
        SHIFT_DN_FWD(40, 40);
    } else if (off == -41) {
        SHIFT_DN_BWD(41);
    }
}

static void render_row_to_buf(byte *scr, byte sy) {
    world_render_row(scr + (unsigned int)sy * PF_COLS, cur_tile_x, cur_tile_y, sy, sc_south_view);
}

static void render_col_to_buf(byte *scr, byte sx) {
    world_render_col(scr, cur_tile_x, cur_tile_y, sx, sc_south_view);
}

static void sync_row_colors(const byte *scr, byte sy) {
    unsigned int off = (unsigned int)sy * PF_COLS;
    const byte *s = scr + off;
    byte *c = COLOR_RAM + off;
    for (byte sx = 0; sx < PF_COLS; sx++) {
        c[sx] = CHAR_COLORS[s[sx]];
    }
}

static void sync_col_colors(const byte *scr, byte sx) {
    const byte *s = scr + sx;
    byte *c = COLOR_RAM + sx;
    for (byte sy = 0; sy < PF_ROWS; sy++) {
        *c = CHAR_COLORS[*s];
        s += PF_COLS;
        c += PF_COLS;
    }
}

void scroller_redraw_full(void) {
    byte *dst = back_screen();
    world_begin_viewport(cur_tile_x, cur_tile_y);
    for (byte sy = 0; sy < PF_ROWS; sy++) {
        world_render_row(dst + (unsigned int)sy * PF_COLS, cur_tile_x, cur_tile_y, sy, sc_south_view);
    }

    // Wait until beam leaves playfield (line 200), then copy all 760 Color RAM cells + flip buffer
    raster_wait_playfield_done();
    if (r_border_prof) vic.color_border = VCOL_RED;

    const byte *s = dst;
    byte *c = COLOR_RAM;
    for (unsigned int i = 0; i < PF_CELLS; i++) {
        c[i] = CHAR_COLORS[s[i]];
    }

    active_buf ^= 1;
    r_pf_xscroll = calc_xscroll();
    r_pf_yscroll = calc_yscroll();
    r_pf_memptr  = make_memptr(active_buf, sc_south_view);
    raster_commit_playfield();

    if (r_border_prof) vic.color_border = VCOL_BLACK;
}

void scroller_init(int center_wx, int center_wy) {
    sc_south_view = 0;
    cur_tile_x = center_wx - (PF_COLS / 2);
    cur_tile_y = center_wy - (PF_ROWS / 2);
    if (cur_tile_x < 0) cur_tile_x = 0;
    if (cur_tile_y < 0) cur_tile_y = 0;
    sc_cam_px = cur_tile_x << 3;
    sc_cam_py = cur_tile_y << 3;

    // Clear both screen buffers initially
    memset(SCREEN_A, 0, 1000);
    memset(SCREEN_B, 0, 1000);
    scroller_redraw_full();
}

void scroller_move(sbyte dpx, sbyte dpy) {
    // Clamp camera inside 768x768 tile archipelago
    int nx = sc_cam_px + (int)dpx;
    int ny = sc_cam_py + (int)dpy;
    int max_px = (768 - PF_COLS - 1) * 8;
    int max_py = (768 - PF_ROWS - 1) * 8;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx > max_px) nx = max_px;
    if (ny > max_py) ny = max_py;

    sc_cam_px = nx;
    sc_cam_py = ny;

    int new_tx = nx >> 3;
    int new_ty = ny >> 3;
    sbyte dx = (sbyte)(new_tx - cur_tile_x);
    sbyte dy = (sbyte)(new_ty - cur_tile_y);

    if (dx == 0 && dy == 0) {
        // Pure sub-tile hardware scroll: just wait for playfield done and latch new fine scroll!
        raster_wait_playfield_done();
        r_pf_xscroll = calc_xscroll();
        r_pf_yscroll = calc_yscroll();
        raster_commit_playfield();
        return;
    }

    // Coarse tile boundary crossed!
    // Step 1: Perform screen buffer shift + 3D edge projection into invisible BACK buffer
    // while VIC-II is busy displaying FRONT buffer!
    cur_tile_x = new_tx;
    cur_tile_y = new_ty;

    // In South View, visual scroll direction is inverted relative to world tile deltas
    sbyte vdx = sc_south_view ? (sbyte)(-dx) : dx;
    sbyte vdy = sc_south_view ? (sbyte)(-dy) : dy;

    byte *front = front_screen();
    byte *back  = back_screen();

    if (vdx >= -1 && vdx <= 1 && vdy >= -1 && vdy <= 1) {
        if (r_border_prof) vic.color_border = VCOL_YELLOW;
        world_begin_viewport(cur_tile_x, cur_tile_y);
        shift_screen_buf(back, front, vdx, vdy);

        // Render newly exposed edge row / column into back buffer
        if (vdy > 0) render_row_to_buf(back, PF_ROWS - 1);
        else if (vdy < 0) render_row_to_buf(back, 0);

        if (vdx > 0) render_col_to_buf(back, PF_COLS - 1);
        else if (vdx < 0) render_col_to_buf(back, 0);

        // Step 2: Wait for scanline 200 (beam exited playfield!), then shift Color RAM
        // and write edge colors + flip buffers atomically for next frame!
        raster_wait_playfield_done();
        if (r_border_prof) vic.color_border = VCOL_RED;

        shift_color_ram(vdx, vdy);
        if (vdy > 0) sync_row_colors(back, PF_ROWS - 1);
        else if (vdy < 0) sync_row_colors(back, 0);

        if (vdx > 0) sync_col_colors(back, PF_COLS - 1);
        else if (vdx < 0) sync_col_colors(back, 0);

        active_buf ^= 1;
        r_pf_xscroll = calc_xscroll();
        r_pf_yscroll = calc_yscroll();
        r_pf_memptr  = make_memptr(active_buf, sc_south_view);
        raster_commit_playfield();

        if (r_border_prof) vic.color_border = VCOL_BLACK;
    } else {
        scroller_redraw_full();
    }
}

void scroller_toggle_view(void) {
    sc_south_view ^= 1;
    scroller_redraw_full();
}

void scroller_animate_water(void) {
    water_tick++;
    if ((water_tick & 7) != 0) return;

    // Gently shift ocean glint pixels 1px right in chars 0..3 of shared tropical charset
    byte *cs = CHARSET_NORTH;
    for (byte i = 0; i < 32; i++) {
        byte b = cs[i];
        if (b) {
            cs[i] = (byte)((b >> 1) | ((b & 1) << 7));
        }
    }
}

int scroller_center_wx(void) {
    return cur_tile_x + (PF_COLS / 2);
}

int scroller_center_wy(void) {
    return cur_tile_y + (PF_ROWS / 2);
}
