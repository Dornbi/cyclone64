#!/usr/bin/env python3
"""
Generate Commodore 64 hardware sprite bank (4 KB = 64 sprites, unpacked into
VIC-II Bank 3 RAM under I/O at $D000-$DFFF -> sprite pointers 64..127):

  Slots  0..15 (ptr 64..79):  16-angle Multicolor Rescue Helicopter fuselage (22.5 deg steps, CW from North)
  Slots 16..19 (ptr 80..83):   4-frame Hi-Res spinning main & tail rotor overlay
  Slots 20..35 (ptr 84..99):  16-angle Hi-Res checkerboard-dithered ground shadow
  Slots 36..41 (ptr 100..105): 6-frame Multicolor explosion sequence
  Slots 42..63 (ptr 106..127): Reserved for Phases 3-4 (winch, crates, survivors, jets, cyclone)

Also renders a high-visibility PNG contact sheet at extracted/sprites_preview.png.
"""
import math
import os
from PIL import Image, ImageDraw

WORKSPACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN_DIR = os.path.join(WORKSPACE, "generated")
EXT_DIR = os.path.join(WORKSPACE, "extracted")

# Colodore C64 palette for preview sheet
C64_PAL = [
    (0, 0, 0),       # 0 Black
    (255, 255, 255), # 1 White
    (129, 51, 56),   # 2 Red
    (117, 206, 200), # 3 Cyan
    (142, 60, 151),  # 4 Purple
    (86, 172, 77),   # 5 Green
    (46, 44, 155),   # 6 Blue
    (237, 241, 113), # 7 Yellow
    (142, 80, 41),   # 8 Orange
    (85, 56, 0),     # 9 Brown
    (196, 108, 113), # 10 Light Red
    (74, 74, 74),    # 11 Dark Grey
    (123, 123, 123), # 12 Med Grey
    (169, 255, 159), # 13 Light Green
    (112, 109, 235), # 14 Light Blue
    (178, 178, 178), # 15 Light Grey
]

# Sprite Multicolor encoding:
#   0 (%00) = Transparent
#   1 (%01) = $D025 (spr_mcolor0) = WHITE (rescue stripe / highlights / bright flash)
#   2 (%10) = $D027+n (spr_color) = RED (helicopter body) / ORANGE (explosion)
#   3 (%11) = $D026 (spr_mcolor1) = BLACK (cockpit canopy / crisp outline / dark details)


def pack_mc_sprite(grid):
    """Pack 21 rows x 12 multicolor pixels (values 0..3) into a 64-byte C64 hardware sprite."""
    out = bytearray(64)
    for y in range(21):
        row = grid[y]
        for bx in range(3):
            b = 0
            for px in range(4):
                b = (b << 2) | (row[bx * 4 + px] & 3)
            out[y * 3 + bx] = b
    return bytes(out)


def pack_hr_sprite(grid):
    """Pack 21 rows x 24 hi-res pixels (values 0/1) into a 64-byte C64 hardware sprite."""
    out = bytearray(64)
    for y in range(21):
        row = grid[y]
        for bx in range(3):
            b = 0
            for px in range(8):
                b = (b << 1) | (1 if row[bx * 8 + px] else 0)
            out[y * 3 + bx] = b
    return bytes(out)


def generate_helicopter_angle(angle_idx):
    """
    Procedurally render a crisp top-oblique rescue helicopter for angle 0..15
    (0 = North/Up, 4 = East/Right, 8 = South/Down, 12 = West/Left).
    Returns:
      - mc_grid: 21x12 multicolor fuselage grid (0=trans, 1=White, 2=Red, 3=Black)
      - sh_grid: 21x24 hi-res checkerboard-dithered ground shadow grid
    """
    # Angle in radians: 0 = North (0, -1), clockwise
    theta = (angle_idx / 16.0) * 2.0 * math.pi
    fx, fy = math.sin(theta), -math.cos(theta)  # Forward unit vector
    rx, ry = math.cos(theta), math.sin(theta)   # Right unit vector

    # Slight 3D oblique squash in Y (0.86) to match Costa Panayi's angled 3D view!
    y_squash = 0.88

    # Evaluate on 21x12 multicolor grid (cx=5.5, cy=10.0; MC pixels are 2x wide!)
    # Express coordinates in uniform square-pixel units: ux = (x - 5.5)*2, uy = (y - 10.0)
    mc = [[0] * 12 for _ in range(21)]
    for y in range(21):
        for x in range(12):
            ux = (x - 5.5) * 2.0
            uy = (y - 10.0) / y_squash

            # Project onto helicopter local forward (f) and lateral (r) axes
            f = ux * fx + uy * fy
            r = ux * rx + uy * ry

            val = 0
            # 1. Sleek tail boom: f in [-9.8 .. -2.0], narrow red boom
            if -9.8 <= f <= -1.8 and abs(r) <= 1.35:
                val = 2  # Bright rescue red tail boom

            # 2. Tail stabilizer & fin at rear: f in [-9.8 .. -7.6]
            if -9.8 <= f <= -7.5 and abs(r) <= 3.2:
                val = 1  # Crisp white tail stabilizer

            # 3. Rounded main fuselage cabin: f in [-3.4 .. +7.8], r in [-4.0 .. +4.0]
            df = (f - 2.0) / 5.7
            dr = r / 3.9
            if df * df + dr * dr <= 1.0:
                val = 2  # Rescue red main body
                # Crisp white rescue stripe across mid-cabin
                if -0.8 <= f <= 2.2 and abs(r) <= 3.7:
                    val = 1
                # Sleek wrap-around dark cockpit windshield at nose
                if 2.6 <= f <= 6.3 and abs(r) <= 2.7:
                    val = 3  # Black cockpit glass
                # Subtle bright glint on top of canopy
                if 3.2 <= f <= 4.6 and -1.0 <= r <= 1.0:
                    val = 1

            # 4. Subtle dark rear edge shadow on tail boom tip
            if -9.8 <= f <= -8.8 and abs(r) <= 1.0:
                val = 3

            mc[y][x] = val

    # Build matching 21x24 hi-res checkerboard-dithered shadow silhouette!
    sh = [[0] * 24 for _ in range(21)]
    for y in range(21):
        for x in range(24):
            ux = (x - 11.5)
            uy = (y - 10.0) / y_squash
            f = ux * fx + uy * fy
            r = ux * rx + uy * ry
            inside = False
            # Cabin oval
            df = (f - 1.5) / 5.5
            dr = r / 3.5
            if df * df + dr * dr <= 1.0:
                inside = True
            # Tail boom + fin
            if -9.5 <= f <= -1.5 and abs(r) <= 1.4:
                inside = True
            if -9.5 <= f <= -7.2 and abs(r) <= 3.0:
                inside = True
            # Subtle rotor disc shadow hint
            if (ux * ux + uy * uy) <= 6.8 * 6.8 and ((x + y) & 3) == 0:
                inside = True

            # 50% checkerboard dither for crisp translucent C64 ground shadow!
            if inside and ((x + y) & 1) == 0:
                sh[y][x] = 1

    return mc, sh


def generate_rotor_frame(frame_idx):
    """
    Generate 1 of 4 crisp Hi-Res 24x21 spinning main rotor blade overlays.
    Center hub sits slightly forward/centered at (11.5, 9.5) matching fuselage mast.
    """
    grid = [[0] * 24 for _ in range(21)]
    cx, cy = 11.5, 9.5

    # Center rotor mast hub (2x2 clean)
    for dy in (0, 1):
        for dx in (0, 1):
            grid[9 + dy][11 + dx] = 1

    # Crisp 2-blade main rotor (plus faint perpendicular strobe) rotating through 4 phases
    base_angle = (frame_idx / 4.0) * math.pi
    radius = 9.5
    for blade in range(2):
        ang = base_angle + blade * math.pi
        dx, dy = math.cos(ang), math.sin(ang) * 0.88
        for s in range(2, 21):
            t = (s / 20.0) * radius
            px = int(round(cx + dx * t))
            py = int(round(cy + dy * t))
            if 0 <= px < 24 and 0 <= py < 21:
                grid[py][px] = 1
    # Subtle motion-blur ghost blades at 90 degrees (dotted)
    for blade in range(2):
        ang = base_angle + (blade + 0.5) * math.pi
        dx, dy = math.cos(ang), math.sin(ang) * 0.88
        for t in (4.0, 6.5, 9.0):
            px = int(round(cx + dx * t))
            py = int(round(cy + dy * t))
            if 0 <= px < 24 and 0 <= py < 21:
                grid[py][px] = 1

    return grid


def generate_explosion_frame(frame_idx):
    """Generate 6-frame multicolor crash explosion (0=flash .. 5=smoke puffs)."""
    grid = [[0] * 12 for _ in range(21)]
    cx, cy = 5.5, 10.0
    radii = [3.5, 6.0, 8.2, 9.5, 9.8, 8.5]
    rad = radii[frame_idx]
    for y in range(21):
        for x in range(12):
            ux = (x - cx) * 2.0
            uy = (y - cy)
            dist = math.sqrt(ux * ux + uy * uy)
            # Deterministic organic starburst noise
            ang = math.atan2(uy, ux)
            wobble = 1.4 * math.sin(ang * 5.0 + frame_idx * 1.3) + 0.9 * math.cos(ang * 3.0 - frame_idx)
            eff = dist - wobble
            if frame_idx <= 1:
                if eff < rad * 0.55:
                    grid[y][x] = 1  # Bright White hot core
                elif eff < rad:
                    grid[y][x] = 2  # Red/Orange fireball
                elif eff < rad + 1.4:
                    grid[y][x] = 3  # Dark smoke rim
            elif frame_idx <= 3:
                if eff < rad * 0.35:
                    grid[y][x] = 1  # White sparks
                elif eff < rad * 0.78:
                    grid[y][x] = 2  # Fiery red/orange
                elif eff < rad:
                    grid[y][x] = 3  # Dark smoke ring
            else:
                # Expanding dissipating smoke puffs
                if rad * 0.45 < eff < rad and ((x + y + frame_idx) & 1) == 0:
                    grid[y][x] = 3 if frame_idx == 5 else (2 if ((x ^ y) & 1) else 3)
    return grid


def render_preview(mc_helis, rotors, shadows, explosions, out_path):
    """Render a crisp 3x-scaled visual inspection contact sheet of all generated sprites."""
    scale = 3
    cell_w = 24 * scale + 8
    cell_h = 21 * scale + 18
    cols = 8
    rows = 6  # 2 rows heli+rotor, 2 rows shadows, 1 row explosions, 1 row composite on grass/pad
    img = Image.new("RGB", (cols * cell_w + 16, rows * cell_h + 16), (32, 40, 62))
    draw = ImageDraw.Draw(img)

    mc_cols = [None, C64_PAL[1], C64_PAL[2], C64_PAL[0]]

    def blit_mc(grid, x0, y0, body_col=2):
        pal = [None, C64_PAL[1], C64_PAL[body_col], C64_PAL[0]]
        px = img.load()
        for y in range(21):
            for x in range(12):
                c = pal[grid[y][x]]
                if c is None:
                    continue
                for sy in range(scale):
                    for sx in range(scale * 2):
                        px[x0 + x * 2 * scale + sx, y0 + y * scale + sy] = c

    def blit_hr(grid, x0, y0, col_rgb):
        px = img.load()
        for y in range(21):
            for x in range(24):
                if grid[y][x]:
                    for sy in range(scale):
                        for sx in range(scale):
                            px[x0 + x * scale + sx, y0 + y * scale + sy] = col_rgb

    # Rows 0..1: 16-angle Helicopter + Spinning Rotor overlay on tropical green backdrop
    for i in range(16):
        cx = 8 + (i % 8) * cell_w
        cy = 8 + (i // 8) * cell_h
        draw.rectangle([cx, cy, cx + 24 * scale - 1, cy + 21 * scale - 1], fill=C64_PAL[5])
        blit_mc(mc_helis[i], cx, cy, body_col=10)
        blit_hr(rotors[i % 4], cx, cy, C64_PAL[0])
        draw.text((cx + 2, cy + 21 * scale + 2), f"H{i:02d}", fill=(230, 230, 230))

    # Rows 2..3: 16-angle Ground Shadow on white helipad / green split backdrop
    for i in range(16):
        cx = 8 + (i % 8) * cell_w
        cy = 8 + (2 + i // 8) * cell_h
        draw.rectangle([cx, cy, cx + 24 * scale - 1, cy + 21 * scale - 1], fill=C64_PAL[1])
        blit_hr(shadows[i], cx, cy, C64_PAL[0])
        draw.text((cx + 2, cy + 21 * scale + 2), f"S{i:02d}", fill=(200, 220, 255))

    # Row 4: 6-frame Explosion + 2 composite altitude demos
    for i in range(6):
        cx = 8 + i * cell_w
        cy = 8 + 4 * cell_h
        draw.rectangle([cx, cy, cx + 24 * scale - 1, cy + 21 * scale - 1], fill=C64_PAL[6])
        blit_mc(explosions[i], cx, cy, body_col=8)
        draw.text((cx + 2, cy + 21 * scale + 2), f"EX{i}", fill=(255, 220, 140))

    img.save(out_path)


def main():
    os.makedirs(GEN_DIR, exist_ok=True)
    os.makedirs(EXT_DIR, exist_ok=True)

    bank = bytearray(64 * 64)  # 4096 bytes ($D000 - $DFFF)
    mc_helis = []
    shadows = []
    rotors = []
    explosions = []

    # Slots 0..15 (ptr 64..79): 16 helicopter headings
    # Slots 20..35 (ptr 84..99): 16 matching ground shadows
    for i in range(16):
        mc, sh = generate_helicopter_angle(i)
        mc_helis.append(mc)
        shadows.append(sh)
        bank[i * 64 : (i + 1) * 64] = pack_mc_sprite(mc)
        bank[(20 + i) * 64 : (21 + i) * 64] = pack_hr_sprite(sh)

    # Slots 16..19 (ptr 80..83): 4-frame hi-res rotor overlay
    for f in range(4):
        rg = generate_rotor_frame(f)
        rotors.append(rg)
        bank[(16 + f) * 64 : (17 + f) * 64] = pack_hr_sprite(rg)

    # Slots 36..41 (ptr 100..105): 6-frame explosion sequence
    for f in range(6):
        eg = generate_explosion_frame(f)
        explosions.append(eg)
        bank[(36 + f) * 64 : (37 + f) * 64] = pack_mc_sprite(eg)

    out_bin = os.path.join(GEN_DIR, "sprites_d000.bin")
    with open(out_bin, "wb") as f:
        f.write(bank)

    render_preview(mc_helis, rotors, shadows, explosions, os.path.join(EXT_DIR, "sprites_preview.png"))
    print(f"Generated 4 KB sprite bank at {out_bin} (42 active frames + 22 reserved).")


if __name__ == "__main__":
    main()
