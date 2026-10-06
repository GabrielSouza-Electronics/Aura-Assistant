## Files inspected
TasksWidget.cpp and its glyph layout conventions.

## Files modified
TouchGFX/gui/src/common/TasksWidget.cpp and this report.

## LCD hardware configuration confirmed
Unchanged; existing Portrait coordinate transform retained.

## Datasheet / initialization sources used
None required for text positioning.

## Any unresolved assumptions
Device rendering has not been physically verified.

## Architecture implemented
Measure the formatted page/deadline subtitle using the same glyph advances,
space width and pixel rounding as the renderer. Center its resulting sprite span
at logical x=240, matching the Tasks title, for every page and task count.

## Build command
cmake --build --preset Debug

## Build result
Passed; existing empty-QSPI loadable-segment objcopy warning remains.

## Memory impact if meaningful
No new buffers or assets.

## Hardware tests to perform when PCB arrives
Check subtitle centering on both example pages and with different total counts.

## Recommended next step
Program the updated firmware; this change requires no new QSPI assets.
