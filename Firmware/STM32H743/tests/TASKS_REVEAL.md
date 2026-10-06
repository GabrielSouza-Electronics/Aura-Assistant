## Files inspected
SettingsLogic.cpp reveal/slide formulas, Screen1View Settings transform, TasksWidget.cpp/hpp.

## Files modified
TasksWidget.cpp/hpp and this report.

## LCD hardware configuration confirmed
Unchanged; horizontal slide follows the existing logical Portrait transform.

## Datasheet / initialization sources used
Existing Settings animation; no hardware initialization changes.

## Any unresolved assumptions
Physical animation smoothness must be verified on the device.

## Architecture implemented
Tasks cards use the same Settings smoothstep fade/slide: 4 ticks between rows,
16 ticks per reveal and alternating 44-pixel horizontal displacement. Card panel,
selector, title, deadline, clock, tag and completion strike animate together.
The reveal starts on menu entry, page change and initial arrival of a nonempty
task list. Selection changes and completion do not restart the animation.
Active frames invalidate the widget so the single framebuffer redraws the effect.
The existing header transition and arrow glow remain independent.

## Build command
cmake --build --preset Debug

## Build result
Passed; existing empty-QSPI objcopy warning remains.

## Memory impact if meaningful
One unsigned animation counter; no additional framebuffers or image assets.

## Hardware tests to perform when PCB arrives
Enter Tasks, navigate both example pages, complete a task and verify synchronized
card contents, gradual opacity and no trails. Compare with Settings entry.

## Recommended next step
Program updated firmware. No QSPI update required for this animation.
