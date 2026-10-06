# Offline Tasks validation

## Files inspected
`app_tasks.c/h`, `tasks_data.c/h`, `TasksWidget.cpp`, `app_calendar.c`,
`freertos.c`, existing native test runner and FreeRTOS test stubs.

## Files modified
`App/Src/app_tasks.c`, `App/Inc/app_tasks.h`, `tests/test_tasks.py`.
Added `tests/host/test_tasks_demo.c` and this report.

## LCD hardware configuration confirmed
No LCD or peripheral configuration changes.

## Datasheet / initialization sources used
Existing application initialization; no new hardware initialization.

## Any unresolved assumptions
Example deadlines use fixed dates, October 6–10, 2026. Without a synchronized
clock the existing renderer displays day/month and time, rather than guessing
Today/Yesterday/Tomorrow. Demo completion resets on reboot.

## Architecture implemented
APP_TasksInit seeds eight const example records, four per page, covering all
categories. Existing ordering, gestures, strikethrough and completion audio apply.
Demo completions are acknowledged locally and never sent to the website.
The first valid web snapshot replaces all examples, even with revision zero or an
empty list. Invalid snapshots leave the demo intact; normal online pending
completion and revision rules resume after replacement.

## Build command
`cmake --build --preset Debug`

## Build result
Passed; existing empty-QSPI-segment objcopy warning remains.

## Memory impact if meaningful
Flash 803,992 bytes (38.34%), up 1,984 bytes; reported RAM usage unchanged.

## Hardware tests to perform when PCB arrives
Boot offline, navigate both pages and all tags; complete via short ToF proximity release (<30 mm, <2 s), verify strike and
success audio, then reboot and verify examples reset. Check replacement by the
first real web response when the API is configured.

## Recommended next step
Flash the Debug firmware and validate the Tasks menu without Wi-Fi.
