# Calendar implementation — 2026-09-30

## Files inspected

- `AuraAssistant.ioc`, `Core/Src/ltdc.c`, `Core/Src/freertos.c`,
  `Core/Inc/FreeRTOSConfig.h`, `STM32H743xx_FLASH.ld`, `CMakeLists.txt`.
- Current Screen1View, MenuLogic, SettingsLogic, GlyphText, Model and generated
  Screen1ViewBase; TouchGFX application/configuration and simulator makefiles.
- W6X API/types, socket/TLS/HTTP implementations and ST67 target configuration.
- TouchGFX PixelDataWidget implementation and LCD/Bitmap APIs; local Poppins
  font and source asset generators.

## Files modified

New implementation:

- `App/Inc/app_calendar.h`, `App/Src/app_calendar.c` — worker, daily sync,
  cache and snapshots shared with GUI.
- `App/Inc/calendar_certificates.h`, `aura_assets/calendar_certs/` — public
  trust anchors from OS-verified TLS chains (Sectigo R46 and GlobalSign Root CA).
- `Components/Calendar/calendar_data.{c,h}` — portable Gregorian date math,
  strict bounded JSON validation and HTTP framing/decoding.
- `TouchGFX/gui/include/gui/common/Calendar{Logic,Widget}.hpp` and
  `TouchGFX/gui/src/common/CalendarWidget.cpp` — ToF gestures and rendering.
- `aura_assets/gen/gen_calendar.py`, source/rotated calendar PNG directories,
  `aura_assets/compiled/calendar/CalendarAssets.{cpp,hpp}` and review previews.
- `tests/test_calendar.py`, `tests/host/test_calendar_*`,
  `tests/calendar_fakes/`, `tests/calendar_fixtures/` and this report.

Integration changes: `App/Src/app_wifi.c`, Screen1View `.hpp/.cpp`, root
`CMakeLists.txt`, linker `.calendar` section and custom simulator Makefile.
Earlier uncommitted ToF/Settings/BLE/emerald changes were preserved.
No CubeMX/TouchGFX generated sources or application.config were edited for
the calendar. No commit, push or board programming was performed.

## LCD hardware configuration confirmed

Current sources use 480x480 RGB565, Portrait TouchGFX orientation, one
460,800-byte framebuffer in AXI SRAM D1. Existing LTDC accumulated timings
and signal polarities are unchanged. The existing rimGlow remains above the
calendar; there is no additional perimeter ring.

## Datasheet / initialization sources used

This change adds no LCD register writes, pin/clock changes or panel init table.
Hardware initialization remains owned by existing CubeMX/BSP code. Rendering
uses the local TouchGFX 4.26.1 API and PixelDataWidget implementation.

Public data sources:

- Time: `https://timeapi.io/api/Time/current/zone?timeZone=Asia%2FDubai`.
  Tested HTTP 200 with chunked transfer; fields include timezone and local
  year/month/day/hour/minute/seconds. An actual response is kept as a test fixture.
- Holidays: `https://worldtimeandweather.com/v1/holidays?country=AE&year=2026&type=public`.
  [Provider documentation](https://worldtimeandweather.com/developers/api/holidays).
  Tested HTTP 200, 3,005 bytes with Content-Length; an actual response is kept
  as a fixture. Year is selected dynamically in firmware. This service has
  returned transient connection resets during live checks; retry is implemented.

## Any unresolved assumptions

- The holiday feed is third-party UAE national public holidays, not an official
  Abu Dhabi government feed. Lunar holidays marked `estimated` remain estimates.
  Daily GET does not guarantee incorporation of every government announcement.
  The UI identifies estimates, unavailable data, offline cache and stale data.
- Navigation supports 2020–2099. Holiday availability depends on the provider's
  explicit `years` list; an unsupported year is never treated as zero holidays.
- Data/cache are volatile. The user-supplied boot fallback is 2026-09-30 23:24:00
  in Abu Dhabi, advancing with RTOS uptime even before the network worker starts.
  The first successful HTTP sync replaces it. Every reboot starts from this seed;
  no backup RTC/persistent cache is added. Holiday data are not fabricated.
- TLS roots may need updating if providers change their certificate chains.
  CA credentials and SNI are configured with no plaintext fallback. Actual NCP
  TLS interoperability/hostname validation and boot-time SNTP require bench checks.
- NCP UTC SNTP is enabled for TLS validity checks. The display's date/time
  source remains HTTP GET in Asia/Dubai (UTC+4).

## Architecture implemented

- WiFiTask starts one static, lower-priority Calendar worker after successful
  Net initialization and publishes IP availability. All DNS/TLS/GET work stays
  out of the GUI and the provisioning poll loop.
- On first connection and each local date change: refresh time and current-year
  holidays. Fetch a browsed year as needed; keep up to three validated years.
  Retry failures from 30 seconds up to one hour, retaining last good data.
- Short critical sections protect snapshots/requested year. The GUI extrapolates
  the synchronized clock using RTOS ticks even while the worker is waiting on
  network I/O. Tick wraparound is covered by tests.
- HTTP responses: 16 KiB wire buffer, 4 KiB header limit, 8 KiB decoded JSON
  limit, 20-second receive-loop deadline, 5-second receive timeout. Validate
  status 200, Content-Length/chunked framing and complete JSON before publishing.
  Reject redirects, compressed responses, duplicate framing headers and bad dates.
  W6X returns zero on timeout: firmware requires explicit length/chunk completion.
- The middleware HTTP helper reports results before the body and has no robust
  chunked lifecycle. The application uses its supported socket API instead;
  the ST library remains unchanged. TLS_SEC_TAG_LIST uses the element count
  expected by the inspected local implementation.
- Entry always returns to the current month; neutral/released hand arms gestures.
  Right/left changes one month per excursion, reversal rearms, down exits.
  Holding a stale ToF sample does not repeatedly scroll. Missing time never
  fabricates a calendar date and does not block the back gesture.
- Clock and labels are antialiased sprites generated at 6x then downsampled.
  Current day pulses blue; public holidays yellow; Saturday/Sunday red.
  On a holiday/today overlap, the yellow fill remains with a pulsing blue outline.
  The six-row layout reserves separate space for data status and gesture hints.
- Runtime ARGB8888 arrays are directly in QSPI. Original -> left 90-degree
  source rotation -> left 90-degree memory layout matches ST ImageConvert 4.26.1;
  verified with an asymmetric 2x3 probe. No dynamic bitmap cache or second
  framebuffer is required. The calendar now has a transparent background:
  the existing board and moving circuit particles stay visible at alpha 102/255
  (40%). Only the home hero/divider animations pause. Board brightness returns
  to its normal value on exit; LCD backlight settings are unaffected.
- Clock snapshot reads follow actual RTOS time each frame. The colon alternates
  visible/hidden at each second boundary, without shifting the HH:MM digits.
  Its dirty rectangle is 88x22 in rotated coordinates. Circuit animation also
  invalidates the background each frame, so physical frame timing must be checked.

## Build command

```powershell
$env:PATH = 'C:\Users\gabri\AppData\Local\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin;C:\Users\gabri\AppData\Local\stm32cube\bundles\ninja\1.13.2+st.1\bin;' + $env:PATH
& 'C:/Users/gabri/AppData/Local/stm32cube/bundles/cmake/4.3.1+st.1/bin/cmake.exe' --build --preset Debug
```

Regenerate custom calendar assets with `python -B aura_assets/gen/gen_calendar.py`.
This does not alter Designer configuration or import PNGs automatically.

## Build result

Debug compile/link successful. Existing objcopy warning about an empty loadable
QSPI segment in the separated internal image remains; external pixel assets
are present in the external programming image. No new compiler warnings.

Host validation (`C:/ST/STEdgeAI/4.0/Utilities/windows/mingw64/bin/` GCC/G++):

- Calendar core: weekdays for all 29,220 supported dates, 2,000 date advances,
  leap years, live JSON fixtures, invalid/duplicate fields, every truncated
  fixture prefix, every HTTP fragment boundary, chunk extensions/trailers and
  invalid/oversized/conflicting headers.
- Navigation: first-entry arming, no repeated held gesture, direction reversal,
  December/January, range bounds, follow-today vs browsing, reset and back offline.
- Production service against fake RTOS/sockets: partial sends and receives,
  daily time/holiday GET, browsed-year cache, offline clock, tick wrap, failed
  JSON preservation, retry backoff, TLS credential cleanup, connection failures
  and receive timeouts. This is not a radio/TLS handshake test.
- Settings, ToF and provisioning regression suites all passed.
- CalendarWidget simulator branch compiled against real TouchGFX headers with
  host G++; full graphical simulator was not run.
- Source assets validated for <=256 RGBA colors and transparent sprite edges;
  six-row and lightened-background previews generated and inspected.

## Memory impact if meaningful

Latest linked usage is recorded in `build/Debug/calendar-build.log`:

| Region | Used | Capacity |
| --- | ---: | ---: |
| DTCM | 125,960 B | 128 KiB |
| AXI D1 framebuffer | 460,800 B | 512 KiB |
| D2 | 96,960 B | 288 KiB |
| D3 | 26,072 B | 64 KiB |
| Internal flash | 726,732 B | 2 MiB |
| QSPI | 4,008,504 B | 16 MiB |

Versus the pre-calendar build: +176 B DTCM, +23,364 B D3, +30,088 B
internal code/data and +1,931,164 B QSPI. DTCM remains tight (~96.1%);
the HTTP buffer, three-year cache and 6 KiB static worker stack are in D3.

## Hardware tests to perform when PCB arrives

1. Program both the internal image and matching external QSPI asset image.
2. Check text orientation, crispness and clipping at the circular edge, six-row
   months, current-day pulse, holiday/today/weekend precedence and retained rim.
3. Test neutral/left/right/down gestures with the actual ToF mount and hand
   placement; verify no immediate scroll/back on calendar entry.
4. Provision Wi-Fi via BLE, validate DNS/TLS with the included roots/SNI, and
   inspect ST67 errors, free heap and Calendar stack high-water mark.
5. Check time/holiday GET at connection, local midnight, year change and after
   disconnect/reconnect; invalid responses must preserve previous values.
6. Confirm BLE provisioning remains responsive during network timeouts and
   check GUI frame timing/tearing on the physical single-buffer display.

## Recommended next step

Bench validation with the matching internal/QSPI images. No physical LCD,
ToF, TLS session or Wi-Fi midnight rollover is claimed validated by this work.

## Shared status header (2026-10-01)

Inspected Screen1View, CalendarWidget and generated base widget positions.
Updated Screen1View.cpp to retain logoStatus, wifiIcon, battFill and battFrame
when entering Calendar. Existing status callbacks remain active. Removed the
ABU DHABI draw call from CalendarWidget.cpp to prevent logo overlap.
Preview: aura_assets/preview/calendar_status_header.png.

LCD configuration, panel initialization and hardware interfaces unchanged; no
new datasheet assumptions. Existing shared widgets are reused without allocation.
Validation: cmake --build --preset Debug passed (calendar-header-build.log).
Internal flash 727084 bytes; DTCM 125896 bytes; QSPI 4043668 bytes unchanged.
Existing objcopy empty external load-segment warning remains. No hardware test
or flashing performed. Next device check: header visibility on entry/exit and
live battery/Wi-Fi updates while browsing months.
