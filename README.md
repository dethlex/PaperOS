# PaperOS

**A tiny multi-app operating system for the [M5Stack M5Paper](https://docs.m5stack.com/en/core/m5paper) e-ink device.**

PaperOS turns the M5Paper (ESP32 + 4.7" 540×960 grayscale e-ink) into a always‑on,
battery‑friendly desk companion: an e‑reader, a Home Assistant remote, a weather
station, a calendar agenda, a 3D‑printer console, a small games console, and a network
file drive — all reachable from a single touch launcher and driven by the three hardware
buttons.

It is written in modern C++17 on Arduino‑ESP32, builds with PlatformIO, and is designed
around a clean four‑layer architecture so the bulk of its logic runs as host unit tests
without any hardware.

- **Bilingual UI** — English / Russian, switchable at runtime.
- **Months of battery life** — a cooperative light‑sleep clock loop, e‑ink powered down between ticks.
- **No companion app, no cloud** — everything runs on‑device; configuration lives on the microSD card.

---

## Hardware

PaperOS targets the **M5Stack M5Paper** exactly as shipped:

| Component | Detail |
|-----------|--------|
| MCU | ESP32‑D0WDQ6‑V3, dual‑core 240 MHz, 16 MB flash, 8 MB PSRAM |
| Display | 4.7" 540×960 e‑ink, 16 grayscale levels, IT8951 controller |
| Touch | GT911 capacitive |
| Buttons | Three side buttons (G37 / G38 / G39) |
| Sensors | SHT30 temperature + humidity (on‑board) |
| Clock | BM8563 RTC (note: **no backup battery** — time is restored over NTP at boot) |
| Storage | microSD slot |
| Power | 1150 mAh LiPo + USB‑C |

No additional wiring or modules are required.

---

## Features

Everything is launched from the **Launcher**: a grid of vector app icons with a live
clock and battery indicator in the status corner. The list scrolls (G37/G39) when there
are more than six apps, and **Settings** is always pinned last.

### 📖 Reader
Reads plain‑text books from `/paperos/books/` on the SD card.
- Automatic **UTF‑8 / CP1251** encoding detection.
- Pagination computed from the *actual* render (e‑ink smooth‑font metrics don't match a
  pre‑pass), so pages never clip mid‑glyph.
- Bookmarks with a progress percentage in the footer; page turns on the hardware buttons.

### 🏠 Home Assistant
A configurable dashboard for your Home Assistant instance, defined entirely in
`/paperos/ha_dashboard.json` (no reflash needed to change it).
- Widget kinds: **toggle** (lights/switches), **sensor** (read‑only value + unit),
  **action** (scripts/scenes), **lock** (lock/unlock).
- **Incremental polling** — one entity per tick (round‑robin) so the UI never stalls on a
  network sweep; instant first paint from the NVS cache.
- HTTPS with keep‑alive connection reuse; optimistic toggle with a confirming re‑query.
- Grouped sections with headers; button scrolling for long dashboards.

### 🌤 Weather
Local + indoor weather without any account.
- **Outdoor** from [Open‑Meteo](https://open-meteo.com/) (free, no API key): current
  conditions + humidity, a 12‑hour hourly strip, and a 5‑day forecast.
- **Indoor** read live from the on‑board SHT30 sensor (with a configurable calibration
  offset for self‑heating).
- Refreshes on open and shows a "last updated HH:MM" stamp.
- A combined outdoor/indoor/battery panel also appears on the lock screen.

### 📅 Calendar
A read‑only agenda — today plus the next 7 days — for one Google calendar, sourced through
Home Assistant's REST API (`/api/calendars/<entity>`), so HA does the recurrence expansion
and timezone handling.
- The calendar entity is chosen from a picker in **Settings** (populated from
  `GET /api/calendars`).
- Events are cached in NVS and refreshed in the same WiFi window as the weather (at boot and
  on the screensaver's refresh boundary) — no extra radio time.
- The next upcoming event is shown as a line under the lock‑screen clock.

### 🖨 Printer (Klipper / Moonraker)
Monitor and drive a 3D printer running Klipper via its Moonraker API.
- **While printing** — live status: state, file‑relative progress, nozzle/bed temperatures,
  time + ETA, current layer, file name, and the sliced model thumbnail.
- **While idle** — a scrollable control deck: printer power (Moonraker `device_power`,
  auto‑discovered), Klipper LED light, XYZ jog (10 mm) + Home with axis gating, preheat /
  cooldown presets, and reprint from the print history.
- Every intervening action (cancel, pause, power‑off, start‑from‑history) is confirmed with an
  on‑screen dialog. Configuration (printer URL, optional API key, presets) lives in a
  `printer` block in `config.json`.

### 🎮 Games
Four self‑contained games, no saves, no animations — just paper‑friendly puzzles.
- **Tic‑Tac‑Toe** — vs. a minimax AI or two‑player.
- **Minesweeper** — 8×8 / 10 mines, flag & reveal modes, safe first tap.
- **15‑Puzzle** — guaranteed‑solvable shuffle, move counter.
- **Sudoku** — hand‑picked puzzles with live conflict highlighting.

Each game's rules live in a pure, host‑unit‑tested core; the device wrapper only draws.

### ⚙️ Settings
On‑device configuration via an **on‑screen keyboard** (English / Russian / symbols, with
shift and icon keys for backspace/shift):
- **WiFi**, **Home Assistant** (URL + token), **Reading** (font size, margins),
  **Time** (UTC offset, NTP sync), **Screensaver** (idle timeout, photo rotation),
  **Weather** (coordinates, refresh interval, indoor offset), **Calendar** (entity picker),
  **Language**, **About**.
- Two‑way **`config.json` sync**: edits in the UI are written back to the card, and the
  card's values are imported on boot or on demand.
- *About* shows the firmware version (derived from the git tag at build time).

### 🖼 Screensaver / Lock screen
The low‑power resting state.
- Full‑screen wallpaper (JPEG or PNG from `/paperos/wallpapers/`), a large clock + date,
  and the combined weather/indoor/battery panel.
- Runs a **light‑sleep clock loop**: wakes ~once a minute to tick the clock, or on the
  G38 button to return to the UI. Weather refreshes only on its configured boundary.

### 🗂 File Server (WebDAV)
Manage the SD card over WiFi without removing it.
- Serves `/paperos/` over WebDAV while the app is open (WiFi and the server stop on exit).
- Read / list / download works from any WebDAV client, including macOS Finder.
- Upload/write works from clients that send `Content-Length` (Cyberduck, Transmit,
  rclone, `curl -T`, Windows "Map network drive"). *(macOS Finder is read‑only — it uploads
  with chunked encoding, which the embedded HTTP server doesn't accept.)*

### 🌍 Internationalization
The entire UI is bilingual (English / Russian). Strings are compile‑time tables with a
`static_assert` completeness check (a missing translation is a build error). The language
switches live from **Settings → Language** (no reboot) and **defaults to English**.

---

## Power & sleep

PaperOS deliberately uses **light sleep**, not deep sleep:

- The M5Paper holds its 3V3 rail with a **GPIO2 latch**. `esp_deep_sleep_start()` releases
  GPIO2, so on battery the board powers off and hangs (invisible on USB, where the rail is
  fed externally). Light sleep keeps the digital domain powered, so the latch holds and the
  device survives sleep on battery.
- There is **no power button** — only reset, which doesn't power on from battery — so a full
  `M5.shutdown()` would strand the unit. The screensaver's light‑sleep loop is the low‑power
  state instead.
- During sleep the e‑ink panel is powered down (its rail is separate from the main latch),
  and the displayed frame persists on the panel for free. Battery life is measured in months.
- The per‑minute wake is tuned to stay short: the loop runs at **80 MHz** (240 MHz only for the
  WiFi refresh window), the decoded wallpaper is cached in PSRAM instead of re‑decoded from SD,
  glyph renders are reused across ticks, the weather panel is repainted only when its data
  changed, and photo rotation happens strictly on its configured interval rather than every wake.

---

## Architecture

PaperOS is structured as four layers with a strict one‑way dependency rule — each layer
only knows the public API of the layer directly below it:

```
Apps  →  Framework  →  Services  →  HAL
```

- **HAL** (`src/hal/`) — thin wrappers over hardware: `Display`, `Touch`, `Buttons`,
  `Rtc`, `Sd`, `Wifi`, `Sht30`, `Battery`, `Nvs`.
- **Services** (`src/services/`) — stateful logic: `ConfigStore`, `WeatherService`,
  `HAClient`, `Power`, `WebDavServer`, `BookIndex`, …
- **Framework** (`src/framework/`) — the app runtime: `AppSwitcher`, `InputRouter`, the
  `IApp` contract, and shared UI helpers (fonts, geometry, `IconKit`, on‑screen `Keyboard`).
- **Apps** (`src/apps/<id>/`) — one folder per app, each exporting an `IApp` factory.

A few principles fall out of this:

- **Single cooperative loop.** There are no FreeRTOS tasks. Long work is chunked across
  `tick()` calls (~10 Hz) so input is never starved and the watchdog never fires.
- **Dependency inversion for testing.** Because Apps never touch the HAL directly, the
  pure logic — game rules, the Open‑Meteo parser, pagination, config (de)serialization,
  launcher ordering, the keyboard layout — compiles and runs as **host unit tests** with no
  hardware (`pio test -e native`).
- **The app contract** (`framework/App.h`):

  ```cpp
  class IApp {
      virtual void enter(AppContext&);
      virtual void leave(AppContext&);
      virtual void tick(AppContext&);                 // ~10 Hz
      virtual void onInput(const InputEvent&, AppContext&);
      virtual bool onBack(AppContext&);               // hierarchical back (G38)
      virtual const char* id() const = 0;
      virtual const char* title() const = 0;
  };
  ```

  Apps are registered by id with a factory in `main.cpp`:

  ```cpp
  appSwitcher.registerApp("weather", [](){ return new WeatherApp(); });
  ```

- **Input model.** Touch plus three buttons: **G37 → NavUp**, **G39 → NavDown** (meaning is
  app‑specific — page, scroll), **G38 → hierarchical Back** (and lock screen at the top
  level; long‑press is intercepted by the router).
- **Assets are embedded** in the binary (fonts, default wallpaper) via the linker — no
  SPIFFS.

---

## Building from source

PaperOS builds with [PlatformIO](https://platformio.org/).

```bash
pio run -e m5paper      # build the firmware
pio test -e native      # run the host unit-test suite (no hardware)
pio device monitor      # serial monitor
```

> **Do not change the toolchain pin.** `platformio.ini` pins
> `platform = espressif32@^5.1.0` with `framework-arduinoespressif32@3.20004.0`. The M5EPD
> library silently breaks on the generic latest `espressif32` (Arduino‑ESP32 v3.x): the
> firmware runs but the display never updates.

The firmware version shown in *Settings → About* is injected at build time from
`git describe --tags` (see `tools/version_flag.py`). Build/flash after tagging a release to
get a clean version string.

## Flashing

The simplest path is PlatformIO's uploader:

```bash
pio run -e m5paper -t upload
```

If that fails with a `termios.error: (22, 'Invalid argument')` from `pyserial`
(a CPython 3.13+ regression that affects PlatformIO's bundled Python), flash with `esptool`
from a separate Python 3.11 virtualenv. Only the app partition needs rewriting
(`0x10000`); the bootloader/partition table are unchanged:

```bash
python3.11 -m venv /tmp/paperos-flash
/tmp/paperos-flash/bin/pip install pyserial esptool
/tmp/paperos-flash/bin/python -m esptool --chip esp32 \
    --port /dev/cu.usbserial-XXXXXXXX --baud 460800 \
    write-flash 0x10000 .pio/build/m5paper/firmware.bin
```

---

## SD card layout

Everything lives under `/paperos/` on a FAT‑formatted microSD card:

```
/paperos/
  books/              TXT books for the Reader
  wallpapers/         JPEG/PNG images for the screensaver
  ha_dashboard.json   Home Assistant dashboard layout (no secrets)
  config.json         Full configuration mirror (incl. secrets)
  cache/              Auto-generated; safe to delete
```

The card is optional for booting, but Reader, the wallpaper screensaver, the HA dashboard,
and config import all need it.

## Configuration

### `config.json`

A complete, human‑readable mirror of the device configuration. On boot (and via
*Settings → About → Sync config.json*) non‑empty fields are imported into NVS, then the
full config is written back — so the device keeps the file up to date and you can edit it
on a computer. Secrets live here (and only here, on the card).

```json
{
  "wifi":    { "ssid": "MyNetwork", "password": "secret" },
  "ha":      { "url": "https://homeassistant.local:8123", "token": "ey..." },
  "timezone_offset_hours": 0,
  "weather": { "lat": "52.52", "lon": "13.41", "refresh_min": 30, "indoor_offset": -2 },
  "reader":  { "font_size": 1, "margin_px": 24 },
  "screensaver": { "idle_s": 300, "rotate_min": 30 },
  "calendar": { "entity": "calendar.personal" },
  "printer": { "url": "http://192.168.1.50", "api_key": "", "power_device": "", "preheat_nozzle": 200, "preheat_bed": 60 },
  "language": "en"
}
```

Importing never overwrites an already‑set value with a missing/empty one, so you can drop a
partial file (e.g. only the `ha` block) without clearing the rest.

### `ha_dashboard.json`

Defines the Home Assistant dashboard. Read on every entry to the HA app (no reflash):

```json
{
  "groups": [
    { "title": "Kitchen", "items": [
      { "entity_id": "light.kitchen_main", "kind": "toggle", "label": "Light" },
      { "entity_id": "sensor.kitchen_temp", "kind": "sensor", "label": "Temp", "unit": "°C" },
      { "entity_id": "script.boil_kettle",  "kind": "action", "label": "Kettle" }
    ]}
  ]
}
```

`kind` is one of `toggle`, `sensor`, `action`, `lock`. Unknown kinds and malformed entries
are skipped.

---

## Project structure

```
src/
  hal/          Hardware abstraction (Display, Touch, Buttons, Rtc, Sd, Wifi, Sht30, Battery, Nvs)
  services/     ConfigStore, WeatherService, HAClient, Power, WebDavServer, BookIndex, ...
  framework/    AppSwitcher, InputRouter, App contract, UI helpers (Fonts, IconKit, Keyboard)
  apps/         launcher, reader, ha, weather, calendar, printer, games, settings, screensaver, fileserver
  i18n/         Compile-time string tables + tr(); RU/EN
  util/         Pure helpers (encoding detection, WMO codes, battery curve, WebDAV paths, ...)
  main.cpp      Boot, app registration, the cooperative loop
test/native/    Host unit tests (Unity) for the pure logic
tools/          Build/version helpers and SD-prep utilities
data/fonts/     Embedded TTF fonts (DejaVu Serif + Sans Mono)
```

## Tech stack

- **PlatformIO** + **Arduino‑ESP32** (pinned 5.1.x / core 3.20004.0), **C++17**.
- **[M5EPD](https://github.com/m5stack/M5EPD)** — display, touch, buttons, RTC, SD.
- **[ArduinoJson](https://arduinojson.org/) v7**, **[PNGdec](https://github.com/bitbank2/PNGdec)**,
  **[TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder)**.
- **DejaVu Serif / Sans Mono** TTFs (cover Latin + Cyrillic, embedded).
- **Unity** for host unit tests.

## Engineering notes

A few non‑obvious constraints shaped the design:

- **Toolchain is pinned** — M5EPD requires Arduino‑ESP32 v2.x (see Building).
- **RTC has no backup battery**, so the clock is wrong after any power loss. On boot, if
  WiFi is configured, PaperOS syncs time over NTP and refreshes the weather cache before the
  first render.
- **E‑ink fast‑refresh (A2) is 1‑bit black/white** — gray tones drop to white in `Quick`
  pushes, so any borders in a fast‑updated region are drawn in pure ink.
- **TrueType + Cyrillic** needs whole strings drawn at once (smooth‑font metrics misbehave on
  single multibyte code points), and pagination is indexed by actual render output.

---

## License

[MIT](LICENSE) © PaperOS authors.
