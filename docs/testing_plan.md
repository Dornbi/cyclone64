# Cyclone (C64) — On-Target Verification & Testing Plan

This plan defines the automated, on-target test suite executed directly on the compiled **Commodore 64 `.prg` binary** inside cycle-exact **VICE `x64sc`** (PAL 50 Hz / MOS 8580 and NTSC 60 Hz / MOS 6581) via `make test` ([run_tests.py](../tools/run_tests.py) + [test_hooks.inc](../src/test_hooks.inc)).

> Production builds (`make all`) exclude `TEST_SCENARIO` completely so zero test code enters `build/cyclone.prg`.

---

## Pillar 1 — Visual Stability & Zero Unexpected Color Changes / Artifacts

| ID | Scenario | On-Target C64 Execution | Automated Assertions |
|---|---|---|---|
| **T01** | `01_title_pal_8580` | Normal boot on PAL + MOS 8580 | Title logo, animated sprites, `PAL 50HZ` & `SID 8580` auto-detection render cleanly |
| **T02** | `02_title_ntsc_6581` | Normal boot on NTSC + MOS 6581 | Title logo, animated sprites, `NTSC 60HZ` & `SID 6581` auto-detection render cleanly |
| **T03** | `03_help_screen` | Open Help screen (`H`) | All key bindings (`I/J/K/L`, `W/S`, `V`, `H`, `RETURN`, `M/SPACE`, `C`, `1-7`) legible with zero missing glyphs |
| **T04–T05** | `04_color_t0` vs `05_color_t8s` | Boot at Base Island, compare frame `t = 0s` vs `t = 8s` (400 frames) | **Zero unexpected palette/shadow color changes** over time (cliff shadows stay consistent Medium Grey `PF_MC2_DEFAULT`, pixel diff on terrain outside ocean waves/rotor is zero) |
| **T06** | `06_8way_colorram_pal` | Continuous flight across **all 8 directions** (N, NE, E, SE, S, SW, W, NW) crossing coarse tile boundaries & `yscroll = 0..7` | **On-target 720-cell Color RAM audit** (`(COLOR_RAM[i] & 0x0F) == (CHAR_COLORS[front[i]] & 0x0F)` for all `i = 0..719`), zero red-border error, `bezel_err == 0`, `hud_row0 == 7px`, `hud_span == 31px`, multicolor terrain intact |
| **T07** | `07_diag_nw_ntsc` | Continuous diagonal NW (`off = -41`) flight on **NTSC 60 Hz** | Same on-target 720-cell Color RAM audit + NTSC VBlank tear-free bezel & 4-row HUD check |
| **T08** | `08_south_view_flight` | Flip to **South View** (`scroller_toggle_view`) + diagonal flight | On-target 720-cell Color RAM audit, smooth mirrored scrolling (`calc_xscroll`/`calc_yscroll`), zero artifacts |
| **T09** | `09_bitmap_radar_map` | Open **320×200 True VIC-II Bitmap Tactical Radar** | Pixel-accurate coastlines for all 14 islands, 7 white helipad crosses, legible labels, pulsing Heli/Cyclone/Base sprites |
| **T10** | `10_radar_roundtrip` | Enter 320×200 Bitmap Radar (backing up charsets into `$C000/$C400`), exit back to 3D flight | **On-target byte-for-byte charset restore check**, clean raster IRQ resume, `bezel_err == 0`, full multicolor playfield & 4-row HUD intact |
| **T11** | `11_storm_cyclone` | Position Cyclone right next to Base Island | 2× swirling red funnel, thin diagonal rain streaks, darkened tempest sea & max `WND` danger bar |

---

## Pillar 2 — Graphics & Sprite Alignment

| ID | Scenario | On-Target C64 Execution | Automated Assertions |
|---|---|---|---|
| **T12** | `12_crate_winch_align` | Position helicopter directly over Crate #0 at hover height (`ground_alt + 24`), run winch hoist | Visual inspection + on-target assertion that crate sits on `SURF_FLAT` at exact `ground_alt`, winch cable connects helicopter bottom to crate, and crate lifts smoothly into `CRATE HOISTED!` |
| **T13** | `13_survivor_align` | Position helicopter directly over Survivor #0 on an outer island at hover height, capture before & during winch | Visual verification that waving survivor sprite aligns cleanly on flat island terrain (not sea/cliff), shadow sits directly on terrain underneath, and winch awards `SURVIVOR +250!` |
| **T14** | `14_kokola_shadow_align` | Land on elevated **Kokola Island helipad** (`ground_alt = 24`) and continuously hold `S` (descend) for 36 frames | On-target assertion `heli_state == HELI_STATE_LANDED && heli_alt == heli_ground_alt` (zero underground sink!) + visual confirmation helicopter rests directly atop its dark-grey shadow on the white helipad |

---

## Pillar 3 — Playability, Fuel Reachability & Mission Solvability

| ID | Scenario | On-Target C64 Execution | Automated Assertions |
|---|---|---|---|
| **T15** | `15_entity_audit_seeds` | On-target C64 loop calling `entities_init(seed)` for **8 seeds (`1..8` = 40 Crates + 80 Survivors = 120 spawns)** | Verifies **100% of spawned Crates & Survivors**: (a) sit on `SURF_FLAT` (never sea/obstacle), (b) have exact `ground_alt == world_get_ground_alt(wx, wy)`, (c) lie within playable archipelago bounds, and (d) have no duplicate overlapping positions |
| **T16** | `16_fuel_and_full_win` | **Part A (Fuel Audit)**: Fly continuous Arcade cruise physics from Base Island for 240 frames, measure real pixels flown vs `game_fuel` burned directly on the C64 CPU, and verify full-tank range exceeds the **4,795 px Banana Island round-trip** (longest no-helipad island hop) by **≥ 30% safety margin**.<br>**Part B (Full Mission Win)**: Visit and winch all 5 Medical Crates via the real winch state machine, return to Base Island helipad, land safely, and assert `crates_collected == 5 && game_state == GAME_WIN` | On-target `MISSION CLEAR!` with black border (zero failures), `bezel_err == 0`, all 5 green crate icons lit, and valid 6-digit score |

### Island-to-Helipad Reachability Table (768 × 768 Archipelago)

The 7 refueling helipads are on **Base**, **Kokola**, **Peak**, **Gilligans**, **Red**, **Skeg**, and **Enterprise**.

| No-Helipad Island | Nearest Refueling Helipad | One-Way Distance | Round-Trip Distance | Tank Usage (at ~8,000 px Full-Tank Range) |
|---|---|---:|---:|---:|
| **Banana Island** *(worst case)* | Peak Island | 299.7 tiles (2,397 px) | **4,795 px** | **~60%** (40% reserve for winch/wind) |
| **Lagoon Island** | Gilligans Island | 183.2 tiles (1,465 px) | 2,931 px | ~36% |
| **Claw Island** | Red Island | 146.1 tiles (1,169 px) | 2,338 px | ~29% |
| **Giants Gateway** | Base Island | 116.0 tiles (928 px) | 1,855 px | ~23% |
| **Bone Island** | Kokola Island | 94.1 tiles (753 px) | 1,506 px | ~19% |
| **Lukeland Isles** | Skeg Island | 81.2 tiles (650 px) | 1,299 px | ~16% |
| **Orte Rocks** | Red Island | 67.7 tiles (541 px) | 1,083 px | ~14% |

---

## Running the Suite

```bash
make test
```

Outputs individual PNG captures and a labeled contact sheet at `build/tests/contact_sheet.png`, exiting non-zero if any on-target C64 check or visual pixel assertion fails.
