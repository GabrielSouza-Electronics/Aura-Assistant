# Task completion audio validation

## Files inspected
`App/Src/app_ui_audio.c`, `App/Inc/app_ui_audio.h`, `BSP/Src/bsp_audio_out.c`,
`BSP/Inc/bsp_audio_out.h`, `App/Src/app.c`, `MenuSound.hpp`, `TasksWidget.*`,
`Model.cpp`, `Screen1View.cpp`, the UI audio converter, existing audio tests,
`CMakePresets.json`, and the supplied WAV metadata.

## Files modified
`App/Inc/app_ui_audio.h`, `App/Src/app_ui_audio.c`, `BSP/Src/bsp_audio_out.c`,
`TouchGFX/gui/include/gui/common/MenuSound.hpp`, `TasksWidget.hpp`,
`TouchGFX/gui/src/common/TasksWidget.cpp`, `TouchGFX/gui/src/model/Model.cpp`,
`TouchGFX/gui/src/screen1_screen/Screen1View.cpp`, `Components/Audio/ui_audio.c`,
`Components/Audio/ui_audio.h`, `aura_assets/audio/ui/convert.py`,
`tests/test_ui_audio.py`, and this report. Added `aura_assets/audio/ui/40_success.wav`.

## LCD hardware configuration confirmed
No LCD, GPIO, clock, MPU, peripheral initialization, or DMA configuration changes.

## Datasheet / initialization sources used
Existing audio BSP and synchronization hooks; no new hardware initialization.

## Any unresolved assumptions
Physical sound output remains unverified. Completion feedback follows the local
accepted transition, before the asynchronous server acknowledgment. Remote sync
does not replay sounds for already completed tasks.

## Architecture implemented
TasksWidget publishes a separate completion event only when Tasks_Complete succeeds.
Model maps it to the firmware audio owner and to the WAV in the Windows simulator.
The original mono PCM16 48 kHz, 33,600-sample WAV is converted losslessly to a const
array. Completion requests are counted independently of navigation requests;
AudioOutputTask streams the full asset using its existing DMA buffer and volume.
Streaming now preserves the first final-buffer marker until that half is played,
avoiding an endless sequence of empty refills for assets longer than the buffer.

## Build command
`cmake --build --preset Debug`

## Build result
Passed. Existing objcopy warning about the empty QSPI loadable segment remains.
`tests/test_ui_audio.py` passed exact PCM comparison for all four WAV assets,
existing playback/ownership/error cases, and a 33,600-sample streaming regression.

## Memory impact if meaningful
Internal Flash: 802,008 bytes (38.24%), approximately 67.4 KB more.
DTCM: 126,008 bytes (96.14%), eight bytes more. DMA buffer size unchanged.

## Hardware tests to perform when PCB arrives
Complete a task via short ToF proximity release (<30 mm, <2 s) and hear the full sound once; repeat the click on the same
task and verify silence; complete several different tasks quickly and verify queued
feedback; check configured volume and subsequent navigation/startup playback.

## Recommended next step
Listen on the PCB and tune the existing user volume if needed.
