# Tag alignment

## Files inspected
TasksWidget.cpp, gen_settings_tasks_style.py, gen_tasks_firmware.py and generated
sprite dimensions; existing Debug build configuration.

## Files modified
`TouchGFX/gui/src/common/TasksWidget.cpp`,
`aura_assets/gen/gen_settings_tasks_style.py`, regenerated SettingsStyleAssets.cpp
and Settings preview PNGs; this report.

## LCD hardware configuration confirmed
Unchanged. Uses existing Portrait sprite dimension convention.

## Datasheet / initialization sources used
No hardware initialization changes or new datasheet assumptions.

## Any unresolved assumptions
Physical rendering still needs device verification.

## Architecture implemented
Settings tag y=(54-24)/2=15 logical pixels inside each row, retaining x=243.
Tasks derives tag y from half the difference between panel and tag heights,
retaining x=303. Logical height is stored in sprite.width due to Portrait rotation.
Tasks title width is limited to the available space before the tag to prevent
long names from drawing over the vertically centered badge.

## Build command
`cmake --build --preset Debug`

## Build result
Passed. Existing empty-QSPI objcopy warning remains. Settings preview regenerated
and inspected. No additional unit tests needed for this positioning change.

## Memory impact if meaningful
QSPI/RAM usage unchanged; internal Flash increased by 24 bytes.

## Hardware tests to perform when PCB arrives
Inspect all tags in Tasks and Settings, including long task titles.

## Recommended next step
Program updated Debug firmware and QSPI asset image.
