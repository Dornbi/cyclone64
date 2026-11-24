#!/usr/bin/env python3
"""
Automated VICE x64sc Visual & Hardware Regression Test Suite for Cyclone (C64).
Builds & captures screenshots across all key game states (Title, Help, Flight,
Diagonal NW/SW scrolling, PAL & NTSC, South View, Radar, Storm, Winch Pickup),
runs automated pixel assertions on the raster split bezel, HUD integrity, and
multicolor terrain fidelity, and outputs a labeled contact sheet PNG.
"""
import os
import subprocess
import sys
from PIL import Image, ImageDraw

WORKSPACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(WORKSPACE, "build", "tests")
os.makedirs(OUT_DIR, exist_ok=True)

OSCAR64 = "/Users/peter/bin/oscar64"
OSCAR64_INC = "/Users/peter/Library/CloudStorage/GoogleDrive-dornbach@gmail.com/My Drive/Programozas/c64/oscar64-main-20260818/include"
X64SC = "/Applications/vice-arm64-gtk3-3.10/bin/x64sc"

SRCS = [
    "src/main.c", "src/world.c", "src/raster.c", "src/scroller.c",
    "src/helicopter.c", "src/entities.c", "src/cyclone.c",
    "src/radar_map.c", "src/hud.c", "src/audio.c", "src/title.c",
]


def compile_prg(out_prg, extra_flags=None):
    cmd = [
        OSCAR64, f"-ii={OSCAR64_INC}",
        "-O2", "-Oo", "-tm=c64", "-n", "-xz",
        f"-o={out_prg}",
    ] + (extra_flags or []) + SRCS
    res = subprocess.run(cmd, cwd=WORKSPACE, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"COMPILE FAILED for {out_prg}:\n{res.stdout}\n{res.stderr}")
        sys.exit(1)


def capture_vice(prg_path, png_path, ntsc=False, sidmodel=1, cycles=5000000):
    if os.path.exists(png_path):
        os.remove(png_path)
    cmd = [
        X64SC,
        "-ntsc" if ntsc else "-pal",
        "-sidmodel", str(sidmodel),
        "-warp",
        "-sounddev", "dummy",
        "-autostartprgmode", "1",
        "-limitcycles", str(cycles),
        "-exitscreenshot", png_path,
        "-autostart", prg_path,
    ]
    subprocess.run(cmd, cwd=WORKSPACE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not os.path.exists(png_path):
        raise RuntimeError(f"VICE failed to produce {png_path}")


def is_black(rgb):
    return rgb[0] < 16 and rgb[1] < 16 and rgb[2] < 16


def is_light_green(rgb):
    # C64 Light Green (bright green channel, relatively bright R/B)
    return rgb[1] > 180 and rgb[1] > rgb[0] + 20 and rgb[1] > rgb[2] + 20


def verify_gameplay_frame(png_path, label, expect_land=True):
    """
    Verify critical C64 VIC-II hardware invariants on a live gameplay screenshot:
      1. Pure black raster bezel between playfield and HUD (no blue leak / IRQ desync)
      2. Pure black HUD background & intact top scanline of HUD row 0 (no FLD clipping)
      3. Multicolor terrain present (light-green MC1 pixels rendered, not single-color hi-res)
    """
    im = Image.open(png_path).convert("RGB")
    w, h = im.size
    px = im.load()

    # Locate top border height dynamically (PAL 384x272 has top screen at y=35; NTSC 384x247 at y=22)
    # Find first non-black row inside center columns (x=64..320) -> playfield top!
    pf_top = None
    for y in range(h):
        non_blk = sum(1 for x in range(64, w - 64) if not is_black(px[x, y]))
        if non_blk > 40:
            pf_top = y
            break
    if pf_top is None:
        return False, "Blank screen!"

    # Playfield is 18 rows = 144 scanlines (lines 50..193). Bezel is lines 194..218 (~20px black).
    # HUD starts 169 scanlines below line 50 -> pf_top + 169..pf_top + 200!
    bezel_y0 = pf_top + 148
    bezel_y1 = pf_top + 164
    bezel_nonblack = 0
    for y in range(bezel_y0, min(bezel_y1, h)):
        for x in range(40, w - 40):
            if not is_black(px[x, y]):
                bezel_nonblack += 1

    # Find HUD top scanline (first non-black scanline after bezel_y0)
    hud_top = None
    for y in range(bezel_y0, h):
        non_blk = sum(1 for x in range(40, w - 40) if not is_black(px[x, y]))
        if non_blk > 10:
            hud_top = y
            break
    if hud_top is None:
        return False, "HUD missing!"

    # Check HUD row 0 height & total 4-row HUD span (4 rows x 8px = 31px active span!)
    hud_row0_lines = 0
    for dy in range(8):
        if hud_top + dy < h:
            cnt = sum(1 for x in range(40, w - 40) if not is_black(px[x, hud_top + dy]))
            if cnt > 10:
                hud_row0_lines += 1

    hud_bot = hud_top
    for y in range(hud_top, h):
        cnt = sum(1 for x in range(40, w - 40) if not is_black(px[x, y]))
        if cnt > 10:
            hud_bot = y
    hud_span = (hud_bot - hud_top) + 1

    # Check HUD background is pure black (sample column 0 blank margin x=33..38!)
    hud_bg_black = 0
    hud_bg_tot = 0
    for y in range(hud_top, min(hud_top + 30, h)):
        for x in range(33, 39):
            hud_bg_tot += 1
            if is_black(px[x, y]):
                hud_bg_black += 1

    # Check multicolor Light Green MC1 presence inside playfield (proves MCM=1 & Color RAM bit3 intact!)
    mc_ltgreen = 0
    for y in range(pf_top, min(pf_top + 140, h)):
        for x in range(48, w - 48):
            if is_light_green(px[x, y]):
                mc_ltgreen += 1

    errs = []
    if bezel_nonblack > 0:
        errs.append(f"bezel_nonblack={bezel_nonblack} (raster split leak!)")
    if hud_row0_lines < 7:
        errs.append(f"hud_row0_lines={hud_row0_lines} (top of HUD clipped!)")
    if hud_span < 30:
        errs.append(f"hud_span={hud_span}px (expected >=30px for all 4 HUD rows!)")
    if hud_bg_tot > 0 and hud_bg_black < hud_bg_tot * 0.9:
        errs.append(f"hud_bg_black={hud_bg_black}/{hud_bg_tot} (HUD bg not black!)")
    if expect_land and mc_ltgreen < 500:
        errs.append(f"mc_ltgreen={mc_ltgreen} (multicolor terrain missing!)")

    info = f"bezel_err={bezel_nonblack}, hud_row0={hud_row0_lines}px, hud_span={hud_span}px, mc_ltgreen={mc_ltgreen}"
    return (len(errs) == 0), ("; ".join(errs) if errs else info)


def build_contact_sheet(entries, out_path):
    if not entries:
        return
    cols = 3
    rows = (len(entries) + cols - 1) // cols
    tw, th = 384, 272
    pad = 24
    sheet = Image.new("RGB", (cols * tw + (cols + 1) * 8, rows * (th + pad) + 8), (24, 24, 28))
    draw = ImageDraw.Draw(sheet)
    for idx, (label, path, ok) in enumerate(entries):
        r, c = divmod(idx, cols)
        x0 = 8 + c * (tw + 8)
        y0 = 8 + r * (th + pad)
        im = Image.open(path).convert("RGB").resize((tw, th), Image.NEAREST)
        sheet.paste(im, (x0, y0 + pad))
        col = (100, 240, 120) if ok else (255, 90, 90)
        draw.text((x0 + 4, y0 + 5), f"{'PASS' if ok else 'FAIL'}  {label}", fill=col)
    sheet.save(out_path)


def main():
    main_prg = os.path.join(WORKSPACE, "build", "cyclone.prg")
    compile_prg(main_prg)

    scenarios = [
        # (name, prg_flags, ntsc, sidmodel, cycles, verify_mode)
        ("01_title_pal_8580",     None,                                  False, 1, 4500000, "screen"),
        ("02_title_ntsc_6581",    None,                                  True,  0, 4500000, "screen"),
        ("03_help_screen",        ["-DSKIP_TITLE", "-DTEST_SCENARIO=3"], False, 1, 4500000, "screen"),
        ("04_title_to_flight",    ["-DSKIP_TITLE", "-DTEST_SCENARIO=4"], False, 1, 6000000, "gameplay"),
        ("05_diag_nw_pal",        ["-DSKIP_TITLE", "-DTEST_SCENARIO=5"], False, 1, 7500000, "gameplay"),
        ("06_diag_sw_pal",        ["-DSKIP_TITLE", "-DTEST_SCENARIO=6"], False, 1, 7500000, "gameplay"),
        ("07_diag_nw_ntsc",       ["-DSKIP_TITLE", "-DTEST_SCENARIO=5"], True,  0, 7500000, "gameplay"),
        ("08_south_view_flight",  ["-DSKIP_TITLE", "-DTEST_SCENARIO=8"], False, 1, 7000000, "gameplay"),
        ("09_radar_map",          ["-DSKIP_TITLE", "-DTEST_SCENARIO=9"], False, 1, 4500000, "screen"),
        ("10_storm_cyclone",      ["-DSKIP_TITLE", "-DTEST_SCENARIO=10"],False, 1, 5000000, "storm"),
        ("11_winch_pickup",       ["-DSKIP_TITLE", "-DTEST_SCENARIO=11"],False, 1, 7000000, "gameplay"),
    ]

    results = []
    all_ok = True
    tmp_prg = os.path.join(OUT_DIR, "_tmp_test.prg")

    for name, flags, ntsc, sidmodel, cycles, vmode in scenarios:
        png = os.path.join(OUT_DIR, f"{name}.png")
        prg = main_prg if flags is None else tmp_prg
        if flags is not None:
            compile_prg(tmp_prg, flags)
        capture_vice(prg, png, ntsc=ntsc, sidmodel=sidmodel, cycles=cycles)

        if vmode == "gameplay":
            ok, msg = verify_gameplay_frame(png, name, expect_land=True)
        elif vmode == "storm":
            # During raging tempest, MC1 darkens from LtGreen to Green, so skip LtGreen check
            ok, msg = verify_gameplay_frame(png, name, expect_land=False)
        else:
            im = Image.open(png).convert("RGB")
            ok = im.size[0] >= 320 and im.size[1] >= 200
            msg = f"captured {im.size[0]}x{im.size[1]}"

        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {name:22s} : {msg}")
        results.append((name, png, ok))
        if not ok:
            all_ok = False

    # Clean up temporary build artifacts
    for ext in ("prg", "map", "lbl", "int", "asm"):
        p = os.path.join(OUT_DIR, f"_tmp_test.{ext}")
        if os.path.exists(p):
            os.remove(p)

    sheet_path = os.path.join(OUT_DIR, "contact_sheet.png")
    build_contact_sheet(results, sheet_path)
    print(f"\nContact sheet saved to: {sheet_path}")
    print(f"Summary: {sum(1 for _, _, ok in results if ok)}/{len(results)} scenarios passed.")
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
