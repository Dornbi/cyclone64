#!/usr/bin/env python3
"""
Extract map, 14-island world metadata, 3D tile stacks, slice graphics,
and visual reference previews from Cyclone_ZX-Spectrum_EN.zip.
"""
import io
import json
import os
import struct
import zipfile
from PIL import Image, ImageDraw

WORKSPACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZIP_PATH = os.path.join(WORKSPACE, "Cyclone_ZX-Spectrum_EN.zip")
OUT_DIR = os.path.join(WORKSPACE, "extracted")

# ZX Spectrum RGB palette (normal + bright)
ZX_PALETTE = [
    (0, 0, 0),       # 0 Black
    (0, 0, 215),     # 1 Blue
    (215, 0, 0),     # 2 Red
    (215, 0, 215),   # 3 Magenta
    (0, 215, 0),     # 4 Green
    (0, 215, 215),   # 5 Cyan
    (215, 215, 0),   # 6 Yellow
    (215, 215, 215), # 7 White
]
ZX_BRIGHT = [
    (0, 0, 0),
    (0, 0, 255),
    (255, 0, 0),
    (255, 0, 255),
    (0, 255, 0),
    (0, 255, 255),
    (255, 255, 0),
    (255, 255, 255),
]


def load_tap_memory():
    with zipfile.ZipFile(ZIP_PATH) as outer:
        tap_zip_data = outer.read("Cyclone (1985)(Vortex Software).TAP.zip")
        with zipfile.ZipFile(io.BytesIO(tap_zip_data)) as inner:
            data = inner.read(inner.namelist()[0])

    pos = 0
    blocks = []
    while pos + 2 <= len(data):
        blen = struct.unpack("<H", data[pos : pos + 2])[0]
        pos += 2
        blocks.append(data[pos : pos + blen])
        pos += blen

    scr_data = blocks[3][1:-1]  # Cyclone2: 6912-byte loading screen
    c3 = blocks[7][1:-1]        # Cyclone3: 40630 bytes @ 0x5FDC
    mem = bytearray(65536)
    mem[0x5FDC : 0x5FDC + len(c3)] = c3
    return mem, scr_data


def decode_loading_screen(scr_data, out_path):
    img = Image.new("RGB", (256, 192))
    px = img.load()
    for y in range(192):
        # Spectrum screen bitmap addressing
        hi = ((y & 0xC0) >> 3) | (y & 0x07)
        lo = ((y & 0x38) << 2)
        row_off = (hi << 8) | lo
        attr_row = 6144 + (y // 8) * 32
        for cx in range(32):
            byte = scr_data[row_off + cx]
            attr = scr_data[attr_row + cx]
            pal = ZX_BRIGHT if (attr & 64) else ZX_PALETTE
            ink = pal[attr & 7]
            pap = pal[(attr >> 3) & 7]
            for bit in range(8):
                px[cx * 8 + bit, y] = ink if (byte & (0x80 >> bit)) else pap
    img.save(out_path)


def parse_islands(mem):
    names = []
    p = 0x6A50
    cur = ""
    while mem[p] != 0xFF:
        b = mem[p]
        p += 1
        if b == 0xFD:
            names.append(cur + " ISLAND")
            cur = ""
        elif b == 0xFE:
            names.append(cur)
            cur = ""
        elif b == 0x3F:
            cur += " "
        else:
            cur += chr(b)

    atlas = mem[0x9300:0xDE00]
    islands = []
    for i in range(14):
        rec = mem[0xF230 + i * 20 : 0xF230 + (i + 1) * 20]
        maddr = rec[10] | (rec[11] << 8)
        my = (maddr - 0x9300) // 128
        mx = (maddr - 0x9300) % 128
        w = rec[14] * 4
        h = rec[15] * 4

        # In world cell coordinates (768x768), inner origin (rec[6], rec[8])
        # corresponds to viewport top-left when camera aligns with atlas (mx, my).
        # Because viewport is 23x23, the actual island cells sit starting at
        # world_x0 = sector_x*256 + rec[6], world_y0 = sector_y*256 + rec[8]
        # (shifted by camera offset; relative spacing between islands is exact).
        world_x = rec[0] * 256 + rec[6]
        world_y = rec[1] * 256 + rec[8]

        helipads = []
        buildings = 0
        trees = 0
        spires = 0
        land_cells = 0
        for dy in range(min(h, 150 - my)):
            for dx in range(min(w, 128 - mx)):
                t = atlas[(my + dy) * 128 + (mx + dx)]
                if t != 0:
                    land_cells += 1
                if t in (0x1F, 0x45, 0x75, 0x86):
                    helipads.append([dx, dy, t])
                elif t == 0xE8:
                    spires += 1
                elif t in (0x82, 0x83, 0x84, 0x85, 0x9D, 0x9E, 0xA0, 0xA1, 0xB4, 0xB5, 0xB7, 0xB8, 0xB9, 0xD2, 0xD3):
                    buildings += 1
                elif (0xA9 <= t <= 0xB1) or (0xC7 <= t <= 0xCF) or (0xDA <= t <= 0xE3):
                    trees += 1

        islands.append({
            "id": i,
            "name": names[i],
            "sector_x": rec[0],
            "sector_y": rec[1],
            "trigger_box": [rec[2], rec[3], rec[4], rec[5]],
            "inner_box": [rec[6], rec[7], rec[8], rec[9]],
            "world_x": world_x,
            "world_y": world_y,
            "atlas_x": mx,
            "atlas_y": my,
            "width": w,
            "height": h,
            "land_cells": land_cells,
            "has_helipad": len(helipads) > 0,
            "helipad_cells": len(helipads),
            "building_cells": buildings,
            "tree_cells": trees,
            "spire_cells": spires,
        })
    return islands


def classify_tile(t, north_slices):
    top = north_slices[-1] if north_slices else 0x80
    if t == 0x00:
        return "sea"
    if t in (0x1F, 0x45, 0x75, 0x86):
        return "helipad"
    if t == 0xE8:
        return "spire"
    if top in (0x2D, 0x2E):
        return "building"
    if top in (0x46, 0x47, 0x48, 0x49, 0x4A, 0x4D, 0x50, 0x53):
        return "tree"
    if top == 0x82 or (0x03 <= top <= 0x15):
        return "beach"
    if top == 0x83:
        return "concrete"
    if top == 0x81:
        return "grass_flat"
    if top == 0x88:
        return "apron"
    return "cliff_slope"


def parse_tiles(mem):
    atlas = mem[0x9300:0xDE00]
    used = sorted(set(atlas))
    tiles = {}
    all_slices = set()
    for t in used:
        ns, ss = [], []
        for l in range(8):
            v = mem[((0x63 + l) << 8) + t]
            if v == 0xFF:
                break
            ns.append(v)
            all_slices.add(v)
        for l in range(8):
            v = mem[((0x6C + l) << 8) + t]
            if v == 0xFF:
                break
            ss.append(v)
            all_slices.add(v)
        tiles[f"0x{t:02x}"] = {
            "id": t,
            "height": len(ns),
            "category": classify_tile(t, ns),
            "north_slices": ns,
            "south_slices": ss,
            "count": atlas.count(t),
        }
    return tiles, sorted(all_slices)


def draw_slice(img, mem, slice_id, x0, y0, scale=1):
    attr = mem[0xFE00 + slice_id]
    pal = ZX_BRIGHT if (attr & 64) else ZX_PALETTE
    ink = pal[attr & 7]
    pap = pal[(attr >> 3) & 7]
    solid = bool(slice_id & 0x80)
    rows = [0] * 8 if solid else mem[0xFA00 + slice_id * 8 : 0xFA00 + slice_id * 8 + 8]
    px = img.load()
    for ry in range(8):
        b = rows[ry]
        for rx in range(8):
            c = ink if (b & (0x80 >> rx)) else pap
            for sy in range(scale):
                for sx in range(scale):
                    px[x0 + rx * scale + sx, y0 + ry * scale + sy] = c


def render_slice_sheet(mem, slices, out_path):
    cols = 16
    rows = (len(slices) + cols - 1) // cols
    scale = 3
    cell = 8 * scale + 6
    img = Image.new("RGB", (cols * cell + 8, rows * cell + 8), (24, 24, 32))
    draw = ImageDraw.Draw(img)
    for idx, s in enumerate(slices):
        cx = (idx % cols) * cell + 4
        cy = (idx // cols) * cell + 4
        draw_slice(img, mem, s, cx + 2, cy + 2, scale=scale)
        draw.text((cx + 2, cy + 8 * scale + 2), f"{s:02X}", fill=(220, 220, 220))
    img.save(out_path)


def render_island_3d(mem, island, south=False, out_path=None):
    atlas = mem[0x9300:0xDE00]
    w, h = island["width"], island["height"]
    mx, my = island["atlas_x"], island["atlas_y"]
    margin_top = 8
    img = Image.new("RGB", (w * 8, (h + margin_top) * 8), ZX_BRIGHT[5])
    base_page = 0x6C if south else 0x63
    # Paint back-to-front so taller southern (or northern in South view) stacks occlude behind them
    for sy in range(h):
        for sx in range(w):
            ay = (h - 1 - sy) if south else sy
            ax = (w - 1 - sx) if south else sx
            if my + ay >= 150 or mx + ax >= 128:
                continue
            t = atlas[(my + ay) * 128 + (mx + ax)]
            for z in range(8):
                s = mem[((base_page + z) << 8) + t]
                if s == 0xFF:
                    break
                if s == 0xFE:
                    continue
                draw_y = (sy + margin_top - z) * 8
                if 0 <= draw_y < img.height:
                    draw_slice(img, mem, s, sx * 8, draw_y, scale=1)
    if out_path:
        img.save(out_path)
    return img


def render_world_overview(mem, islands, tiles, out_path):
    atlas = mem[0x9300:0xDE00]
    img = Image.new("RGB", (768, 768), (18, 52, 118))
    px = img.load()
    draw = ImageDraw.Draw(img)

    # Sector grid lines
    for g in (256, 512):
        draw.line([(g, 0), (g, 767)], fill=(34, 78, 150))
        draw.line([(0, g), (767, g)], fill=(34, 78, 150))

    cat_colors = {
        "sea": (18, 52, 118),
        "beach": (235, 210, 115),
        "apron": (215, 190, 90),
        "grass_flat": (68, 178, 72),
        "cliff_slope": (44, 132, 52),
        "tree": (26, 98, 40),
        "building": (210, 62, 54),
        "helipad": (248, 248, 255),
        "concrete": (195, 200, 210),
        "spire": (255, 180, 60),
    }

    for isl in islands:
        wx0 = isl["world_x"]
        wy0 = isl["world_y"]
        mx, my = isl["atlas_x"], isl["atlas_y"]
        for dy in range(isl["height"]):
            for dx in range(isl["width"]):
                if my + dy >= 150 or mx + dx >= 128:
                    continue
                t = atlas[(my + dy) * 128 + (mx + dx)]
                if t == 0:
                    continue
                info = tiles[f"0x{t:02x}"]
                c = cat_colors.get(info["category"], (80, 160, 80))
                # Lighten slightly by height tier for visual relief
                h_boost = (info["height"] - 2) * 14
                c = tuple(max(0, min(255, ch + h_boost)) for ch in c)
                x = wx0 + dx
                y = wy0 + dy
                if 0 <= x < 768 and 0 <= y < 768:
                    px[x, y] = c

        # Draw island label & helipad marker
        lx = max(4, min(640, wx0))
        ly = max(4, wy0 - 12)
        tag = f"{isl['name']}" + (" [H]" if isl["has_helipad"] else "")
        draw.text((lx + 1, ly + 1), tag, fill=(0, 0, 0))
        draw.text((lx, ly), tag, fill=(255, 255, 220))

    # Mark helicopter initial start on Base Island (sector 1,1 -> 256+57, 256+112)
    hx, hy = 256 + 57, 256 + 112
    draw.ellipse([(hx - 5, hy - 5), (hx + 5, hy + 5)], outline=(255, 60, 60), width=2)
    draw.text((hx + 8, hy - 4), "START", fill=(255, 110, 110))

    img.save(out_path)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    mem, scr_data = load_tap_memory()

    # Save raw 128x150 atlas
    with open(os.path.join(OUT_DIR, "atlas_128x150.bin"), "wb") as f:
        f.write(mem[0x9300:0xDE00])

    islands = parse_islands(mem)
    tiles, slices = parse_tiles(mem)

    with open(os.path.join(OUT_DIR, "islands.json"), "w") as f:
        json.dump(islands, f, indent=2)
    with open(os.path.join(OUT_DIR, "tiles.json"), "w") as f:
        json.dump(tiles, f, indent=2)

    decode_loading_screen(scr_data, os.path.join(OUT_DIR, "loading_screen.png"))
    render_slice_sheet(mem, slices, os.path.join(OUT_DIR, "slices_spectrum.png"))
    render_world_overview(mem, islands, tiles, os.path.join(OUT_DIR, "archipelago_world.png"))
    render_island_3d(mem, islands[5], south=False, out_path=os.path.join(OUT_DIR, "base_island_north.png"))
    render_island_3d(mem, islands[5], south=True, out_path=os.path.join(OUT_DIR, "base_island_south.png"))

    print(f"Extracted {len(islands)} islands, {len(tiles)} tile IDs, {len(slices)} unique 3D slices.")
    print(f"Helipad islands ({sum(1 for x in islands if x['has_helipad'])}):",
          [x["name"] for x in islands if x["has_helipad"]])


if __name__ == "__main__":
    main()
