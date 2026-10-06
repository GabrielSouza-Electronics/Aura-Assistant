# ToF menu gestures

## Files inspected
App hand tracker and sensor task, Tasks application/store, Model/Listener,
Screen1 Presenter/View, MenuLogic, SettingsLogic, TasksLogic, CalendarLogic,
ToF BSP interface, text/hint generators, and native navigation/input tests.

## Files modified
`App/Src/app_hand_tracking.c`, `App/Inc/app_hand_tracking.h`, `App/Src/app.c`,
`App/Src/app_tasks.c`, `App/Inc/app_tasks.h`, Model/Listener, Screen1 Presenter/View,
MenuLogic, SettingsLogic, TasksLogic, CalendarLogic and TasksWidget. Hint generator
sources and custom compiled Typography/Calendar sprites updated. Native input,
Settings and calendar/Tasks navigation tests updated. Lovable API instructions
and audio/offline validation documentation updated to describe ToF completion.

## LCD hardware configuration confirmed
No LCD, clocks, sensor ranging configuration, pins, DMA or MPU changes.

## Datasheet / initialization sources used
Existing BSP_TOF data contract and tracker validation rules. No new initialization
registers or hardware timing assumptions. IMU tap initialization and polling were
removed from SensorTask; component/BSP APIs remain available for future features.

## Any unresolved assumptions
ToF short range and finger returns must be verified on the physical enclosure.
The existing 5 Hz ranging rate samples gestures approximately every 200 ms;
an approach/withdrawal entirely between samples cannot be detected.
The simulator approximates two seconds with 120 GUI ticks.

## Architecture implemented
SensorTask publishes <30 mm proximity state and mutually exclusive short-release
and two-second hold events through the existing thread-safe tracker snapshot.
Timing uses FreeRTOS elapsed ticks, including wraparound, rather than GUI frames.
30 mm itself is outside the close range. Valid withdrawal >=30 mm or an empty
target frame releases a short gesture. Invalid reported targets cancel it.
Frames stale for 600 ms reset gesture timing; stale depth cannot request back.
One hold event is emitted per approach. A long hold never also completes a task.

Model forwards near/back/click through Presenter. View globally closes every open
menu on back and suspends directional input during close proximity. Task completion
occurs on short release, using the existing completion/audio/network logic.
Tasks high-hand exit, Calendar downward exit, Settings left-to-carousel exit and
generic downward submenu exit are removed. Settings left still leaves value-edit
mode. Simulator submenu press/release models proximity; drag models navigation.
Hints in Settings, Calendar, Tasks and other menus now describe the 2 s gesture.

## Build command
`cmake --build --preset Debug`

## Build result
Passed. Simulator `build_executable` also compiled and linked successfully.
Native ToF/Model integration tests, Settings tests and Tasks/calendar/web suite
passed. Coverage includes strict 30 mm threshold, short release, 1999/2000 ms,
one-shot long hold, withdrawal without a target, stale data, tick wrap, directional
exit removal and normal navigation. Existing empty-QSPI objcopy warning remains.

## Memory impact if meaningful
DTCM 125,992 bytes (96.12%); Flash 801,904 bytes (38.24%);
QSPI 5,175,260 bytes (30.85%). Existing DMA buffers unchanged.

## Hardware tests to perform when PCB arrives
In every menu, hold a finger <30 mm for two seconds and verify return once. In
Tasks, approach and withdraw quickly: only the selected task completes and sounds.
Hold a task instead: verify return without completion. Test at/above 30 mm,
finger loss, reconnect, and navigation while the finger remains farther away.

## Recommended next step
Flash both firmware and updated QSPI assets, then validate close-range ToF returns
on the device. Both images are produced by the existing Debug build.
