#!/usr/bin/env python3
"""
Automated On-Target VICE x64sc Visual, Alignment & Playability Test Suite for Cyclone (C64).
See docs/testing_plan.md for the full 16-scenario specification covering:
  1. Visual stability & zero unexpected color changes / raster IRQ artifacts
  2. Graphics & sprite alignment (shadows, waving survivors, medical crates, winch, South view)
  3. Playability & mission solvability (real C64 fuel range, 120-entity audit, full 5-crate win)
  4. True 320x200 VIC-II Bitmap Tactical Radar Map & lossless charset restoration
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
_NOFOCUS_X64SC = os.path.expanduser("~/.local/vice-nofocus/bin/x64sc")
X64SC = os.environ.get("X64SC") or (
    _NOFOCUS_X64SC if os.access(_NOFOCUS_X64SC, os.X_OK) else "/Applications/vice-arm64-gtk3-3.10/bin/x64sc"
)

SRCS = [
    "src/main.c", "src/world.c", "src/raster.c", "src/scroller.c",
    "src/helicopter.c", "src/entities.c", "src/cyclone.c",
    "src/radar_map.c", "src/hud.c", "src/audio.c", "src/title.c",
]


def compile_prg(out_prg, extra_flags=None):
    cmd = [
        OSCAR64, f"-ii={OSCAR64_INC}",
        "-Os", "-Oo", "-tm=c64", "-n", "-xz",
        f"-o={out_prg}",
    ] + (extra_flags or []) + SRCS
    res = subprocess.run(cmd, cwd=WORKSPACE, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"COMPILE FAILED for {out_prg}:\n{res.stdout}\n{res.stderr}")
        sys.exit(1)


def capture_vice(prg_path, png_path, ntsc=False, sidmodel=1, cycles=6000000):
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
    return rgb[1] > 180 and rgb[1] > rgb[0] + 20 and rgb[1] > rgb[2] + 20


def check_border_black(im):
    """On-target C64 test hooks paint vic.color_border = RED on any assertion failure!"""
    w, h = im.size
    px = im.load()
    bad = 0
    for x in range(8, w - 8, 8):
        if not is_black(px[x, 6]):
            bad += 1
        if not is_black(px[x, h - 6]):
            bad += 1
    return bad


def verify_gameplay_frame(png_path, expect_land=True):
    im = Image.open(png_path).convert("RGB")
    w, h = im.size
    px = im.load()

    border_err = check_border_black(im)
    if border_err > 0:
        return False, f"ON-TARGET C64 ASSERTION FAILED (non-black border pixels={border_err})!"

    pf_top = None
    for y in range(h):
        non_blk = sum(1 for x in range(64, w - 64) if not is_black(px[x, y]))
        if non_blk > 40:
            pf_top = y
            break
    if pf_top is None:
        return False, "Blank screen!"

    bezel_y0 = pf_top + 148
    bezel_y1 = pf_top + 164
    bezel_nonblack = 0
    for y in range(bezel_y0, min(bezel_y1, h)):
        for x in range(40, w - 40):
            if not is_black(px[x, y]):
                bezel_nonblack += 1

    hud_top = None
    for y in range(bezel_y0, h):
        non_blk = sum(1 for x in range(40, w - 40) if not is_black(px[x, y]))
        if non_blk > 10:
            hud_top = y
            break
    if hud_top is None:
        return False, "HUD missing!"

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

    hud_bg_black = 0
    hud_bg_tot = 0
    for y in range(hud_top, min(hud_top + 30, h)):
        for x in range(33, 39):
            hud_bg_tot += 1
            if is_black(px[x, y]):
                hud_bg_black += 1

    mc_ltgreen = 0
    for y in range(pf_top, min(pf_top + 140, h)):
        for x in range(48, w - 48):
            if is_light_green(px[x, y]):
                mc_ltgreen += 1

    errs = []
    if bezel_nonblack > 0:
        errs.append(f"bezel_nonblack={bezel_nonblack}")
    if hud_row0_lines < 7:
        errs.append(f"hud_row0_lines={hud_row0_lines}")
    if hud_span < 30:
        errs.append(f"hud_span={hud_span}px")
    if hud_bg_tot > 0 and hud_bg_black < hud_bg_tot * 0.9:
        errs.append(f"hud_bg_black={hud_bg_black}/{hud_bg_tot}")
    if expect_land and mc_ltgreen < 500:
        errs.append(f"mc_ltgreen={mc_ltgreen}")

    info = f"bezel=0, hud={hud_row0_lines}/{hud_span}px, mc_ltg={mc_ltgreen}"
    return (len(errs) == 0), ("; ".join(errs) if errs else info)


def verify_bitmap_radar(png_path):
    im = Image.open(png_path).convert("RGB")
    if check_border_black(im) > 0:
        return False, "Non-black border!"
    w, h = im.size
    px = im.load()
    # Count bright green island pixels inside the 320x200 bitmap viewport
    ltg = sum(1 for y in range(50, h - 50) for x in range(50, w - 50) if is_light_green(px[x, y]))
    if ltg < 200:
        return False, f"Bitmap islands missing (ltg={ltg})"
    return True, f"320x200 bitmap OK (island_px={ltg})"


def verify_color_stability(png_t0, png_t8):
    """Compare terrain palette stability between t=0s and t=8s (must not shift MedGrey <-> DarkGrey!)."""
    a = Image.open(png_t0).convert("RGB")
    b = Image.open(png_t8).convert("RGB")
    pa, pb = a.load(), b.load()
    w, h = a.size
    # Count Med Grey cliff pixels (R≈G≈B in 105..145) across playfield in both frames
    def count_med_grey(p):
        return sum(
            1 for y in range(40, 180) for x in range(48, w - 48)
            if 100 <= p[x, y][0] <= 150 and abs(p[x, y][0] - p[x, y][1]) < 15 and abs(p[x, y][1] - p[x, y][2]) < 15
        )
    mg0, mg8 = count_med_grey(pa), count_med_grey(pb)
    if mg0 < 400 or abs(mg0 - mg8) > 120:
        return False, f"MedGrey cliff drift: t0={mg0}, t8s={mg8}"
    return True, f"MedGrey stable: t0={mg0}, t8s={mg8}"


def build_contact_sheet(entries, out_path):
    if not entries:
        return
    cols = 4
    rows = (len(entries) + cols - 1) // cols
    tw, th = 320, 226
    pad = 22
    sheet = Image.new("RGB", (cols * tw + (cols + 1) * 8, rows * (th + pad) + 8), (22, 22, 26))
    draw = ImageDraw.Draw(sheet)
    for idx, (label, path, ok) in enumerate(entries):
        r, c = divmod(idx, cols)
        x0 = 8 + c * (tw + 8)
        y0 = 8 + r * (th + pad)
        im = Image.open(path).convert("RGB").resize((tw, th), Image.NEAREST)
        sheet.paste(im, (x0, y0 + pad))
        col = (100, 240, 120) if ok else (255, 90, 90)
        draw.text((x0 + 4, y0 + 4), f"{'PASS' if ok else 'FAIL'} {label}", fill=col)
    sheet.save(out_path)


def main():
    print(f"Using VICE: {X64SC}")
    main_prg = os.path.join(WORKSPACE, "build", "cyclone.prg")
    compile_prg(main_prg)

    scenarios = [
        # (name, prg_flags, ntsc, sidmodel, cycles, verify_mode)
        ("01_title_pal_8580",     None,                                   False, 1,  5800000, "screen"),
        ("02_title_ntsc_6581",    None,                                   True,  0,  5800000, "screen"),
        ("03_help_screen",        ["-DSKIP_TITLE", "-DTEST_SCENARIO=3"],  False, 1,  5800000, "screen"),
        ("04_color_t0",           ["-DSKIP_TITLE", "-DTEST_SCENARIO=4"],  False, 1,  6000000, "gameplay"),
        ("05_color_t8s",          ["-DSKIP_TITLE", "-DTEST_SCENARIO=5"],  False, 1, 14000000, "gameplay"),
        ("06_8way_colorram_pal",  ["-DSKIP_TITLE", "-DTEST_SCENARIO=6"],  False, 1, 18000000, "gameplay"),
        ("07_diag_nw_ntsc",       ["-DSKIP_TITLE", "-DTEST_SCENARIO=7"],  True,  0,  8000000, "gameplay"),
        ("08_south_view_flight",  ["-DSKIP_TITLE", "-DTEST_SCENARIO=8"],  False, 1,  8000000, "gameplay"),
        ("09_bitmap_radar_map",   ["-DSKIP_TITLE", "-DTEST_SCENARIO=9"],  False, 1, 11000000, "radar"),
        ("10_radar_roundtrip",    ["-DSKIP_TITLE", "-DTEST_SCENARIO=10"], False, 1,  6500000, "gameplay"),
        ("11_storm_cyclone",      ["-DSKIP_TITLE", "-DTEST_SCENARIO=11"], False, 1,  5500000, "storm"),
        ("12_crate_winch_align",  ["-DSKIP_TITLE", "-DTEST_SCENARIO=12"], False, 1,  7000000, "gameplay"),
        ("13_survivor_align",     ["-DSKIP_TITLE", "-DTEST_SCENARIO=13"], False, 1,  7000000, "gameplay"),
        ("14_kokola_shadow",      ["-DSKIP_TITLE", "-DTEST_SCENARIO=14"], False, 1,  7500000, "gameplay"),
        ("15_entity_audit_120",   ["-DSKIP_TITLE", "-DTEST_SCENARIO=15"], False, 1,  7500000, "gameplay"),
        ("16_fuel_and_full_win",  ["-DSKIP_TITLE", "-DTEST_SCENARIO=16"], False, 1, 10000000, "gameplay"),
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
            ok, msg = verify_gameplay_frame(png, expect_land=True)
        elif vmode == "storm":
            ok, msg = verify_gameplay_frame(png, expect_land=False)
        elif vmode == "radar":
            ok, msg = verify_bitmap_radar(png)
        else:
            im = Image.open(png).convert("RGB")
            ok = check_border_black(im) == 0 and im.size[0] >= 320
            msg = f"captured {im.size[0]}x{im.size[1]}"

        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {name:22s} : {msg}")
        results.append((name, png, ok))
        if not ok:
            all_ok = False

    # Cross-frame check: verify zero unexpected cliff shadow color shift between t=0s and t=8s!
    st_ok, st_msg = verify_color_stability(
        os.path.join(OUT_DIR, "04_color_t0.png"),
        os.path.join(OUT_DIR, "05_color_t8s.png"),
    )
    print(f"[{'PASS' if st_ok else 'FAIL'}] {'color_stability_t0_t8s':22s} : {st_msg}")
    if not st_ok:
        all_ok = False

    for ext in ("prg", "map", "lbl", "int", "asm"):
        p = os.path.join(OUT_DIR, f"_tmp_test.{ext}")
        if os.path.exists(p):
            os.remove(p)

    sheet_path = os.path.join(OUT_DIR, "contact_sheet.png")
    build_contact_sheet(results, sheet_path)
    print(f"\nContact sheet saved to: {sheet_path}")
    print(f"Summary: {sum(1 for _, _, ok in results if ok)}/{len(results)} scenarios + stability check {'PASSED' if all_ok else 'FAILED'}.")
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
