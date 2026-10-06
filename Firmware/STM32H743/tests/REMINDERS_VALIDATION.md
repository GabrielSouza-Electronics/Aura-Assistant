## Files inspected
TaskStore, TasksWidget/View, web_state and bounded JSON parser, calendar worker,
existing task wrappers/init, CMake source list and simulator layout/build.

## Files modified
Added App/Inc/app_reminders.h, App/Src/app_reminders.c and tests/host/test_reminders.c.
Updated CMakeLists.txt, freertos.c USER CODE sections, TasksWidget.cpp/hpp,
Screen1View.cpp, web_state.h, calendar_data.c, app_calendar.c, native tests,
Lovable API instructions, preview generator and this report.

## LCD hardware configuration confirmed
No LCD, pins, clocks, cache or framebuffer changes.

## Datasheet / initialization sources used
Existing application/ToF/audio APIs; no new hardware initialization.

## Any unresolved assumptions
Physical rendering and gestures require device validation. Reminders implements
the same list workflow as Tasks; scheduled notifications/alarms were not requested
and are not introduced. Missing reminders in old API responses preserves the list.

## Architecture implemented
Carousel index 2 now opens Reminders using the same TasksWidget in reminder mode,
including the existing REMINDERS header sprite and its transition. Navigation,
four items/page, sort, deadline formatting, categories, centered tags, entry cascade,
cyan sphere/highlight pulse, raised footer, Wi-Fi/battery and gestures are shared.
Short ToF release completes/reopens with distinct sounds; long proximity returns.
Separate CPU-owned TaskStore in SRAM4 ensures task and reminder IDs/states do not
interfere. Eight offline reminder examples initialize before the scheduler.
The simulator also uses separate stores. Both widgets use the approved Tasks
design directly; no divergent visual pipeline or extra assets are created.

The web state supports an optional reminders array parsed by the same bounded
record validator. First real list replaces demo data; [] clears it. PUT uses the
dedicated reminders/{id}/completion path and the existing desired-state/ack logic.
Lovable contract documents both lists and endpoint behavior.

## Build command
cmake --build --preset Debug

## Build result
Debug firmware and simulator build_executable passed. Native suite passed demo
and online state isolation, complete/reopen, two reminder pages, reminder JSON,
all JSON truncations, dedicated PUT reopen route and GET reminder publication.
Existing Tasks/calendar/web regressions passed. Empty-QSPI objcopy warning remains.

## Memory impact if meaningful
DTCM 126,088 B (96.20%); SRAM4 36,536 B (55.75%); Flash 805,152 B;
QSPI 5,348,544 B unchanged. No additional framebuffer or DMA buffers.

## Hardware tests to perform when PCB arrives
Open Reminders from carousel, traverse both example pages, check transition,
highlight/tags/status, complete/reopen, then open Tasks and confirm independence.
Check long-hold return and empty-list rendering. Configure API and verify real
reminder GET/PUT behavior. Validate separately on physical hardware.

## Recommended next step
Program the Debug firmware and use the existing current UI assets. Send the new
Reminders section of aura_assets/LOVABLE_DEVICE_API.md to Lovable.
