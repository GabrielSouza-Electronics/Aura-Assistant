## Files inspected
MenuLogic.cpp/hpp, Screen1View.cpp, existing ToF/Model host integration test.

## Files modified
MenuLogic.cpp/hpp, Screen1View.cpp, tests/test_tof_pointer.py and this report.

## LCD hardware configuration confirmed
Unchanged.

## Datasheet / initialization sources used
No hardware changes.

## Any unresolved assumptions
Physical device behavior remains to be checked.

## Architecture implemented
View previously passed its navigation-enable flag as hand presence. Finger
proximity disabled navigation and therefore falsely faded out the carousel before
the release click opened a menu. MenuLogic now receives actual presence and a
separate proximity flag. Proximity pauses rotation, alignment, dwell selection and
directional cancel while retaining carousel visibility and the selected item.
The release click opens that item; short/long ToF gesture timing is unchanged.

## Build command
cmake --build --preset Debug

## Build result
Passed; existing empty-QSPI objcopy warning remains. Native integration regression
passes: proximity preserves visibility, angle and selection for 120 ticks, never
opens by dwell, and release confirms the retained option.

## Memory impact if meaningful
No new buffers or assets.

## Hardware tests to perform when PCB arrives
Select an off-center carousel item, approach <30 mm and release: verify no fade-out
or selection change before entry. Verify genuine hand withdrawal still hides the
carousel and submenu long-hold return still works.

## Recommended next step
Program updated firmware; no QSPI update required for this fix.
