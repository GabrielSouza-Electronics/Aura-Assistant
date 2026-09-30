# Settings integration validation

## Behavior

- Left leaves the selected setting. Hold does not leave a second level; return
  to neutral and move left again to exit Settings. A direct right-to-left
  reversal is accepted even when the 5 Hz sensor skips the neutral position.
  Horizontal gestures win diagonal ties and suppress proximity confirmation.
- Wi-Fi is read-only. The owner publishes the associated SSID or DISCONNECTED.
  The App retains up to 32 SSID bytes; the existing GlyphText renderer may
  shorten long names and substitute unsupported characters to fit the row.
- Brightness is 10..100%, in 10% steps, initially 100%. The percentage specifies
  electrical duty cycle, not measured luminance.
- Sound is 0..10, in single steps, using the existing BSP audio volume control.
- Bluetooth starts OFF. Up requests ON, down requests OFF. ON opens a new
  five-minute provisioning window using the existing encrypted BLE GATT
  protocol. Repeated ON while open does not extend the timer. OFF or expiry
  closes admission, stops advertising and disconnects the current BLE client.
  OFF denotes provisioning disabled; the shared Wi-Fi/BLE module stays powered.
  The PC still needs a client implementing the existing provisioning protocol.
  Advertising start/stop failures are available in `ble_adv_status`; retries
  occur in the Wi-Fi task. No AT operation runs in the GUI task.

## Sources inspected and hardware confirmed

Reviewed `SettingsLogic`, `HandInput`, `Screen1View`, `Model`, App settings,
Wi-Fi/provisioning, BSP LCD/audio, the `.ioc`, generated GPIO/TIM initialization,
HAL TIM6 timebase, `main.c`, build presets and the REV01 main-board schematic.

`Hardware/Main_Board/Schematic_PCB_Main_REV01.PDF` connects LCD_BL/PG1 to the
STLD40DPUR enable input. The `.ioc` and generated code configure PG1 as an
active-high output. No timer PWM channel is configured on this pin.

ST's [STLD40D datasheet, Rev 7, section 5.3](https://www.st.com/resource/en/datasheet/stld40d.pdf)
permits low-frequency PWM on EN but does not prescribe this implementation's
100 Hz frequency. The BSP uses ten phases of the existing 1 ms TIM6 callback,
latching brightness at period boundaries. There are no new timer, clock-tree,
NVIC, panel initialization, framebuffer or DMA configuration changes. The
generated-file hook is inside `main.c` USER CODE sections.

The PWM stops progressing if the HAL timebase is suspended or interrupts are
blocked. Check this before adding tickless low-power operation. The current
firmware uses the continuously running timebase.

## Files changed for this task

- `TouchGFX/gui/include/gui/common/SettingsLogic.hpp` and
  `TouchGFX/gui/src/common/SettingsLogic.cpp` implement navigation.
- `App/Inc/app_ui_settings.h`, `App/Src/app_ui_settings.c` bind setting requests.
- `App/Inc/app_provisioning.h`, `App/Src/app_provisioning.c`,
  `App/Src/app_wifi.c` own BLE lifecycle and publish network state.
- `BSP/Inc/bsp_lcd.h`, `BSP/Src/bsp_lcd.c`, `Core/Src/main.c` implement PWM.
- `tests/host/test_settings_nav.cpp`, `tests/test_settings.py`,
  `tests/test_provisioning.c`, `tests/test_provisioning.py` cover regressions.

Pre-existing ToF changes and the unrelated whitespace edit in `main.c` remain.

## Automated validation

```text
python tests/test_settings.py --cc <host-gcc> --cxx <host-g++>
python tests/test_provisioning.py --compiler <host-gcc>
python tests/test_tof_pointer.py --compiler <host-g++>
cmake --build --preset Debug
```

Host tests cover direct horizontal reversal, diagonal back plus click, one-level
back, read-only Wi-Fi, value bounds, brightness write failure, every PWM duty,
off gating, period-boundary updates, BLE startup OFF, reopening, expiry,
disconnect and advertising failures/retries. Hardware commands are faked;
these tests do not validate electrical or RF behavior.

All three host suites passed on this change. The existing Debug CMake build
completed with exit code 0 and produced `AuraAssistant.elf`. The toolchain was
the installed STM32 bundle (CMake 4.3.1, GNU Arm 14.3.1); host tests used the
installed STEdgeAI MinGW compiler. The existing objcopy warning about an empty
loadable segment at `0x90000000` remains.

Final link usage: DTCM 125,784 / 131,072 bytes (95.97%, 5,288 bytes remaining),
AXI SRAM framebuffer 450 / 512 KiB, D2 SRAM 96,960 bytes, D3 SRAM 2,708 bytes,
internal Flash 696,644 bytes, QSPI 2,077,280 bytes. This task adds no dynamic
allocation or DMA buffers. These totals include the user's current UI version.

## Required bench checks / next step

1. At PG1/TP11 measure a 10 ms period, with 1..9 ms high for 10..90% and
   continuously high at 100%. Check current and visible flicker at low levels.
2. Move the finger left from each setting, including diagonal moves and
   simultaneous proximity clicks. Verify one-level exit and intentional rearm.
3. Check audio at 0, 1 and 10, including a change during playback.
4. Turn Bluetooth ON, pair/connect from the PC and update Wi-Fi through the
   existing client. Check OFF, expiry, reconnect and reopening without reboot.
5. Verify the actual SSID, disconnect indication and behavior with long names.

No physical hardware validation was performed by the agent.
