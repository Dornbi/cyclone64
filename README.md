# Cyclone — Commodore 64 Edition

A native **Commodore 64** port of the 1985 ZX Spectrum 3D helicopter rescue classic **Cyclone** (Vortex Software), built from a clean-room reverse engineering of the original Z80 archipelago map and gameplay logic and re-engineered in C ([Oscar64](https://github.com/drmortalwombat/oscar64)) to take full advantage of C64 **VIC-II** graphics and **SID** audio.

| Title Screen | 3D Tropical Archipelago |
|---|---|
| ![Title Screen](docs/screenshots/title.png) | ![Base Island Gameplay](docs/screenshots/gameplay.png) |

| Tactical Radar | Raging Cyclone Tempest |
|---|---|
| ![320x200 Tactical Map](docs/screenshots/radar_map.png) | ![Cyclone Storm](docs/screenshots/storm.png) |

---

## About this game

- This was done as a fully AI port with Antigravity and Genini 4 Neon.
- Extracts the exact **768 × 768** 14-island tile atlas, 3D vertical slice stacks, and North/South viewpoint geometry directly from the original release (documented in [docs/gameplay_spec.md](docs/gameplay_spec.md)).
- **Single-Load 50 KB `.prg`, No Cartridges or Expansions**:
  - **50/60 Hz 8-Way Hardware Scrolling**: Double-buffered VIC-II Multicolor playfield 18-row Color RAM shifting and IRQ split with a 4-row Hi-Res Cockpit HUD across both PAL and NTSC.
  - **Hardware Sprites**
  for spinning rotor, helicopter, ground shadow.
  - **Tactical Radar (`M` / `Space`)**.
  - **3-Voice Filtered SID Audio (MOS 6581 & 8580 Auto-Calibrated)**: Auto-detects the SID revision at boot to calibrate filter cutoffs across both chips.

---

## How to Play

### Play in Your Browser

There are many online and offline C64 emulators. One of the easiest is https://ty64.krissz.hu/ — click the web icon and paste:

```
https://github.com/Dornbi/cyclone64/raw/refs/heads/main/bin/cyclone.prg
```

Alternatively, download the release binary directly and upload or load it into any online or offline C64 emulator:

- [cyclone.prg](bin/cyclone.prg) — Single-load C64 PRG (stock 64 KB, auto-detects PAL/NTSC and SID 6581/8580)

Emulators:

- https://ty64.krissz.hu/ (online)
- https://c64online.com/ (online)
- https://retrogamecoders.com/c64-emulator/ (online)
- [VICE](https://vice-emu.sourceforge.io/) (offline)

### Mission Objectives

1. **Take off** from the Base Island helipad (`W` or `Fire + Up`).
2. **Recover all 5 Medical Crates** hidden across outer islands (hover low and slow over a flashing crate or waving **Survivor** to automatically deploy the winch) before the **08:00** countdown expires.
3. **Watch your fuel (`FUL`) and the Cyclone (`WND`)**: Use the live **320×200 Tactical Radar (`M` / `Space`)** to track the storm and plan refueling stops at any of the 7 white island helipads (`BASE`, `KOKOLA`, `PEAK`, `GILLIGANS`, `RED`, `SKEG`, `ENTERPRISE`).
4. **Return to Base Island** and land gently on its helipad with all 5 crates to win.

### Controls

| Input | Action |
|---|---|
| **Joystick Port 2** or **`I` / `J` / `K` / `L`** | Fly helicopter (Classic rotate+thrust or Direct 8-Way Arcade mode) |
| **Fire + Up / Down** or **`W` / `S`** | Climb / Descend altitude |
| **Quick-tap Fire** or **`Return`** | Flip 3D Camera View (`NORTH` ↔ `SOUTH`) |
| **`M`** or **`Space`** | Open / Close 320×200 Bitmap Tactical Radar Map |
| **`C`** | Toggle Control Mode (`CLASSIC` / `ARCADE`) |
| **`V`** | Toggle SID Audio Mute (Title Menu & in-flight) |
| **`H`** | Open Controls & Mission Help Screen (Title Menu & in-flight) |
| **`1`–`7`** | Quick-warp to the 7 helipad islands |
| **`T`** / **`Run/Stop`** *(after Win / Game Over)* | Return to Title Screen |

---

## Stock 64 KB C64 Memory Map

| Address Range | Contents |
|---|---|
| `$0801 – $4FFF` | Startup stub + Core 3D engine, scroller, helicopter physics, entities, HUD & SID audio |
| `$5000 – $9AFF` | Full 19,200-byte dense 14-island tile atlas |
| `$9B00 – $BFFF` | 3D tile slice LUTs + Cyclone AI, 320×200 Bitmap Radar, Title & Help screens |
| `$C000 – $CBFF` | Boot-only LZO payloads (unpacked at startup, then reclaimed as Screen A, Screen B & HUD/Map Screen) |
| `$CC00 – $CFFF` | Compact bitmap primitives, upper data/BSS & stack |
| `$D000 – $DFFF` | Hardware I/O (CPU) / 4 KB Static Sprite Bank under I/O (VIC-II Bank 3) |
| `$E000 – $FF3F` | Tropical Multicolor Charset (`$E000`) & HUD Charset (`$F000`); temporarily serves as 8,000-byte Bitmap RAM during Tactical Map |

---

## Repository Layout

- `bin/` — Pre-built release binary ([cyclone.prg](bin/cyclone.prg))
- `src/` — C64 C source files (`main.c`, `world.c`, `raster.c`, `scroller.c`, `helicopter.c`, `entities.c`, `cyclone.c`, `radar_map.c`, `hud.c`, `audio.c`, `title.c`, `memmap.h`, `test_hooks.inc`)
- `tools/` — Python asset & verification pipeline (`extract_spectrum.py`, `build_assets.py`, `build_sprites.py`, `run_tests.py`)
- `generated/` — Pre-built C64 charsets, sprite bank, tile tables, and dense archipelago atlas
- `extracted/` — Extracted Spectrum reference maps, JSON descriptors, and preview renders
- `docs/` — Reverse-engineered specification ([gameplay_spec.md](docs/gameplay_spec.md)), on-target testing plan ([testing_plan.md](docs/testing_plan.md)), and showcase screenshots

---

## Building, Running & Testing

Requires [Oscar64](https://github.com/drmortalwombat/oscar64), Python 3 (with Pillow), and [VICE](https://vice-emu.sourceforge.io/) (`x64sc`).

```bash
make all        # Build stock 64 KB single-load build/cyclone.prg (~50 KB)
make release    # Build and update bin/cyclone.prg
make run        # Launch in VICE x64sc (PAL 50 Hz)
make run-ntsc   # Launch in VICE x64sc (NTSC 60 Hz)
make test       # Run 16-scenario on-target VICE visual, alignment & playability suite
make assets     # Regenerate C64 charsets, sprites & tile tables from extracted/
```

See [docs/testing_plan.md](docs/testing_plan.md) for details on the 16 automated on-target VICE tests (color stability, 8-way 720-cell Color RAM integrity, sprite/shadow/survivor alignment, fuel reachability, and full 5-crate mission win).

---

## License

Source code and tooling are released under the **BSD 3-Clause License** — see [LICENSE](LICENSE). Original *Cyclone* (1985) concept and archipelago design by Vortex Software.
