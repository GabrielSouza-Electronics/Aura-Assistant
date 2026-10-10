# Aura Assistant

- [Aura Assistant Dossier PDF](/AuraAssistant_Dossier.pdf)

**Aura Assistant** is a custom embedded smart desktop assistant designed around a dual-PCB architecture, combining high-performance STM32 processing, wireless connectivity, audio capture/playback, a 480×480 graphical interface, gesture sensing, motion sensing and a rechargeable battery power system.

The project was developed from the ground up, including system architecture, schematic design, PCB layout, signal and power integrity considerations, mechanical integration and embedded firmware.

> Hardware Revision: REV01  
> Main MCU: STM32H743ZIT6  
> PCB Design: Altium Designer  
> Firmware: STM32 / C / CMake  
> Status: Hardware Design & Firmware Development

## Hardware Documentation

The Aura Assistant hardware is divided into two custom PCBs: the **Main Board**, responsible for processing, connectivity, display, audio and sensing, and the **Power Board**, responsible for battery management, charging and power distribution.

### Main Board

- [Schematic - Main Board REV01](Hardware/Main_Board/Schematic_PCB_Main_REV01.pdf)
- [PCB Design Dossier - Main Board](Hardware/Main_Board/Aura_MainBoard_Design_Dossier.pdf)
- [PCB Fabrication Drawing - Main Board REV01](Hardware/Main_Board/Fabrication_PCB_Main_REV01.PDF)
- [PCB 3D Views - Main Board REV01](Hardware/Main_Board/3D_PCB_Main_REV01.PDF)

### Power Board

- [Schematic - Power Board REV01](Hardware/PWR_Board/Schematic_PCB_PWR_REV01.PDF)
- [PCB Design Dossier - Power Board](Hardware/PWR_Board/Aura_PWRBoard_Design_Dossier.pdf)
- [PCB Fabrication Drawing - Power Board REV01](Hardware/PWR_Board/Fabrication_PCB_PWR_REV01.PDF)
- [PCB 3D Views - Power Board REV01](Hardware/PWR_Board/3D_PCB_PWR_REV01.PDF)
---

## System Overview

Aura is divided into two custom printed circuit boards:

### Main Board

The **Main Board** is the processing and interface core of the system.

It is built around an **STM32H743ZIT6 ARM Cortex-M7 running at up to 480 MHz** and integrates the display, wireless connectivity, external memory, audio interfaces and environmental/user-interaction sensors.

Main features:

- STM32H743ZIT6 Cortex-M7 MCU
- 480×480 circular TFT display
- Parallel RGB / LTDC display interface
- Wi-Fi / Bluetooth connectivity
- External Quad-SPI NOR Flash
- Dual digital MEMS microphones
- I²S Class-D audio output
- Time-of-Flight gesture sensor
- 6-axis inertial measurement unit
- SWD debugging interface
- Multiple isolated power domains
- FPC connection to the Power Board

### Power Board

The **Power Board** manages the complete power architecture of Aura.

It supports operation from USB-C or from an internal rechargeable Li-Po battery and provides the system power rail required by the Main Board.

Main features:

- USB-C 5 V input
- 3.7 V rechargeable Li-Po battery
- BQ24075 battery charger and power-path controller
- TPS61022 synchronous boost converter
- Battery voltage monitoring
- Power/status signals to the Main Board
- FPC connection between boards
- 10 × WS2812C-2020 addressable RGB LEDs
- Dedicated LED power distribution

---

## System Architecture

                         ┌──────────────────────┐
                         │      USB-C 5 V       │
                         └──────────┬───────────┘
                                    │
                                    ▼
                    ┌───────────────────────────┐
                    │         PWR BOARD         │
                    │                           │
                    │  BQ24075 Power Path       │
                    │        │                  │
                    │        ├── Li-Po Battery  │
                    │        │                  │
                    │        ▼                  │
                    │   TPS61022 Boost          │
                    │        │                  │
                    │        ├── 5 V System     │
                    │        └── RGB LED Ring   │
                    └───────────┬───────────────┘
                                │
                               FPC
                                │
                                ▼
                ┌───────────────────────────────┐
                │          MAIN BOARD           │
                │                               │
                │       STM32H743ZIT6           │
                │         480 MHz               │
                │             │                 │
                │   ┌─────────┼──────────┐      │
                │   │         │          │      │
                │   ▼         ▼          ▼      │
                │ Display    Audio    Connectivity
                │  LTDC    SAI / I²S    Wi-Fi/BLE
                │   │         │          │      │
                │   ▼         ▼          ▼      │
                │ 480×480   2× MEMS   ST67W611M1
                │  TFT       + Amp                │
                │                               │
                │   ┌─────────┼──────────┐      │
                │   ▼         ▼          ▼      │
                │ QSPI       ToF        IMU      │
                │ Flash     Gesture    Motion    │
                └───────────────────────────────┘
