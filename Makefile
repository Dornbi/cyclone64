# Cyclone (C64) - Oscar64 Build System

OSCAR64 ?= /Users/peter/bin/oscar64
OSCAR64_INC ?= /Users/peter/Library/CloudStorage/GoogleDrive-dornbach@gmail.com/My Drive/Programozas/c64/oscar64-main-20260818/include
VICE_DIR ?= /Applications/vice-arm64-gtk3-3.10/bin
X64SC ?= $(VICE_DIR)/x64sc
X64SC_NOFOCUS ?= $(if $(wildcard $(HOME)/.local/vice-nofocus/bin/x64sc),$(HOME)/.local/vice-nofocus/bin/x64sc,$(X64SC))
C1541 ?= $(VICE_DIR)/c1541
PYTHON ?= /usr/bin/python3

BUILD_DIR = build
PRG = $(BUILD_DIR)/cyclone.prg
D64 = $(BUILD_DIR)/cyclone.d64

OSCAR_FLAGS = -ii="$(OSCAR64_INC)" -Os -Oo -tm=c64 -n -xz

SRCS = src/main.c src/world.c src/raster.c src/scroller.c src/helicopter.c src/entities.c src/cyclone.c src/radar_map.c src/hud.c src/audio.c src/title.c
HDRS = src/memmap.h src/world.h src/raster.h src/scroller.h src/helicopter.h src/entities.h src/cyclone.h src/radar_map.h src/hud.h src/audio.h src/title.h generated/islands_data.h

.PHONY: all extract assets clean run run-ntsc disk screenshot test release

all: $(PRG)

extract:
	$(PYTHON) tools/extract_spectrum.py

assets:
	$(PYTHON) tools/build_assets.py
	$(PYTHON) tools/build_sprites.py

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(PRG): $(SRCS) $(HDRS) | $(BUILD_DIR)
	$(OSCAR64) $(OSCAR_FLAGS) -o=$(PRG) $(SRCS)

disk: $(PRG)
	rm -f $(D64)
	$(C1541) -format "cyclone,c6" d64 $(D64) -write $(PRG) "cyclone"

run: $(PRG)
	$(X64SC) -pal -autostartprgmode 1 -autostart $(PRG)

run-ntsc: $(PRG)
	$(X64SC) -ntsc -autostartprgmode 1 -autostart $(PRG)

screenshot: $(PRG)
	$(X64SC_NOFOCUS) -pal -sidmodel 1 -warp -sounddev dummy -autostartprgmode 1 -limitcycles 4500000 -exitscreenshot $(BUILD_DIR)/screenshot_title_pal.png -autostart $(PRG) || true
	$(X64SC_NOFOCUS) -ntsc -sidmodel 0 -warp -sounddev dummy -autostartprgmode 1 -limitcycles 4500000 -exitscreenshot $(BUILD_DIR)/screenshot_title_ntsc.png -autostart $(PRG) || true
	$(OSCAR64) $(OSCAR_FLAGS) -DSKIP_TITLE -o=$(BUILD_DIR)/cyclone_game.prg $(SRCS)
	$(X64SC_NOFOCUS) -pal -warp -sounddev dummy -autostartprgmode 1 -limitcycles 5000000 -exitscreenshot $(BUILD_DIR)/screenshot_pal.png -autostart $(BUILD_DIR)/cyclone_game.prg || true
	$(X64SC_NOFOCUS) -ntsc -warp -sounddev dummy -autostartprgmode 1 -limitcycles 5000000 -exitscreenshot $(BUILD_DIR)/screenshot_ntsc.png -autostart $(BUILD_DIR)/cyclone_game.prg || true
	rm -f $(BUILD_DIR)/cyclone_game.*

test: $(PRG)
	X64SC="$(X64SC_NOFOCUS)" $(PYTHON) tools/run_tests.py

release: $(PRG)
	mkdir -p bin
	cp $(PRG) bin/cyclone.prg

clean:
	rm -rf $(BUILD_DIR)
