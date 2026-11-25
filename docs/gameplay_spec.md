# Cyclone (ZX Spectrum -> C64) Reverse-Engineered Technical Specification

Extracted directly from `Cyclone (1985)(Vortex Software).tap` (`Cyclone3` loaded at `$5FDC–$FE91`) via [extract_spectrum.py](../tools/extract_spectrum.py).

## 1. World Coordinate System & Packed 14-Island Atlas
- **World Space**: **768 × 768 tiles**, organized as a **3 × 3 sector grid** (`sector_x ∈ 0..2`, `sector_y ∈ 0..2`, each 256 × 256 tiles).
- **Packed Tile Atlas**: All 14 islands are tightly packed inside a **128 × 150 byte array** (`19,200 bytes`, Spectrum `$9300–$DDFF`, exported to `extracted/atlas_128x150.bin`). Open ocean between islands consumes zero map RAM.
- **Island Descriptor Table** (Spectrum `$F230`, 14 records × 20 bytes, exported to `extracted/islands.json`):

| ID | Name | Sector | World Origin | Atlas Origin | Size (W×H) | Helipad? |
|---:|------|:------:|:------------:|:------------:|:----------:|:--------:|
| 0 | **BANANA ISLAND** | (2, 1) | (626, 412) | (0, 0) | 52×24 | No |
| 1 | **ORTE ROCKS** | (1, 2) | (414, 540) | (0, 20) | 48×28 | No |
| 2 | **KOKOLA ISLAND** | (1, 0) | (365, 176) | (0, 44) | 48×28 | **Yes** |
| 3 | **LAGOON ISLAND** | (0, 0) | (138, 212) | (0, 70) | 64×28 | No |
| 4 | **PEAK ISLAND** | (2, 0) | (574, 116) | (0, 96) | 40×28 | **Yes** |
| 5 | **BASE ISLAND** | (1, 1) | (306, 364) | (0, 121) | 80×28 | **Yes (Start/Return)** |
| 6 | **GILLIGANS ISLAND** | (1, 0) | (306, 116) | (84, 0) | 40×28 | **Yes** |
| 7 | **RED ISLAND** | (1, 2) | (398, 604) | (88, 25) | 36×28 | **Yes** |
| 8 | **SKEG ISLAND** | (0, 2) | (134, 540) | (88, 53) | 36×28 | **Yes** |
| 9 | **BONE ISLAND** | (1, 0) | (450, 152) | (67, 74) | 60×28 | No |
| 10 | **GIANTS GATEWAY** | (1, 1) | (430, 296) | (55, 0) | 28×40 | No |
| 11 | **CLAW ISLAND** | (1, 1) | (474, 476) | (53, 40) | 32×32 | No |
| 12 | **LUKELAND ISLES** | (0, 2) | (70, 588) | (40, 102) | 36×32 | No |
| 13 | **ENTERPRISE ISLAND** | (0, 1) | (90, 460) | (79, 102) | 44×48 | **Yes** |

- **Initial Helicopter Position**: Sector `(1, 1)`, pos `(57, 112)` -> world `(313, 368)`, resting directly on **BASE ISLAND**'s western 4×3 white helipad (`tile 0x1F`).

## 2. 3D Stacked-Slice Projection Engine
- Across the entire archipelago, **130 unique tile IDs** are used, built from only **62 unique 8×8 graphic slices** (`extracted/tiles.json`, `extracted/slices_spectrum.png`).
- Each tile ID defines a vertical stack of `1..8` slices (`height = z`), stored separately for **North View** (`$6300–$6AFF`) and **South View** (`$6C00–$73FF`), terminated by `$FF`:
  - `z = 1`: Ocean (`$00`), sandy beaches (`$04–$0E`)
  - `z = 2`: Tier 1 lowlands (`$10–$24`), Base helipad (`$1F`), taxiway apron (`$24`)
  - `z = 3`: Tier 2 hills (`$30–$4F`), mid-elevation helipads (`$45`)
  - `z = 4`: Tier 3 highlands (`$58–$86`), low buildings (`$82–$85`), high helipads (`$75`, `$86`)
  - `z = 5`: Tier 4 peaks (`$8C–$A2`), mid-height trees (`$A9–$B1`), buildings (`$9D–$A1`)
  - `z = 6–7`: Tall buildings (`$B4–$B9`, `$D2–$D3`) and tall trees (`$C7–$CF`, `$DA–$E3`)
  - `z = 8`: Tall spires/lighthouses (`$E8` — 3 across the map)
- **Rendering rule**: Paint back-to-front along the camera axis; each cell `(x, y)` writes slice `z` at screen row `y_screen - z`. In **South View**, traverse both X and Y in reverse (`180°` rotation) and read the South slice table so cliffs, overhangs, and shadows flip cleanly, revealing crates/survivors hidden behind hills!
- **C64 Adaptation**: Every one of the 62 Spectrum slices maps to upgraded **detailed tropical multicolor C64 characters** (with height-tier color/texture differentiation so lowlands, palm groves, striated rocky cliffs, sandy beaches, red-roofed outposts, blinking helipads, and foaming surf look richly detailed at 50/60 FPS).

## 3. Core Gameplay Constants & Systems
- **Objective**: Locate and winch **5 medical crates** scattered across the islands, optionally rescue **stranded survivors** for bonus points, and land back at **BASE ISLAND** before the timer expires.
- **Lives**: **3 helicopters** (`$9245` compares against `3`).
- **Altitude & Collision**:
  - Helicopter altitude ranges from ground/sea level (`0`) up to cruising ceiling (`8` height levels, matching max spire height).
  - Descending onto a **helipad** (`$1F`, `$45`, `$75`, `$86`) at low horizontal speed lands safely and **refuels**.
  - Colliding horizontally with terrain/trees/buildings taller than current altitude, hitting open sea, or slamming down too fast destroys a helicopter.
- **Automatic Winch**: Hovering directly over a crate or survivor at low clearance above its terrain height deploys the winch cable and hoists it aboard.
- **Cyclone AI & Wind Force** (Spectrum `$9034–$9178`):
  - Tactical map grid divides the `768×768` world by `32` (`>> 5`) into a **`24×24`** grid (playable interior `1..21` × `1..20`).
  - Cyclone moves 1 tactical cell every 24 ticks along an 8-direction vector, re-rolling bias every 8 steps or when touching map boundaries.
  - `dist = max(|heli_grid_x - cyc_grid_x|, |heli_grid_y - cyc_grid_y|)` clamped to `0..15`.
  - `wind_force = 16 - dist` (bar meter `1..16`).
  - `dist < 6`: Triggers **WIND DANGER** alert, heavy aerodynamic drift/turbulence, darkening storm palette, rain/lightning FX, and howling SID band-pass wind.
- **Low-Flying Aircraft**: Spawn over open ocean corridors with a **`BEWARE AIRCRAFT`** HUD collision warning and mid-air collision check at cruising altitude.
- **Fuel & Time**: Continuous fuel drain while airborne (higher at full thrust/climb) with **`NO FUEL`** warning; countdown mission clock with **`NO TIME`** failure state.
