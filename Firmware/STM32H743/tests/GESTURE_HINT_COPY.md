## Files inspected
TasksWidget, Screen1View and typography/calendar/text/Settings hint generators.

## Files modified
TasksWidget.cpp, Screen1View.cpp, hint generators, custom compiled typography and
calendar assets, regenerated previews and this report.

## LCD hardware configuration confirmed
Unchanged.

## Datasheet / initialization sources used
None; instruction copy only.

## Any unresolved assumptions
Physical readability remains to be checked on the device.

## Architecture implemented
User-visible numeric proximity thresholds replaced with HOLD CLOSE wording.
Generic back hint: HOLD FINGER CLOSE 2s TO GO BACK. Tasks: TAP TO COMPLETE /
HOLD CLOSE 2s TO GO BACK. Settings and Calendar retain navigation hints and append
the new back instruction. Underlying ToF distance/timing behavior is unchanged.

## Build command
cmake --build --preset Debug

## Build result
Passed; existing empty-QSPI objcopy warning remains. Typography generator checks
hint width; calendar generator validates sprite transparency and color limits.

## Memory impact if meaningful
Updated hint pixels in QSPI; no new framebuffer or RAM buffers.

## Hardware tests to perform when PCB arrives
Check footer text readability and clearance on all menus.

## Recommended next step
Program updated firmware and QSPI assets to display the new hints.

## Latest footer text adjustment
Tasks and Reminders now display `TAP TO COMPLETE / HOLD CLOSE TO GO BACK`;
completed selections display `TAP TO REOPEN / HOLD CLOSE TO GO BACK`.
Only TasksWidget.cpp changed for this adjustment. The shared renderer applies
the copy to both lists. The two-second ToF hold threshold is unchanged.
Debug build passed; no QSPI asset update is required for this text-only change.

## Dynamic footer alignment
TasksWidget now measures the active hint with the renderer's glyph advances,
space width and pixel rounding, then centers its sprite span at logical x=240.
Both COMPLETE and REOPEN variants remain centered in Tasks and Reminders.
Inspected and modified TasksWidget.cpp; no hardware or asset changes, new buffers,
or initialization assumptions. Debug build passed; the existing QSPI warning remains.
Verify both states on the device after updating firmware.

## Two-line footer
Tasks and Reminders now display TAP TO COMPLETE (or TAP TO REOPEN) above
HOLD CLOSE TO GO BACK. Each line is independently centered using glyph metrics.
Logical top positions are 422 and 437, with the narrower second line kept within
the circular display. Inspected/modified TasksWidget.cpp; no hardware or QSPI
asset changes. Debug build passed. Verify both lines and completed state on device.

## Settings and Calendar two-line hints
Settings list: RIGHT SELECT above HOLD CLOSE TO GO BACK. Editing: UP/DOWN CHANGE
above the same back instruction. Calendar: LEFT / RIGHT MONTH above HOLD CLOSE
TO GO BACK. Lines are centered individually, separated by 15 pixels. Settings
hint center y=414 clears the divider; Calendar center y=450 clears its status.
Inspected/modified hint generators, Screen1View.cpp and CalendarWidget.cpp;
regenerated custom Typography/Calendar assets and previews. No hardware or
initialization changes. Two-second ToF threshold unchanged. Generator validation
and Debug build passed. Update firmware and QSPI; verify readability on device.
