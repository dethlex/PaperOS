# PaperOS dev commands. Wraps PlatformIO + a few helpers we needed during
# bring-up (pio device monitor requires an interactive TTY which isn't
# available in some shells/tooling).

ENV       := m5paper
NATIVE    := native
PIO       := pio
PORT      := $(firstword $(wildcard /dev/cu.usbserial-*))
BAUD      := 115200
PYTHON    := /opt/homebrew/Cellar/platformio/6.1.19_1/libexec/bin/python3
ESPTOOL   := $(HOME)/.platformio/packages/tool-esptoolpy/esptool.py
CAPTURE   := tools/serial_capture.py
PREP      := tools/prepare_wallpapers.py
# First /Volumes/ entry that isn't a macOS system mount. Resolved via shell so
# volumes with spaces in their names (`Macintosh HD`) don't break make's
# word-splitting on $(wildcard). Override explicitly with SD=... when in doubt.
SD        := $(shell ls -d /Volumes/*/ 2>/dev/null | grep -vE 'Macintosh|com\.apple' | head -1 | sed 's:/$$::')

.PHONY: help build upload flash monitor serial reset reset-cycle chip-id test test-native clean info wallpaper wallpapers wallpapers-clean

help:
	@echo "PaperOS dev targets:"
	@echo "  make build         — compile firmware (no flash)"
	@echo "  make upload        — build + flash firmware to M5Paper"
	@echo "  make flash         — alias for upload"
	@echo "  make monitor       — interactive serial monitor (needs TTY)"
	@echo "  make serial        — capture 10s of serial output (no reset)"
	@echo "  make reset         — pulse RTS to reset the device"
	@echo "  make reset-cycle   — reset and capture serial output"
	@echo "  make chip-id       — read ESP32 chip + flash info"
	@echo "  make test          — run native unit tests (host)"
	@echo "  make clean         — wipe PIO build cache"
	@echo "  make info          — show detected port / paths"
	@echo ""
	@echo "Wallpaper helpers:"
	@echo "  make wallpaper FILE=...  — convert ONE image to 540x960 JPG right next to it"
	@echo "                              (<name>-540x960.jpg; original untouched, no SD needed)"
	@echo "  make wallpapers SRC=...  — resize/convert images to baseline JPG 540x960"
	@echo "                              and copy to SD (default \$$SD=/Volumes/<first>)"
	@echo "  make wallpapers-clean    — strip macOS ._* metadata sidecars from SD"
	@echo ""
	@echo "Detected port: $(PORT)"

info:
	@echo "PORT    = $(PORT)"
	@echo "SD      = $(SD)"
	@echo "BAUD    = $(BAUD)"
	@echo "PYTHON  = $(PYTHON)"
	@echo "ESPTOOL = $(ESPTOOL)"

build:
	$(PIO) run -e $(ENV)

upload flash:
	$(PIO) run -e $(ENV) -t upload --upload-port $(PORT)

monitor:
	$(PIO) device monitor --port $(PORT) --baud $(BAUD) \
		--filter time --filter esp32_exception_decoder

serial:
	$(PYTHON) $(CAPTURE) --port $(PORT) --baud $(BAUD) --seconds 10

reset:
	$(PYTHON) $(CAPTURE) --port $(PORT) --baud $(BAUD) --reset --seconds 0

reset-cycle:
	$(PYTHON) $(CAPTURE) --port $(PORT) --baud $(BAUD) --reset --seconds 10

chip-id:
	$(PYTHON) $(ESPTOOL) --chip esp32 --port $(PORT) --baud 460800 flash_id

test test-native:
	$(PIO) test -e $(NATIVE)

clean:
	rm -rf .pio

# Resize images and drop them onto the SD card. Usage:
#   make wallpapers SRC=~/Pictures/wallpapers
#   make wallpapers SRC="a.jpg b.png c.heic"  SD=/Volumes/M5SD
# SD defaults to the first volume in /Volumes/; output goes to
# <SD>/paperos/wallpapers/.
# Convert a single image to a 540x960 baseline JPG right next to it (original kept):
#   make wallpaper FILE=~/Downloads/pic.heic   ->  ~/Downloads/pic-540x960.jpg
# No SD card needed — grab the result and drop it on the device (WebDAV / SD).
wallpaper:
	@test -n "$(FILE)" || { echo 'Usage: make wallpaper FILE=<path-to-image>'; exit 2; }
	$(PYTHON) $(PREP) "$(FILE)" --beside

wallpapers:
	@test -n "$(SRC)" || { echo 'Usage: make wallpapers SRC=<file-or-dir> [SD=/Volumes/...]'; exit 2; }
	@test -n "$(SD)"  || { echo 'No SD card mounted in /Volumes/. Pass SD=... explicitly.'; exit 2; }
	mkdir -p "$(SD)/paperos/wallpapers"
	$(PYTHON) $(PREP) $(SRC) --out "$(SD)/paperos/wallpapers"
	-find "$(SD)/paperos/wallpapers" -name '._*' -delete
	-find "$(SD)/paperos/wallpapers" -iname '*.png' -delete   # drop now-redundant PNG sources

# Remove macOS ._ sidecars + .DS_Store left over from previous copies.
wallpapers-clean:
	@test -n "$(SD)" || { echo 'No SD card mounted; pass SD=...'; exit 2; }
	-find "$(SD)" -name '._*' -delete
	-find "$(SD)" -name '.DS_Store' -delete
