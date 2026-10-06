# Tasks toggle and selection work

## Files inspected
TaskStore/application wrappers, TasksWidget/View/Model audio path, audio owner,
web worker/parser, Tasks asset generator, native Tasks/audio/web/input tests and
AGENTS.md visual preview gate (lines 1509–1511).

## Files modified
Components/Tasks/tasks_data.c/h, App/Src/app_tasks.c and App/Inc/app_tasks.h,
App/Src/app_calendar.c, App/Inc/app_ui_audio.h, App/Src/app_ui_audio.c,
TasksWidget.cpp/hpp, MenuSound.hpp, Model.cpp, Screen1View.cpp, Tasks asset generator
and compiled sprites, Lovable instructions, native test files, previews and report.

## LCD hardware configuration confirmed
No LCD/peripheral/pin/clock/cache/framebuffer changes.

## Datasheet / initialization sources used
Existing audio and ToF infrastructure; no new hardware initialization.

## Any unresolved assumptions
Visual preview approved by the user and integrated. The selected panel and halo
breathe over a 90-tick cycle; the sphere has a brighter shaded core. Animated
repaints are restricted to the selected card after the entry transition settles.
Physical ToF, sound and rendering remain unverified.

## Architecture implemented
Short ToF release toggles the displayed selected task. Completing adds the strike
and success sound; reopening removes the strike and plays the existing menu-exit
PCM, which differs from success. Audio task events use a fixed FIFO of 32 entries,
prioritized over pending navigation audio; on overflow the oldest entry is evicted.
Demo changes remain local. Real pending changes now retain both ID and desired
boolean. PUT sends true/false explicitly. Acknowledgments update the confirmed
state without clearing a newer opposite click while the request was in flight.
Snapshots preserve opposite pending local state until the server catches up.
Tasks footer now describes reopening for a completed selection.

## Build command
cmake --build --preset Debug

## Build result
Debug firmware and simulator build_executable passed. Existing empty-QSPI objcopy
warning remains. Native tests passed toggle/reopen, offline local behavior,
in-flight acknowledgment, rejected reopen rollback, stale snapshot preservation,
both PUT bodies, Model sound mapping and distinct audio PCM/FIFO ordering during
playback. Existing Tasks/navigation/calendar and audio BSP regressions passed.

## Memory impact if meaningful
DTCM 126,032 B; D3 31,360 B; Flash 803,656 B; QSPI 5,269,920 B.
No additional framebuffer/DMA buffers. New visual sprites reside in QSPI.

## Hardware tests to perform when PCB arrives
Complete, move focus away/back, then reopen: verify strike removal and distinct
audio. Repeat offline and online, including clicks during an outstanding PUT.
After visual integration, compare selected/unselected cards and animated glow.

## Recommended next step
Program the updated Debug firmware and QSPI assets; validate sound and rendering
on the device. Firmware and simulator compilation passed after visual integration.

## Prepared visual integration
`aura_assets/preview/tasks_focus/firmware_highlight.patch` contains the proposed
TasksWidget change: 90-tick breathing cycle, selected-card cyan border/fill,
orb halo, and invalidation restricted to the selected card once entry settles.
The patch was applied after approval. The invalidation Rect was made mutable to
match the TouchGFX 4.26.1 API; both firmware and simulator builds then passed.
Check with git apply --check --ignore-space-change, accounting for workspace CRLF.
