# Chat speech test

Source: `chat_test_source.mp3` (the supplied luvvoice recording).
Playback: `chat_test.wav`, mono PCM16, 48 kHz, 236160 samples / 4.92 seconds.
The PCM array lives in internal Flash; the existing D2 DMA buffer is reused.
No changes to I2S frequency, pin assignments, LCD registers, MPU or linker layout.

## Behavior (original version restored)

The 4.92-second recording plays ONCE after the existing Chat entry fade.
Exiting cancels it; re-entering starts it again. There is no repeat timer.
Speak1-8 loops at 120 ms/frame in silence, while waiting and after completion.
Voiced spans select Speak9-14, Speak14-23 or Speak23-37 according to the existing
offline duration/intensity analysis. Each range is mapped linearly to the audio
sample position over its span, at its original speed. Pauses use Speak37-39
at 60 ms/frame (180 ms total), then resume Speak1-8. New speech interrupts
closing immediately. Original alignment at framebuffer Y=82 is preserved.

Frames are selected directly from the audio clock. Slow GUI rendering can skip
frames; this restores the original strategy instead of completing every frame
or easing the animation speed. No extra intermediate frames are inserted.

## Architecture and sources inspected

- `gen_chat_speech.py`: reusable offline PCM/envelope conversion; no audio
  analysis, allocation or word recognition runs in the ISR or GUI.
- `speech_animation.h`: direct frame selection by played sample position.
- View -> Presenter -> Model -> `APP_UIAudio`: request/stop and frame snapshot.
- `APP_UIAudio_Run`: sole audio owner, preserving the existing UI sound queue.
- BSP: circular DMA playback with cancellation and position from NDTR, completed
  cycles and pending transfer-complete flag. The prefill offset is not a clock.
- Windows simulator: MCI WAV playback and queried sample position, using dynamic
  winmm loading. The WAV is resolved relative to `TouchGFX/build/bin`.
- Hardware context checked against `AuraAssistant.ioc`, `Core/Src/i2s.c`, DMA HAL
  macros, callbacks in `Core/Src/main.c`, linker, and Main Board Design Dossier.
  Existing I2S1 configuration is 48 kHz/16-bit with DMA1 Stream0. The dossier
  identifies MAX98357A. No new component register/init values were introduced.

For another recording, convert to 48 kHz mono PCM16 WAV using FFmpeg, then run:

```
python aura_assets/gen/gen_chat_speech.py aura_assets/audio/chat_test.wav
cmake --build --preset Debug
python tests/test_speech.py --compiler <host-gcc>
python tests/test_ui_audio.py --compiler <host-gcc>
```

The generated timeline report is `aura_assets/preview/chat_speech_timeline.json`.
Asset fidelity, frame ranges, skipped ticks, silence, cancellation, rapid
re-entry, DMA rollover and existing UI sound behavior have host tests.

## Validation limits and device check

Compilation and fake-HAL tests do not validate the acoustic/display latency.
Test initial entry, the central pause, final silence, exit during speech,
rapid re-entry, volume zero and playback errors on the board. The DMA position
tracks data consumed by the peripheral; I2S/FIFO, amplifier and LCD scanout add
physical latency that must be measured if visible alignment needs adjustment.
The simulator backend also needs a listening/visual check on Windows.

Validation on this change: Debug CMake build succeeded; both host test scripts
passed. Final memory use: internal Flash 1302932 bytes (62.13%), DTCM 126448
bytes (96.47%), QSPI 16325916 bytes (97.31%). The existing objcopy warning about
an empty loadable segment at 0x90000000 remains. The available host MinGW lacks
standard C++ headers, so a full Windows simulator build was not validated;
portable animation tests use its C++ frontend with C standard-library headers.

The test adds about 461 KiB of PCM in internal Flash and no large RAM buffers.
QSPI usage remains unchanged. Reprogram the internal firmware image to use the
new test; the speech images already exist in the external assets.
