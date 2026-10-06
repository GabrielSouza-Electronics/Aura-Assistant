## Files inspected
TasksWidget.cpp, Screen1View.cpp, SettingsLayout.hpp and Settings preview generator.

## Files modified
TasksWidget.cpp, Screen1View.cpp, gen_settings_tasks_style.py, Settings preview PNG
and this report.

## LCD hardware configuration confirmed
Unchanged; existing Portrait transform preserved.

## Datasheet / initialization sources used
None required for widget positioning.

## Any unresolved assumptions
Physical device rendering remains unverified. The reported page counter and
arrows belong to Tasks in the current firmware; Settings has the gesture hint.

## Architecture implemented
Tasks pagination/arrows/glow moved up 20 logical pixels; gesture hint moved up
24 pixels, retaining its current ToF instruction. Settings and generic menu hints
now center at logical y=400 instead of 418, using a separate constant so carousel
status text positions stay unchanged. Settings preview matches the current hint.

## Build command
cmake --build --preset Debug

## Build result
Passed; existing empty-QSPI objcopy warning remains.

## Memory impact if meaningful
No additional buffers or asset data.

## Hardware tests to perform when PCB arrives
Check both task pages, arrow glow and Settings hint for clearance from the rim.

## Recommended next step
Program updated firmware. This position-only change requires no new QSPI assets.
