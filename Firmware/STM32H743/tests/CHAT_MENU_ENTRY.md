## Files inspected
Screen1View.cpp/hpp, MenuLogic, TasksLogic, SettingsLogic, typography hint
generator and current Debug build preset.

## Files modified
Screen1View.cpp/hpp, install_typography.py, compiled TypographyHints.cpp,
generated hint source assets and this report.

## LCD hardware configuration confirmed
Unchanged. Title positions follow existing Portrait coordinates.

## Datasheet / initialization sources used
No new hardware initialization required.

## Any unresolved assumptions
Device visual timing is not physically validated. Chat conversation/backend
behavior is outside this request; this change implements entry and back UI.

## Architecture implemented
Chat is carousel screen 4. Its title follows the same 14-tick smoothstep
movement from carousel center to header coordinate 74 as Tasks. Chat retains
existing circuit background, status icons and global ToF close-hold back event:
under 30 mm for 2 seconds. The centered footer reads HOLD CLOSE TO GO BACK at
logical vertical center 430; divider/spark use the Settings positions above it.
The existing shared third typography hint was regenerated with this wording.

## Build command
cmake --build --preset Debug

## Build result
Passed. Existing empty-QSPI objcopy warning remains.

## Memory impact if meaningful
DTCM 126,096 bytes, +8 including view alignment; QSPI 5,348,532 bytes after
shortening the shared hint. No new framebuffer or dynamic allocation.

## Hardware tests to perform when PCB arrives
Enter Chat via stationary carousel selection and short proximity click;
verify title slides up, footer is visible and a 2-second close hold returns
to the carousel. Verify Settings/Tasks/Reminders still display their own hints.

## Recommended next step
Program both internal firmware and updated QSPI assets for the changed hint.
