#ifndef CYCLONE_MEMMAP_H
#define CYCLONE_MEMMAP_H

#include <c64/types.h>

// ============================================================================
// Cyclone (C64) Stock 64 KB Memory Map (Oscar64 Regions + VIC-II Bank 3)
// ============================================================================
// $0801 - $09FF : startup (Oscar64 boot & init)
// $0A00 - $4FFF : main (Engine code, data, bss, stack)
// $5000 - $9AFF : worldmap (19,200-byte packed 14-island dense tile atlas)
// $9B00 - $BFFF : tables (3D slice LUTs, LZO-compressed charsets & sprites)
//
// VIC-II Bank 3 ($C000 - $FFFF):
// $C000 - $C3FF : Playfield Screen Buffer A (+ sprite ptrs $C3F8-$C3FF)
// $C400 - $C7FF : Playfield Screen Buffer B (+ sprite ptrs $C7F8-$C7FF)
// $C800 - $CBFF : Cockpit HUD & Tactical Map Screen RAM
// $CC00 - $CFFF : Dynamic Sprite Slots (16 sprites)
// $D000 - $DFFF : I/O (CPU) / 64 Static Sprite Frames (VIC-II RAM under I/O)
// $E000 - $E7FF : Playfield Tropical Multicolor Charset (North View)
// $E800 - $EFFF : Playfield Tropical Multicolor Charset (South View)
// $F000 - $F7FF : Hi-Res Cockpit HUD & Tactical Map Charset
// $F800 - $FFFF : Extra Sprite Frames + Hardware IRQ/NMI Vectors

#pragma stacksize(512)
#pragma heapsize(0)

#pragma section(worldmap_sec, 0)
#pragma section(tables_sec, 0)

#pragma region(main, 0x0a00, 0x5000, , , {code, data, bss, stack})
#pragma region(worldmap, 0x5000, 0x9b00, , , {worldmap_sec})
#pragma region(tables, 0x9b00, 0xc000, , , {tables_sec})

// VIC-II Bank 3 pointers
#define SCREEN_A        ((byte *)0xc000)
#define SCREEN_B        ((byte *)0xc400)
#define SCREEN_HUD      ((byte *)0xc800)
#define COLOR_RAM       ((byte *)0xd800)

#define CHARSET_NORTH   ((byte *)0xe000)
#define CHARSET_SOUTH   ((byte *)0xe800)
#define CHARSET_HUD     ((byte *)0xf000)

// VIC-II Bank 3 Sprite RAM under I/O ($D000-$DFFF -> pointers 64..127)
#define SPRITE_RAM_D000 ((byte *)0xd000)
#define SPR_PTRS_A      ((byte *)0xc3f8)
#define SPR_PTRS_B      ((byte *)0xc7f8)

#define SPR_PTR_HELI_BASE   64   // 64..79  : 16 helicopter angles (MC)
#define SPR_PTR_ROTOR_BASE  80   // 80..83  : 4 spinning rotor frames (Hi-Res)
#define SPR_PTR_SHAD_BASE   84   // 84..99  : 16 ground shadow angles (Hi-Res dithered)
#define SPR_PTR_EXPL_BASE   100  // 100..105: 6 crash explosion frames (MC)

// Surface categories for 3D terrain collision & landing
#define SURF_SEA        0
#define SURF_HELIPAD    1
#define SURF_FLAT       2
#define SURF_OBSTACLE   3

// $D018 memory pointer values within VIC Bank 3 ($C000 base)
// Screen bits 7..4 = (addr - $C000) / $0400; Char bits 3..1 = (addr - $C000) / $0800
#define MEMPTR_A_NORTH  0x08  // Screen $C000 + Charset $E000
#define MEMPTR_B_NORTH  0x18  // Screen $C400 + Charset $E000
#define MEMPTR_A_SOUTH  0x0a  // Screen $C000 + Charset $E800
#define MEMPTR_B_SOUTH  0x1a  // Screen $C400 + Charset $E800
#define MEMPTR_HUD      0x2c  // Screen $C800 + Charset $F000

// HUD occupies bottom 4 character rows (rows 20..23 starting at exact FLD VCBASE=800 -> +800)
#define HUD_ROW_OFFSET  800
#define HUD_SCR_BASE    (SCREEN_HUD + HUD_ROW_OFFSET)
#define HUD_COL_BASE    (COLOR_RAM + HUD_ROW_OFFSET)

// Playfield dimensions (rows 0..17 = 18 rows x 40 cols = 720 bytes; 38 cols visible)
#define PF_ROWS         18
#define PF_COLS         40
#define PF_CELLS        (PF_ROWS * PF_COLS)

// Playfield shared multicolor palette
#define PF_BG_DEFAULT   6     // VCOL_BLUE (Deep tropical ocean)
#define PF_MC1_DEFAULT  13    // VCOL_LT_GREEN (Sunlit tropical foliage)
#define PF_MC2_DEFAULT  12    // VCOL_MED_GREY (Slate cliff rock / shadows / trunks)

#endif
