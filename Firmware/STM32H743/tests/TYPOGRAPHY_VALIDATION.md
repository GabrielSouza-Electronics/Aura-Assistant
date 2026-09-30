# Typography implementation validation

## Files inspected / modified

Inspected existing text, settings, icon and status generators, GlyphText,
SettingsLayout/SettingsGlyphs, Screen1View, CalendarWidget's rotated pixel
rendering, CMake and simulator configuration.

Modified generators in `aura_assets/gen/`, added `install_typography.py`,
installed 105 PNG assets under text/settings/glyphs/icons/status and their
source/rotated copies. Regenerated image sources with TouchGFX ImageConvert
4.26.1. Updated SettingsLayout/SettingsGlyphs and Screen1View sizing.
Added `aura_assets/compiled/typography/TypographyHints.*` and explicit build
entries in root CMake and simulator Makefile.

## LCD configuration / datasheet sources

UI-only change. Existing 480x480 RGB565 single framebuffer and Portrait sprite
rotation preserved. No panel initialization, pins, clocks, MPU or backlight
changes; no new hardware or datasheet assumptions required. Existing calendar
pixel rendering is the reference for the new hint assets.

## Architecture

Poppins Medium rendered at 6x and reduced once to native resolution. Settings
labels/values 18 px, menu titles 24 px, subtitles 12 px, hints/messages 14 px.
Settings icons 30 px. Emerald focus retained. Existing circuit/hero/rim retained
as approved. Static hints reside in QSPI; dynamic values retain GlyphText with
16-bit advances to accommodate glyphs exceeding 255 sixteenth-pixels.
Value columns respect larger labels; long SSIDs retain existing truncation.
GUI uses bitmap dimensions for centering rather than old size tables.

## Validation

- 105 asset palettes <=256 RGBA colors, clear boundaries, no dithering.
- Native preview inspected with maximum values and long SSID; lightened
  background preview available under `aura_assets/preview/typography/`.
- Official image conversion completed successfully; application.config unchanged.
- `python -B tests/test_settings.py --cc C:/ST/STEdgeAI/4.0/Utilities/windows/mingw64/bin/gcc.exe --cxx C:/ST/STEdgeAI/4.0/Utilities/windows/mingw64/bin/g++.exe`: passed navigation, value bounds, BLE requests, read-only SSID and PWM checks.
- Existing Debug build: `cmake --build --preset Debug`, using installed
  STM32Cube CMake 4.3.1 and GNU Arm 14.3.1 toolchain: passed.
- Build log: `build/Debug/typography-build.log`.

## Memory impact

Compared with pre-typography calendar build: DTCM 125960 -> 125896 bytes
(96.05%); framebuffer unchanged at 460800 bytes; internal flash 726732 ->
727132 bytes; QSPI 4008504 -> 4043668 bytes (24.10% of 16 MiB).
D2/D3 allocations unchanged. No additional framebuffer or runtime heap buffer.

## Remaining issues / physical checks / next step

Existing objcopy warning about the empty 0x90000000 load segment remains when
producing the internal-only ELF. Whole-tree diff check also finds pre-existing
trailing whitespace in Core/Src/main.c; this UI task does not modify it.
Compilation is validated; physical typography, hint rotation, animation
performance and readability require checking the LCD. Install matching internal
firmware and external QSPI asset images together, then inspect standby, each
menu title, Settings list/edit hints and maximum/long values on the device.
No flashing performed as part of this change.
