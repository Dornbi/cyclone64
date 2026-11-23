#!/usr/bin/env python3
"""
Generate Commodore 64 assets for Cyclone:
  - Detailed tropical-archipelago Multicolor Charsets (North & South views, 2 KB each)
  - Crisp Hi-Res Cockpit HUD Charset (2 KB) with bold font, 8-step bar gauges & icons
  - Remapped dense 128x150 island atlas (tile IDs 0..129)
  - Compact C64 3D slice stack lookup tables + island metadata binary/header
  - Visual C64-palette preview PNGs of Base Island (North & South)
"""
import json
import os
from PIL import Image

WORKSPACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXT_DIR = os.path.join(WORKSPACE, "extracted")
GEN_DIR = os.path.join(WORKSPACE, "generated")

# Colodore C64 RGB palette for accurate preview rendering
C64_PAL = [
    (0, 0, 0),       # 0  Black
    (255, 255, 255), # 1  White
    (129, 51, 56),   # 2  Red
    (117, 206, 200), # 3  Cyan
    (142, 60, 151),  # 4  Purple
    (86, 172, 77),   # 5  Green
    (46, 44, 155),   # 6  Blue
    (237, 241, 113), # 7  Yellow
    (142, 80, 41),   # 8  Orange
    (85, 56, 0),     # 9  Brown
    (196, 108, 113), # 10 Light Red
    (74, 74, 74),    # 11 Dark Grey
    (123, 123, 123), # 12 Med Grey
    (169, 255, 159), # 13 Light Green
    (112, 109, 235), # 14 Light Blue
    (178, 178, 178), # 15 Light Grey
]

# Shared playfield multicolor palette:
#   %00 ($D021) = BLUE (6)      Deep ocean
#   %01 ($D022) = LT_GREEN (13) Sunlit tropical foliage / highlights
#   %10 ($D023) = MED_GREY (12) Cool slate cliff rock / shadows / trunks / building details
#   %11 (ColRAM & 7, bit3=1)    Per-cell accent color (0..7)
BG_COL = 6
MC1_COL = 13
MC2_COL = 12

# Multicolor color RAM values (bit 3 set -> multicolor mode)
MC_BLACK  = 8 + 0
MC_WHITE  = 8 + 1
MC_RED    = 8 + 2
MC_CYAN   = 8 + 3
MC_PURPLE = 8 + 4
MC_GREEN  = 8 + 5
MC_BLUE   = 8 + 6
MC_YELLOW = 8 + 7

# Hi-res color RAM values (bit 3 clear -> 8x8 hires mode over BG_COL)
HR_CYAN   = 3
HR_WHITE  = 1
HR_YELLOW = 7
HR_GREEN  = 5


def mc_tile(rows_str):
    """
    Convert 8 strings of 4 chars into 8 C64 multicolor bytes.
    Pixel legend:
      '.' or '0' -> %00 (BG = Deep Blue)
      '+' or '1' -> %01 (MC1 = Light Green)
      '#' or '2' -> %10 (MC2 = Brown)
      '@' or '3' -> %11 (Per-cell Color RAM & 7)
    """
    cmap = {'.': 0, '0': 0, '+': 1, '1': 1, '#': 2, '2': 2, '@': 3, '3': 3}
    out = bytearray(8)
    for y in range(8):
        s = rows_str[y]
        b = 0
        for x in range(4):
            b = (b << 2) | cmap[s[x]]
        out[y] = b
    return bytes(out)


def hr_tile(rows_str):
    """Convert 8 strings of 8 chars ('.' = 0, '#'/'@' = 1) into 8 hi-res bytes."""
    out = bytearray(8)
    for y in range(8):
        b = 0
        for x in range(8):
            b = (b << 1) | (0 if rows_str[y][x] in ('.', ' ') else 1)
        out[y] = b
    return bytes(out)


def flip_mc_h(tile_bytes):
    out = bytearray(8)
    for y in range(8):
        b = tile_bytes[y]
        p0 = (b >> 6) & 3
        p1 = (b >> 4) & 3
        p2 = (b >> 2) & 3
        p3 = b & 3
        out[y] = (p3 << 6) | (p2 << 4) | (p1 << 2) | p0
    return bytes(out)


def flip_mc_v(tile_bytes):
    return bytes(tile_bytes[::-1])


def build_tropical_charset():
    """
    Build a rich 256-char tropical multicolor charset + per-char Color RAM table.
    Chars 0..3   : Animated shimmering ocean waves (Hi-res Cyan on Deep Blue!)
    Chars 4..127 : Mapped tropical terrain, beaches, cliffs, palms, buildings, helipads, spires
    """
    charset = bytearray(2048)
    colors = bytearray(256)

    def set_char(idx, data, col):
        charset[idx * 8 : idx * 8 + 8] = data
        colors[idx] = col

    # ------------------------------------------------------------------
    # 0..3: Calm shimmering ocean water (Hi-res Cyan on Deep Blue)
    # ------------------------------------------------------------------
    set_char(0, hr_tile([
        "........",
        "........",
        "..##....",
        "........",
        "........",
        "........",
        "........",
        "........",
    ]), HR_CYAN)
    set_char(1, hr_tile([
        "........",
        "........",
        "........",
        "........",
        "........",
        "....###.",
        "........",
        "........",
    ]), HR_CYAN)
    set_char(2, hr_tile([
        "........",
        "........",
        "........",
        "........",
        "........",
        "........",
        "........",
        "........",
    ]), HR_CYAN)
    set_char(3, hr_tile([
        "........",
        ".....##.",
        "........",
        "........",
        "........",
        "........",
        ".##.....",
        "........",
    ]), HR_CYAN)

    # Extra animation frames (4..7) stored in unused region for smooth wave shifting
    # (will be copied into 0..3 during bottom border IRQ!)

    # ------------------------------------------------------------------
    # 8..23: Tropical Grass Plateaus (differentiated by elevation tier!)
    #   %01 = LtGreen (+), %10 = Brown (#), %11 = Green (@)
    # ------------------------------------------------------------------
    # 8: Tier 1 lush lowland grass
    set_char(8, mc_tile([
        "@@@@",
        "@+@+",
        "@@@@",
        "@@@@",
        "@@+@",
        "@@@@",
        "+@@@",
        "@@@@",
    ]), MC_GREEN)
    # 9: Tier 2 sunlit meadow grass (brighter tropical dither)
    set_char(9, mc_tile([
        "@+@+",
        "@@@@",
        "+@@@",
        "@@+@",
        "@+@+",
        "@@@@",
        "@@@+",
        "+@@@",
    ]), MC_GREEN)
    # 10: Tier 3 highland ridge grass (lush + warm earthy speckles)
    set_char(10, mc_tile([
        "+@+@",
        "@+@+",
        "@@+@",
        "@+@+",
        "+@+@",
        "@#@@",
        "@+@+",
        "@@@@",
    ]), MC_GREEN)
    # 11: Tier 4 peak plateau (highland sunlit crown)
    set_char(11, mc_tile([
        "++@+",
        "@++@",
        "+@++",
        "@@+@",
        "++@+",
        "@++@",
        "+@@+",
        "@+@+",
    ]), MC_GREEN)

    # ------------------------------------------------------------------
    # 12..19: Striated Volcanic Tropical Cliff Walls (3D vertical sides!)
    #   Top rim shows grassy lip (+/@), lower body shows layered rock (#/@)
    # ------------------------------------------------------------------
    # 12: Upper cliff wall (just below plateau edge - hanging moss + rock)
    set_char(12, mc_tile([
        "@@@@",
        "@##@",
        "####",
        "##@#",
        "####",
        "#@##",
        "####",
        "###@",
    ]), MC_GREEN)
    # 13: Mid/deep cliff wall (striated dark volcanic rock with crevices - MC_BLACK)
    set_char(13, mc_tile([
        "####",
        "#@##",
        "####",
        "###@",
        "##@#",
        "####",
        "@###",
        "####",
    ]), MC_BLACK)
    # 14: Coastal base cliff wall (rock meeting ocean spray!)
    set_char(14, mc_tile([
        "####",
        "#@##",
        "####",
        "##@#",
        "####",
        "####",
        "#..#",
        "....",
    ]), MC_BLACK)

    # ------------------------------------------------------------------
    # 15..29: Beveled 3D Plateau Edges & Sloped Corners
    # ------------------------------------------------------------------
    # 15: North rim edge (slice 0x27 - crisp dark shadow crest along top)
    set_char(15, mc_tile([
        "####",
        "@+@+",
        "@@@@",
        "@@+@",
        "@@@@",
        "+@@@",
        "@@@@",
        "@@+@",
    ]), MC_GREEN)
    # 16: East edge (slice 0x28 - shaded cliff drop on right)
    set_char(16, mc_tile([
        "@@@#",
        "@+@#",
        "@@@#",
        "@@+#",
        "@@@#",
        "+@@#",
        "@@@#",
        "@@@#",
    ]), MC_GREEN)
    # 17: West edge (slice 0x29 - sunlit/shaded edge on left)
    set_char(17, mc_tile([
        "#@@@",
        "#@+@",
        "#@@@",
        "#@@+",
        "#@@@",
        "#+@@",
        "#@@@",
        "#@@@",
    ]), MC_GREEN)
    # 18: NE corner (slice 0x2b)
    set_char(18, mc_tile([
        "####",
        "@+@#",
        "@@@#",
        "@@+#",
        "@@@#",
        "+@@#",
        "@@@#",
        "@@@#",
    ]), MC_GREEN)
    # 19: NW corner (slice 0x2c)
    set_char(19, mc_tile([
        "####",
        "#@+@",
        "#@@@",
        "#@@+",
        "#@@@",
        "#+@@",
        "#@@@",
        "#@@@",
    ]), MC_GREEN)

    # Sloped diagonal corners over water / grass
    # 20: Coastal diag NW->SE (slice 0x19: land bottom-left, water top-right)
    set_char(20, mc_tile([
        "....",
        "@...",
        "@@..",
        "+@@.",
        "@@@.",
        "@@@#",
        "@@@@",
        "@@@@",
    ]), MC_GREEN)
    # 21: Coastal diag NE->SW (slice 0x1a: land bottom-right, water top-left)
    set_char(21, flip_mc_h(charset[20 * 8 : 20 * 8 + 8]), MC_GREEN)
    # 22: Inner diagonal edge NW (slice 0x1b)
    set_char(22, mc_tile([
        "#@@@",
        "@#@@",
        "@@#@",
        "@@@#",
        "@@+@",
        "@@@@",
        "+@@@",
        "@@@@",
    ]), MC_GREEN)
    # 23: Inner diagonal edge NE (slice 0x1c)
    set_char(23, flip_mc_h(charset[22 * 8 : 22 * 8 + 8]), MC_GREEN)

    # 24: Shaded cliff corner over water right (slice 0x1d)
    set_char(24, mc_tile([
        ".###",
        ".##@",
        "..##",
        "..##",
        "...#",
        "...#",
        "....",
        "....",
    ]), MC_BLACK)
    # 25: Shaded cliff corner over water left (slice 0x1e)
    set_char(25, flip_mc_h(charset[24 * 8 : 24 * 8 + 8]), MC_BLACK)
    # 26: Shaded cliff corner over grass right (slice 0x1f)
    set_char(26, mc_tile([
        "@###",
        "@@##",
        "@@##",
        "@@@#",
        "@@@#",
        "@@@#",
        "@@@@",
        "@@@@",
    ]), MC_GREEN)
    # 27: Shaded cliff corner over grass left (slice 0x20)
    set_char(27, flip_mc_h(charset[26 * 8 : 26 * 8 + 8]), MC_GREEN)
    # 28: Diagonal shaded slope left (slice 0x21)
    set_char(28, mc_tile([
        "#@@@",
        "##@@",
        "##@@",
        "###@",
        "###@",
        "####",
        "####",
        "####",
    ]), MC_GREEN)
    # 29: Diagonal shaded slope right (slice 0x22)
    set_char(29, flip_mc_h(charset[28 * 8 : 28 * 8 + 8]), MC_GREEN)
    # 30: Vertical shaded cliff left (slices 0x5d, 0x5e)
    set_char(30, mc_tile([
        "###@",
        "##@@",
        "###@",
        "##@#",
        "###@",
        "##@@",
        "###@",
        "##@#",
    ]), MC_GREEN)
    # 31: Vertical shaded cliff right (slices 0x5b, 0x5c)
    set_char(31, flip_mc_h(charset[30 * 8 : 30 * 8 + 8]), MC_GREEN)

    # ------------------------------------------------------------------
    # 32..45: Golden Sandy Beaches, Lagoon Coves & Aprons (MC_YELLOW)
    #   %00 = Blue ocean, %10 = Brown wet sand, %11 = Yellow sand, %01 = LtGreen
    # ------------------------------------------------------------------
    # 32: Solid golden sandy beach (slice 0x82)
    set_char(32, mc_tile([
        "@@@@",
        "@@@@",
        "@@#@",
        "@@@@",
        "@#@@",
        "@@@@",
        "@@@@",
        "@@@#",
    ]), MC_YELLOW)
    # 33: Shaded sandy wall / apron base (slice 0x88)
    set_char(33, mc_tile([
        "@@@@",
        "#@@#",
        "####",
        "#@##",
        "####",
        "##@#",
        "####",
        "####",
    ]), MC_YELLOW)
    # 34: Beach corner NW (slice 0x03)
    set_char(34, mc_tile([
        "@@@.",
        "@@@.",
        "@@..",
        "@...",
        "#...",
        "....",
        "....",
        "....",
    ]), MC_YELLOW)
    # 35: Beach corner NE (slice 0x04)
    set_char(35, flip_mc_h(charset[34 * 8 : 34 * 8 + 8]), MC_YELLOW)
    # 36: Beach corner SE (slice 0x05)
    set_char(36, flip_mc_v(charset[35 * 8 : 35 * 8 + 8]), MC_YELLOW)
    # 37: Beach corner SW (slice 0x06)
    set_char(37, flip_mc_v(charset[34 * 8 : 34 * 8 + 8]), MC_YELLOW)
    # 38: Beach-to-grass transition A (slice 0x08)
    set_char(38, mc_tile([
        "+@@@",
        "++@@",
        "@+@@",
        "@@@+",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
    ]), MC_YELLOW)
    # 39: Beach-to-grass transition B (slice 0x0a)
    set_char(39, flip_mc_v(flip_mc_h(charset[38 * 8 : 38 * 8 + 8])), MC_YELLOW)
    # 40: Beach south shore edge (slice 0x10)
    set_char(40, mc_tile([
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "####",
        ".##.",
        "....",
    ]), MC_YELLOW)
    # 41: Beach north shore edge (slice 0x11)
    set_char(41, flip_mc_v(charset[40 * 8 : 40 * 8 + 8]), MC_YELLOW)
    # 42: Beach east shore edge (slice 0x12)
    set_char(42, mc_tile([
        "@@@.",
        "@@@#",
        "@@@#",
        "@@@@",
        "@@@@",
        "@@@#",
        "@@@#",
        "@@@.",
    ]), MC_YELLOW)
    # 43: Beach west shore edge (slice 0x13)
    set_char(43, flip_mc_h(charset[42 * 8 : 42 * 8 + 8]), MC_YELLOW)
    # 44: Beach inner cove S (slice 0x14)
    set_char(44, mc_tile([
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        ".@@.",
    ]), MC_YELLOW)
    # 45: Beach inner cove N (slice 0x15)
    set_char(45, flip_mc_v(charset[44 * 8 : 44 * 8 + 8]), MC_YELLOW)

    # ------------------------------------------------------------------
    # 48..63: Lush Tropical Palm & Jungle Trees (3 vertical slices!)
    #   Trunk (# Brown) -> Lush Canopy (@ Green / + LtGreen) -> Crown
    # ------------------------------------------------------------------
    # Palm Type A (slices 0x4c -> 0x4b -> 0x4a / 0x47 / 0x48 / 0x49)
    # 48: Palm trunk base on grass (slice 0x4c, 0x4f, 0x52)
    set_char(48, mc_tile([
        "@+#@",
        "@@#@",
        "@@#@",
        "@@#@",
        "@@#@",
        "@##@",
        "@@@@",
        "@@@@",
    ]), MC_GREEN)
    # 49: Lush palm mid-foliage over grass (slice 0x4b, 0x4e, 0x51)
    set_char(49, mc_tile([
        "+@++",
        "++@+",
        "@#@+",
        "+@#@",
        "@+#+",
        "@@#@",
        "@##@",
        "@@#@",
    ]), MC_GREEN)
    # 50: Palm mid-foliage silhouette against ocean sky (slice 0x46)
    set_char(50, mc_tile([
        ".+@.",
        "+@++",
        "++@+",
        "@#@.",
        ".@#.",
        "..#.",
        "..#.",
        "..#.",
    ]), MC_GREEN)
    # 51: Palm crown over grass (slice 0x4a, 0x47, 0x4d, 0x50)
    set_char(51, mc_tile([
        "@++@",
        "++++",
        "+@++",
        "@+@+",
        "+@++",
        "@@+@",
        "@@@@",
        "@@@@",
    ]), MC_GREEN)
    # 52: Palm crown against ocean sky (slice 0x49)
    set_char(52, mc_tile([
        "....",
        ".++.",
        "++++",
        "+@++",
        ".@@.",
        ".+#.",
        "..#.",
        "....",
    ]), MC_GREEN)
    # 53: Palm crown over white concrete (slice 0x48)
    set_char(53, mc_tile([
        "@++@",
        "++++",
        "+#++",
        "@@+@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
    ]), MC_WHITE)

    # Golden Tropical Palms (slices 0x55 -> 0x54 -> 0x53: yellow sunlit fronds!)
    # 54: Golden palm trunk base (slice 0x55)
    set_char(54, mc_tile([
        "@@#@",
        "@@#@",
        "@@#@",
        "@@#@",
        "@@#@",
        "@##@",
        "@@@@",
        "@@@@",
    ]), MC_GREEN)
    # 55: Golden palm mid fronds (slice 0x54)
    set_char(55, mc_tile([
        "+@@+",
        "@@@@",
        "@#@+",
        "+@#@",
        "@@#@",
        "@@#@",
        "+@#@",
        "@@#@",
    ]), MC_YELLOW)
    # 56: Golden palm sunlit crown (slice 0x53)
    set_char(56, mc_tile([
        "+@@+",
        "@@@@",
        "@++@",
        "@@@@",
        "+@@+",
        "@@@@",
        "@#@@",
        "@@@@",
    ]), MC_YELLOW)

    # ------------------------------------------------------------------
    # 60..75: Outpost Buildings, Terracotta Roofs, Helipads & Spires
    # ------------------------------------------------------------------
    # 60: Red terracotta tiled roof front-facing (slice 0x2d)
    set_char(60, mc_tile([
        "####",
        "@#@#",
        "@@@@",
        "#@#@",
        "@@@@",
        "@#@#",
        "@@@@",
        "####",
    ]), MC_RED)
    # 61: Red terracotta tiled roof sunlit top (slice 0x2e)
    set_char(61, mc_tile([
        "@#@#",
        "@@@@",
        "#@#@",
        "@@@@",
        "@#@#",
        "@@@@",
        "#@#@",
        "@@@@",
    ]), MC_RED)
    # 62: White stucco wall with eaves line (slice 0x57)
    set_char(62, mc_tile([
        "####",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
    ]), MC_WHITE)
    # 63: White stucco wall with doorway (slice 0x58)
    set_char(63, mc_tile([
        "@@@@",
        "@@@@",
        "@##@",
        "@##@",
        "@##@",
        "@##@",
        "@##@",
        "@##@",
    ]), MC_WHITE)
    # 64: White stucco wall with eaves + lit window (slice 0x59)
    set_char(64, mc_tile([
        "####",
        "@@@@",
        "@##@",
        "@.#@",
        "@##@",
        "@@@@",
        "@@@@",
        "@@@@",
    ]), MC_WHITE)
    # 65: White stucco wall with window (slice 0x5a)
    set_char(65, mc_tile([
        "@@@@",
        "@@@@",
        "@##@",
        "@.#@",
        "@##@",
        "@@@@",
        "@@@@",
        "@@@@",
    ]), MC_WHITE)
    # 66: Shaded building / tower concrete side wall (slice 0x89)
    set_char(66, mc_tile([
        "@@@@",
        "@@@#",
        "@@@@",
        "@@@#",
        "@@@@",
        "@@@#",
        "@@@@",
        "####",
    ]), MC_WHITE)
    # 67: Concrete roof / flat pad top (slice 0x83)
    set_char(67, mc_tile([
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
    ]), MC_WHITE)
    # 68: Helipad clean concrete landing pad surface
    set_char(68, mc_tile([
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
        "@@@@",
    ]), MC_WHITE)
    # 69: Helipad 'H' center marking
    set_char(69, mc_tile([
        "@##@",
        "@##@",
        "@##@",
        "@@@@",
        "@@@@",
        "@##@",
        "@##@",
        "@##@",
    ]), MC_WHITE)
    # 70: Radio Spire / Lighthouse top beacon (for 0xE8 top!)
    set_char(70, mc_tile([
        "+@@+",
        "@##@",
        "@##@",
        "+@@+",
        "+##+",
        "+@@+",
        "+##+",
        "+@@+",
    ]), MC_RED)
    # 71: Radio Spire / Lighthouse striped shaft (for 0xE8 body!)
    set_char(71, mc_tile([
        "+@@+",
        "+@@+",
        "+##+",
        "+##+",
        "+@@+",
        "+@@+",
        "+##+",
        "+##+",
    ]), MC_WHITE)

    return bytes(charset), bytes(colors)


def map_slice_to_c64_char(slice_id, z, tile_height, tile_id):
    """Map an original Spectrum slice ID + elevation context to our C64 tropical char index."""
    # Special overrides for helipads & tall spires
    if tile_id == 0xE8:
        return 70 if z == tile_height - 1 else 71
    if tile_id in (0x1F, 0x45, 0x75, 0x86):
        if z == tile_height - 1:
            return 68  # Helipad landing surface
        return 66      # Concrete pad foundation wall

    if slice_id == 0x80:
        return 0       # Ocean (spatially varied 0..3 at render time!)
    if slice_id == 0x81:
        # Differentiate flat plateau grass by elevation height!
        if tile_height <= 2:
            return 8
        elif tile_height == 3:
            return 9
        elif tile_height == 4:
            return 10
        else:
            return 11
    if slice_id == 0x87 or slice_id in (0x17, 0x18):
        # Differentiate cliff walls: top slice below rim = mossy rock, base = coastal rock, middle = volcanic strata
        if z == 0:
            return 14
        elif z == tile_height - 2:
            return 12
        return 13

    direct = {
        0x27: 15,
        0x28: 16,
        0x29: 17,
        0x2B: 18,
        0x2C: 19,
        0x19: 20,
        0x1A: 21,
        0x1B: 22,
        0x1C: 23,
        0x1D: 24,
        0x1E: 25,
        0x1F: 26,
        0x20: 27,
        0x21: 28,
        0x22: 29,
        0x5D: 30,
        0x5E: 30,
        0x5B: 31,
        0x5C: 31,
        0x82: 32,
        0x88: 33,
        0x03: 34,
        0x04: 35,
        0x05: 36,
        0x06: 37,
        0x08: 38,
        0x0A: 39,
        0x10: 40,
        0x11: 41,
        0x12: 42,
        0x13: 43,
        0x14: 44,
        0x15: 45,
        0x4C: 48, 0x4F: 48, 0x52: 48,
        0x4B: 49, 0x4E: 49, 0x51: 49,
        0x46: 50,
        0x4A: 51, 0x47: 51, 0x4D: 51, 0x50: 51,
        0x49: 52,
        0x48: 53,
        0x55: 54,
        0x54: 55,
        0x53: 56,
        0x2D: 60,
        0x2E: 61,
        0x57: 62,
        0x58: 63,
        0x59: 64,
        0x5A: 65,
        0x89: 66,
        0x83: 67,
    }
    return direct.get(slice_id, 8)


def build_hud_charset():
    """
    Build a crisp 2 KB Hi-Res Cockpit HUD charset:
      - Screen codes 0..63: Clean bold arcade 8x8 uppercase font + digits + symbols
      - Screen codes 64..72: 8-step horizontal gauge bar segments (0..8 px filled, framed top/bottom)
      - Screen codes 73..76: Cockpit bezel border pieces
      - Screen codes 77..79: Helicopter icon, Medical Crate icon, Compass N/S badge
    """
    charset = bytearray(2048)

    # Compact 5x7 / 7x7 crisp arcade font definitions (indexed by C64 screen code)
    font = {
        32: [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00], # ' '
        1:  [0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00], # A
        2:  [0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00], # B
        3:  [0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00], # C
        4:  [0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00], # D
        5:  [0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00], # E
        6:  [0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00], # F
        7:  [0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00], # G
        8:  [0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00], # H
        9:  [0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00], # I
        10: [0x1E, 0x0C, 0x0C, 0x0C, 0x6C, 0x6C, 0x38, 0x00], # J
        11: [0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00], # K
        12: [0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00], # L
        13: [0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00], # M
        14: [0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00], # N
        15: [0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00], # O
        16: [0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00], # P
        17: [0x3C, 0x66, 0x66, 0x66, 0x6A, 0x6C, 0x36, 0x00], # Q
        18: [0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00], # R
        19: [0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00], # S
        20: [0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00], # T
        21: [0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00], # U
        22: [0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00], # V
        23: [0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00], # W
        24: [0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00], # X
        25: [0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00], # Y
        26: [0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00], # Z
        48: [0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00], # 0
        49: [0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00], # 1
        50: [0x3C, 0x66, 0x06, 0x1C, 0x30, 0x60, 0x7E, 0x00], # 2
        51: [0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00], # 3
        52: [0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00], # 4
        53: [0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00], # 5
        54: [0x3C, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x3C, 0x00], # 6
        55: [0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00], # 7
        56: [0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00], # 8
        57: [0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00], # 9
        58: [0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00], # :
        45: [0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00], # -
        47: [0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00, 0x00], # /
        46: [0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00], # .
        27: [0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00], # [
        29: [0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00], # ]
        33: [0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00], # !
    }
    for code, rows in font.items():
        charset[code * 8 : code * 8 + 8] = bytes(rows)

    # 64..72: Horizontal gauge segments (0..8 px filled, framed top/bottom with dotted rail)
    for px_fill in range(9):
        mask = (0xFF << (8 - px_fill)) & 0xFF if px_fill > 0 else 0x00
        rows = [
            0xAA,        # subtle top rail
            mask,
            mask,
            mask,
            mask,
            mask,
            0xAA,        # subtle bottom rail
            0x00,
        ]
        charset[(64 + px_fill) * 8 : (64 + px_fill) * 8 + 8] = bytes(rows)

    # 73: Medical Crate icon (box with red cross cutout!)
    charset[73 * 8 : 73 * 8 + 8] = bytes([0x7E, 0x66, 0x5A, 0x42, 0x5A, 0x66, 0x7E, 0x00])
    # 74: Empty Crate slot
    charset[74 * 8 : 74 * 8 + 8] = bytes([0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00])
    # 75: Helicopter life icon
    charset[75 * 8 : 75 * 8 + 8] = bytes([0xFE, 0x10, 0x3C, 0xFE, 0x3E, 0x14, 0x3E, 0x00])
    # 76: Horizontal metallic divider
    charset[76 * 8 : 76 * 8 + 8] = bytes([0x00, 0xFF, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00])

    return bytes(charset)


def render_c64_char(img, charset, colors, ch, x0, y0, scale=2):
    px = img.load()
    col = colors[ch]
    mc = bool(col & 8)
    c11 = C64_PAL[col & 7]
    c00 = C64_PAL[BG_COL]
    c01 = C64_PAL[MC1_COL]
    c10 = C64_PAL[MC2_COL]
    lut = [c00, c01, c10, c11]
    base = ch * 8
    for ry in range(8):
        b = charset[base + ry]
        if mc:
            for rx in range(4):
                c = lut[(b >> (6 - rx * 2)) & 3]
                for sy in range(scale):
                    for sx in range(scale * 2):
                        px[x0 + rx * 2 * scale + sx, y0 + ry * scale + sy] = c
        else:
            for rx in range(8):
                c = c11 if (b & (0x80 >> rx)) else c00
                for sy in range(scale):
                    for sx in range(scale):
                        px[x0 + rx * scale + sx, y0 + ry * scale + sy] = c


def render_c64_island_preview(atlas_dense, tile_heights, tile_stacks, charset, colors, island, south, out_path):
    w, h = island["width"], island["height"]
    mx, my = island["atlas_x"], island["atlas_y"]
    margin_top = 8
    scale = 2
    img = Image.new("RGB", (w * 8 * scale, (h + margin_top) * 8 * scale), C64_PAL[BG_COL])
    # Paint using exact cell-projection rule (checking dy=7..0)
    for sy in range(h + margin_top):
        for sx in range(w):
            ch = ((sx + sy) & 3)  # Ocean wave pattern
            for z in range(7, -1, -1):
                row_y = sy - margin_top + z
                if 0 <= row_y < h:
                    ay = (h - 1 - row_y) if south else row_y
                    ax = (w - 1 - sx) if south else sx
                    if my + ay < 150 and mx + ax < 128:
                        dt = atlas_dense[(my + ay) * 128 + (mx + ax)]
                        if dt != 0 and tile_heights[dt] > z:
                            ch = tile_stacks[dt * 8 + z]
                            break
            render_c64_char(img, charset, colors, ch, sx * 8 * scale, sy * 8 * scale, scale=scale)
    img.save(out_path)


def main():
    os.makedirs(GEN_DIR, exist_ok=True)
    with open(os.path.join(EXT_DIR, "atlas_128x150.bin"), "rb") as f:
        atlas_raw = bytearray(f.read())
    with open(os.path.join(EXT_DIR, "islands.json")) as f:
        islands = json.load(f)
    with open(os.path.join(EXT_DIR, "tiles.json")) as f:
        tiles = json.load(f)

    # Remap original tile IDs -> dense 0..N-1 (with 0x00 -> 0)
    orig_ids = sorted(int(k, 16) for k in tiles.keys())
    assert orig_ids[0] == 0
    orig_to_dense = {oid: idx for idx, oid in enumerate(orig_ids)}
    num_tiles = len(orig_ids)

    atlas_dense = bytearray(len(atlas_raw))
    for i, b in enumerate(atlas_raw):
        atlas_dense[i] = orig_to_dense[b]

    charset_north, char_colors = build_tropical_charset()
    # South view uses the same rich tropical glyphs (slice stacks already flip bevels & roofs!)
    charset_south = charset_north

    tile_heights = bytearray(num_tiles)
    tile_surface = bytearray(num_tiles)
    tile_north = bytearray(num_tiles * 8)
    tile_south = bytearray(num_tiles * 8)

    # Surface types: 0=SEA, 1=HELIPAD, 2=FLAT_LAND, 3=OBSTACLE (cliffs/trees/buildings/spires)
    cat_to_surf = {
        "sea": 0,
        "helipad": 1,
        "grass_flat": 2,
        "beach": 2,
        "apron": 2,
        "concrete": 2,
        "cliff_slope": 3,
        "tree": 3,
        "building": 3,
        "spire": 3,
    }

    for idx, oid in enumerate(orig_ids):
        info = tiles[f"0x{oid:02x}"]
        h = info["height"]
        tile_heights[idx] = h
        tile_surface[idx] = cat_to_surf.get(info["category"], 3)
        for z, s in enumerate(info["north_slices"]):
            tile_north[idx * 8 + z] = map_slice_to_c64_char(s, z, h, oid)
        for z, s in enumerate(info["south_slices"]):
            tile_south[idx * 8 + z] = map_slice_to_c64_char(s, z, h, oid)

    hud_charset = build_hud_charset()

    # Write binary blobs for Oscar64 #embed
    with open(os.path.join(GEN_DIR, "atlas_dense.bin"), "wb") as f:
        f.write(atlas_dense)
    with open(os.path.join(GEN_DIR, "charset_north.bin"), "wb") as f:
        f.write(charset_north)
    with open(os.path.join(GEN_DIR, "charset_south.bin"), "wb") as f:
        f.write(charset_south)
    with open(os.path.join(GEN_DIR, "hud_charset.bin"), "wb") as f:
        f.write(hud_charset)
    with open(os.path.join(GEN_DIR, "char_colors.bin"), "wb") as f:
        f.write(char_colors)
    with open(os.path.join(GEN_DIR, "tile_heights.bin"), "wb") as f:
        f.write(tile_heights)
    with open(os.path.join(GEN_DIR, "tile_surface.bin"), "wb") as f:
        f.write(tile_surface)
    with open(os.path.join(GEN_DIR, "tile_north.bin"), "wb") as f:
        f.write(tile_north)
    with open(os.path.join(GEN_DIR, "tile_south.bin"), "wb") as f:
        f.write(tile_south)

    # Generate C header for 14-island metadata table
    with open(os.path.join(GEN_DIR, "islands_data.h"), "w") as f:
        f.write("#ifndef ISLANDS_DATA_H\n#define ISLANDS_DATA_H\n\n")
        f.write("#include <c64/types.h>\n\n")
        f.write(f"#define NUM_ISLANDS {len(islands)}\n")
        f.write(f"#define NUM_DENSE_TILES {num_tiles}\n\n")
        f.write("typedef struct IslandDesc {\n")
        f.write("    int world_x;\n")
        f.write("    int world_y;\n")
        f.write("    byte atlas_x;\n")
        f.write("    byte atlas_y;\n")
        f.write("    byte width;\n")
        f.write("    byte height;\n")
        f.write("    byte has_helipad;\n")
        f.write("    const char *name;\n")
        f.write("} IslandDesc;\n\n")
        f.write("static const IslandDesc ISLANDS[NUM_ISLANDS] = {\n")
        for isl in islands:
            f.write(
                f'    {{ {isl["world_x"]:3d}, {isl["world_y"]:3d}, '
                f'{isl["atlas_x"]:3d}, {isl["atlas_y"]:3d}, '
                f'{isl["width"]:2d}, {isl["height"]:2d}, '
                f'{1 if isl["has_helipad"] else 0}, "{isl["name"]}" }},\n'
            )
        f.write("};\n\n#endif\n")

    # Render visual C64 previews of Base Island (North & South!)
    render_c64_island_preview(
        atlas_dense, tile_heights, tile_north, charset_north, char_colors,
        islands[5], south=False, out_path=os.path.join(EXT_DIR, "base_island_c64_north.png")
    )
    render_c64_island_preview(
        atlas_dense, tile_heights, tile_south, charset_south, char_colors,
        islands[5], south=True, out_path=os.path.join(EXT_DIR, "base_island_c64_south.png")
    )
    print(f"Generated C64 assets in {GEN_DIR}/ ({num_tiles} dense tiles, 2KB North/South/HUD charsets).")


if __name__ == "__main__":
    main()
