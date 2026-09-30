# Calendar review — approved and implemented

User configuration: Abu Dhabi, UAE; public APIs may be chosen by the agent.
Clock timezone: Asia/Dubai. Monday-first grid, Saturday/Sunday red as requested.

Visual revision: removed the calendar-specific perimeter ring at the user's
request. Integration must preserve the current interface ring and avoid
covering it with the calendar background. The standalone preview shows only
calendar content, without reproducing the existing ring.

Latest user-requested revision: transparent calendar over the existing board
and animated CircuitField at 40% alpha. The preview uses the actual board PNG
at that brightness; particle movement is supplied by CircuitField at runtime.
The GIF shows the colon changing visibility once per second.

The user approved implementation after removing the extra perimeter ring.

`gen/gen_calendar.py` generates 179 source sprites with 6x font rendering,
native-resolution antialiasing, <=256 RGBA entries per image and transparent
edges. Pre-rotated copies are in `assets_rotacionados/calendar/`. Runtime
ARGB8888 pixel arrays live in `compiled/calendar/`, linked directly into QSPI
and drawn with TouchGFX PixelDataWidget. This avoids modifying Designer's
application.config rotation settings. No new framebuffer is allocated.

Review images: `calendar_mockup.png`, `calendar_mockup_leak.png`,
`calendar_mockup.gif` (today pulses), `calendar_six_rows.png` (August 2026).
October 14 and yellow October 6/26 are illustrative dates matching the supplied
reference; they must never be used as runtime holiday data.

## Network research (live GET tested 2026-09-30)

- `https://timeapi.io/api/Time/current/zone?timeZone=Asia%2FDubai`
  returned HTTP 200 with year/month/day/hour/minute/seconds/timeZone fields.
- `https://worldtimeandweather.com/v1/holidays?country=AE&year=2026&type=public`
  returned HTTP 200 and 12 public holiday dates, with `confidence` fields
  distinguishing generated/estimated dates. Candidate holiday source;
  [provider API docs](https://worldtimeandweather.com/developers/api/holidays).
- Tallyfy AE returned 403 for the default Python user agent, 200 with an
  application user agent, but includes Commemoration Day on November 30.
- holidays.abouts.co returned 200 but included obvious inconsistencies:
  Eid holiday entries in February alongside March entries. Do not use it.
- Caldays returned 200 but had differing dates and omissions; not selected.

Public providers are not authoritative government announcements. Even the
candidate marks lunar holidays as estimated and lacks March 19 from the
FAHR 2026 federal Eid circular. Daily fetching cannot guarantee that an
upstream dataset incorporates every newly announced date. Surface freshness
and estimated status rather than claiming official verification.

## Implemented firmware behavior

- Get date/time and holidays when Wi-Fi becomes available and once each local
  day thereafter; retry failures with backoff. Advance the clock locally
  between syncs. Do not make network requests from the GUI tick.
- On each Calendar entry reset the displayed month to the current local month.
  ToF left/right changes months; down returns to the carousel. Avoid repeated
  actions from the same held/stale sensor sample.
- Holidays yellow, weekends red, today animated blue. Define precedence for
  a holiday falling on a weekend/today so multiple meanings remain legible.
- Validate JSON/date ranges, bound all buffers, preserve the last good data
  on transient HTTP failures and distinguish no data from no holidays.
- The W6X socket API handles TLS with supplied public CA roots and SNI. A
  bounded application HTTP parser handles fragmented/chunked responses; the
  middleware HTTP helper was unsuitable for this lifecycle. No ST middleware
  sources were modified. Worker stack and scratch live in SRAM4.
- CMake explicitly includes the new application, pure calendar core, GUI and
  generated custom pixel source. Designer rotation settings are untouched.

See [CALENDAR_VALIDATION.md](../../tests/CALENDAR_VALIDATION.md) for implementation details,
validation, memory usage and remaining physical checks. The preview also
includes status text below the six-row grid; all dates remain illustrative.
