## Files inspected
TasksWidget, TaskStore/store wrappers, web state/parser, HTTP buffer, current
header positioning, existing native Tasks/web tests and asset previews.

## Files modified
tasks_data.c/h, TasksWidget.cpp/hpp, app_calendar.c, calendar_data.c, native tests,
preview generator, Lovable API instructions and this report.

## LCD hardware configuration confirmed
Unchanged; existing Portrait transform used for procedural arc rendering.

## Datasheet / initialization sources used
No new hardware initialization.

## Any unresolved assumptions
Rendering smoothness and readability need device validation. With fewer than
40 items, the exact ratio is rounded to the nearest 2.5% arc step; the numeric
completed/total counter remains exact. Empty lists show 0/0 and an unfilled arc.

## Architecture implemented
Tasks and Reminders share a progress ring at logical (354,86), right of the title.
Its center moved 23 px left and 8 px down from (377,78), approximately 30% and
10% of the 78 px bounds including glow. Both generated previews were updated;
the Reminders preview was visually inspected. The shared widget applies the
same position and centered inner text to both menus.
The old page/deadline subtitle is removed. A dark ring track and bright cyan arc
surround completed/total and completed text. The arc uses 40 steps of 2.5% and
counts every page, including pending optimistic changes; reopening reduces it.
No new bitmap assets: ring and glow are procedurally drawn with clipped coverage.

List capacity is 40 each (10 pages). Pending/desired masks upgraded to uint64_t
to preserve items 33–40. CPU-owned stores remain in SRAM4. HTTP scratch capacity
increased to 32 KiB to accommodate both lists; Content-Length parsing is bounded
accordingly. Existing framebuffers and DMA buffers unchanged.

## Build command
cmake --build --preset Debug

## Build result
Debug firmware and simulator build passed. Native Tasks/calendar/web/reminder
suite passed, including all 40 toggles, global progress per page, high-bit pending
preservation, acknowledgment/reopen of item 40, rejection of item 41, and 0/100%
and partial progress steps. HTTP tests cover 26,000-byte Content-Length and
chunked responses, rejection above 32 KiB and oversized hexadecimal lengths.
Existing empty-QSPI objcopy warning remains.

## Memory impact if meaningful
SRAM4 55,512 B (84.70%) after larger stores and HTTP scratch; DTCM unchanged at
126,088 B. QSPI unchanged at 5,352,012 B. No additional framebuffer.

## Hardware tests to perform when PCB arrives
Inspect ring next to each title; complete/reopen across pages, verify counts and
arc updates, 40-item full cycle, empty lists and online snapshot replacement.

## Recommended next step
Program updated firmware; current QSPI assets are reused. Send updated 40-item
and 32 KiB response limits in LOVABLE_DEVICE_API.md to the web developer.
