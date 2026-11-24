# Cyclone — Commodore 64 Edition

A native **Commodore 64 (stock 64 KB, PAL & NTSC)** port of the 1985 ZX Spectrum classic **Cyclone** (Costa Panayi / Vortex Software), engineered from the original reverse-engineered 14-island archipelago data and crafted to take full advantage of C64 **VIC-II** graphics and **SID** audio.

## Features

- **Authentic 768 × 768 Archipelago**: Exact 14-island tile atlas, 3D vertical slice projection, and North/South viewpoint flipping extracted directly from the 1985 ZX Spectrum original.
- **50/60 Hz Hardware Scrolling & Raster Split**: Double-buffered 8-way VIC-II smooth scroll with unrolled Color RAM shifting and cycle-exact FLD bezel locking a 4-row Hi-Res Cockpit HUD across both PAL and NTSC.
- **8-Layer Hardware Sprite System (62 Static Frames under I/O `$D000–$DFFF`)**:
  - 16-angle Multicolor Rescue Helicopter + 4-frame Hi-Res spinning rotor + 16-angle Dark Grey terrain-hugging ground shadow
  - Animated Winch Cable, pulsing Medical Crates & waving Survivors
  - 2×-expanded swirling Red Cyclone Funnel, driving Storm Rain streaks & Low-Flying Jets
  - Live Tactical Radar Icons (Helicopter, Cyclone Danger Ring, Base Beacon)
- **Full Rescue Gameplay Loop**: Recover 5 Medical Crates across outer islands and rescue stranded Survivors before the `08:00` countdown expires, refueling at any of the 7 island helipads and returning safely to Base Island.
- **Dynamic Cyclone AI & Weather**: Wandering storm front with aerodynamic wind drift, eye downdrafts, darkening tempest palette, lightning flashes, and open-sea aircraft hazards.
- **Live 320×200 Bitmap Tactical Radar (`M` / `Space`)**: True VIC-II Hi-Res Bitmap Mode (`BMM=1`, `$E000–$FF3F`) rendering all 14 pixel-accurate island coastlines, 7 white helipad crosses, 3×3 sector grid, pulsing Helicopter/Cyclone/Base hardware sprites, and lossless charset restoration on exit.
- **3-Voice Filtered SID Audio Engine (6581 & 8580 Auto-Calibrated)**:
  - **Voice 1**: Speed-coupled helicopter rotor blade-pass synthesis
  - **Voice 2**: Resonant Band-Pass/Low-Pass Cyclone wind & booming thunder rolls
  - **Voice 3**: Priority SFX engine (winch servo, crate/survivor chimes, Doppler jet flyby, fuel klaxon, multi-stage crash explosion, victory fanfare)
  - **Title & Radar Audio**: Original 3-voice SID rescue theme on the Title Screen and atmospheric radar sonar ping on the Tactical Map.

## Controls

| Input | Action |
|---|---|
| **Joystick Port 2** or **`I` / `J` / `K` / `L`** | Fly helicopter (Classic rotate+thrust or Direct 8-Way Arcade mode) |
| **Fire + Up / Down** or **`W` / `S`** | Climb / Descend altitude |
| **Quick-tap Fire** or **`Return`** | Flip 3D Camera View (`NORTH` ↔ `SOUTH`) |
| **`M`** or **`Space`** | Toggle Live Tactical Archipelago Radar Map |
| **`C`** | Toggle Control Mode (`CLASSIC` / `ARCADE`) |
| **`V`** | Toggle SID Audio Mute (works in Title Menu & in-flight) |
| **`H`** | Show Controls & Mission Help Screen (works in Title Menu & in-flight) |
| **`1`–`7`** | Quick-warp to the 7 helipad islands |
| **`T`** / **`Run/Stop`** *(after Game Over / Win)* | Return to Title Screen |

## Building & Running

Requires [Oscar64](https://github.com/drmortalwombat/oscar64), Python 3 (with Pillow), and [VICE](https://vice-emu.sourceforge.io/) (`x64sc`, `c1541`).

```bash
make all        # Build single-load build/cyclone.prg (50 KB, stock 64 KB C64)
make run        # Launch in VICE x64sc (PAL)
make run-ntsc   # Launch in VICE x64sc (NTSC)
make test       # Run 16-scenario on-target VICE test suite (see docs/testing_plan.md)
make screenshot # Capture PAL/NTSC Title & Gameplay verification screenshots
```
