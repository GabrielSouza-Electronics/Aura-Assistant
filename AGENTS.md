<!-- ==========================================================================
 SINCRONIA: este arquivo existe em duas copias identicas na raiz do
 repositorio, CLAUDE.md e AGENTS.md, porque ferramentas diferentes procuram
 nomes diferentes (Claude Code le CLAUDE.md; Codex le AGENTS.md).

 AO EDITAR, ATUALIZE OS DOIS. Copias que divergem produzem comportamento
 imprevisivel dependendo de qual agente esta rodando, e a causa e dificil de
 diagnosticar depois.
========================================================================== -->

# AuraAssistant — Engineering Context and Agent Instructions

You are working on **AuraAssistant**, a custom embedded AI voice-assistant device designed from scratch around an STM32H743 microcontroller.

Treat this document as persistent engineering context for this repository.

The human engineer responsible for the project is Gabriel Souza.

Communicate explanations, reviews, warnings, plans, and questions to the user in **Brazilian Portuguese** unless explicitly requested otherwise.

Code, identifiers, commit messages, technical terminology, comments, and documentation may remain in English.

---

# 1. CRITICAL WORKING RULES

This is a real custom hardware project that will eventually run on a manufactured PCB.

Do not treat it like a generic STM32 demo project.

Before making hardware-related changes:

1. Inspect the repository.
2. Inspect the current `.ioc`.
3. Inspect generated initialization code.
4. Inspect schematic/datasheet documentation available in the repository.
5. Identify assumptions and conflicts.
6. Prefer current hardware sources over old notes.
7. Never invent register values, timing values, initialization tables, electrical assumptions, pin assignments, or memory locations.

If information is missing, say so explicitly.

DO NOT silently substitute values from another evaluation board or another display/module using the same controller.

---

# 2. SOURCE OF TRUTH PRIORITY

Some older project documents contain stale configuration information.

When sources conflict, use this priority:

1. Current PCB schematic/netlist
2. Current STM32CubeMX `.ioc`
3. Current generated source code
4. Current CMake/linker configuration
5. Latest component/module datasheet
6. Latest CubeMX configuration report
7. Older bring-up / Pin Map documents
8. General assumptions or examples

Do not overwrite a current configuration because an older PDF says something different.

Known stale conflicts are documented below.

---

# 3. PROJECT PURPOSE

AuraAssistant is a desktop embedded AI voice assistant.

Main functionality planned:

- Circular 480x480 graphical interface
- TouchGFX GUI
- Dual PDM microphones
- Speaker output
- Wi-Fi connectivity
- BLE provisioning
- Cloud AI/STT/TTS communication
- IMU sensing
- Time-of-Flight gesture/proximity sensing
- RGB LED feedback
- Battery / USB / charger monitoring
- External QSPI NOR for graphics/assets/data
- FreeRTOS-based firmware architecture

The objective is a professional embedded product architecture, not a quick prototype.

Maintainability, testability, clear hardware abstraction and deterministic embedded behavior are important.

---

# 4. MCU / BOARD

MCU:

STM32H743ZIT6
STM32H743/753 family
LQFP144
ARM Cortex-M7
Maximum project CPU clock: 480 MHz
Internal Flash: 2 MB
Total SRAM: approximately 1 MB distributed across memory domains

Project generated with approximately:

STM32CubeMX 6.18.1
STM32Cube FW_H7 V1.13.0

Always verify current versions from the repository before relying on these numbers.

Custom PCB — NOT a Nucleo or Discovery board.

Do not import assumptions from ST evaluation boards.

---

# 5. BUILD ENVIRONMENT

Primary development environment:

- VS Code
- STM32CubeIDE / STM32 extension for VS Code
- CMake
- Ninja/build system generated through CMake
- GNU Arm Embedded GCC
- Git + GitHub

Observed compiler version during current bring-up:

GNU Arm GCC 14.3.1

Current language configuration is expected to be approximately:

C: C11 / GNU11
C++: C++17 / GNU++17

Verify this from the current root `CMakeLists.txt` before changing it.

Do NOT migrate the project to C17/C23 unless explicitly requested.

---

# 6. CURRENT BUILD STATUS

The firmware currently performs a complete successful CMake build.

Known successful state:

- CubeMX generated code builds
- TouchGFX generated code builds
- FreeRTOS builds
- X-CUBE-ST67W61 middleware builds
- Custom App/BSP/Components structure builds
- Linker completes successfully
- AuraAssistant.elf is produced
- Build exits with code 0

Preserve this known-good baseline.

After modifying source code, always run a CMake build.

Do not declare a task complete if the build is broken.

---

# 7. GIT RULES

The repository is cloned locally from GitHub.

Do not automatically:

- push
- force push
- rebase
- delete branches
- reset --hard
- rewrite history

unless explicitly requested by the user.

You may inspect Git history and diffs.

Before substantial modifications, inspect:

git status

After modifications, show the user:

- files changed
- important diff summary
- build result
- remaining warnings/issues

Do not commit or push unless explicitly requested.

---

# 8. GENERATED CODE RULES

STM32CubeMX owns generated peripheral initialization.

CubeMX has been configured with:

"Generate peripheral initialization as a pair of '.c/.h' files per peripheral"

Therefore Core contains files such as:

Core/Inc/
- adc.h
- dma.h
- gpio.h
- i2c.h
- ltdc.h
- quadspi.h
- sai.h
- spi.h
- tim.h
- usart/uart related headers
- main.h
- freertos.h etc.

Core/Src/
- adc.c
- dma.c
- gpio.c
- i2c.c
- ltdc.c
- quadspi.c
- sai.c
- spi.c
- tim.c
- main.c
- freertos.c
- etc.

Do not move CubeMX-generated peripheral initialization into custom source files.

Do not manually redesign MX_xxx_Init() functions unless explicitly requested.

Only modify generated files inside protected CubeMX sections when possible:

/* USER CODE BEGIN ... */
/* USER CODE END ... */

Never assume code outside USER CODE sections will survive regeneration.

Do not modify STM32 HAL library source files under:

Drivers/STM32H7xx_HAL_Driver/

unless specifically requested.

---

# 9. CUSTOM SOFTWARE ARCHITECTURE

The project now intentionally separates code into:

App/
BSP/
Components/
Core/
Drivers/
Middlewares/
ST67W6X_Network_Driver/
TouchGFX/

The intended dependency direction is:

Application
    ↓
BSP
    ↓
Component Driver
    ↓
STM32 HAL / Middleware
    ↓
Hardware

Do not create circular dependencies between these layers.

---

# 10. APP LAYER

App/
├── Inc/
│   └── app.h
└── Src/
    └── app.c

The App layer represents product/application logic.

Future modules may include:

app_bringup
app_audio
app_wifi
app_sensors
app_power

Do not put direct GPIO register control or low-level sensor transactions in App.

Application code should call BSP APIs.

Example intended flow:

APP_Init()
    ↓
BSP_LCD_Init()
    ↓
ST7701S component driver
    ↓
HAL GPIO / LTDC

---

# 11. BSP LAYER

Current BSP structure includes approximately:

BSP/Inc/
- bsp_lcd.h
- bsp_flash.h
- bsp_imu.h
- bsp_tof.h
- bsp_audio_in.h
- bsp_audio_out.h
- bsp_led.h
- bsp_wifi.h
- bsp_power.h

BSP/Src/
- corresponding `.c` files

BSP represents **this specific Aura PCB**.

BSP is allowed to know:

- STM32 HAL
- GPIO labels
- peripheral handles
- pin assignments
- board-specific power/reset sequences
- DMA instances
- external component wiring

Component drivers should ideally NOT know those board-specific details.

---

# 12. COMPONENT LAYER

Current component directories include approximately:

Components/
├── ST7701S/
├── W25Q128/
├── ISM330DLC/
├── VL53L5CX/
└── WS2812C/

Each component directory contains `.c/.h` driver files.

Component drivers should be portable where practical.

Example:

BAD:

ST7701S component directly manipulating GPIOG pin 0.

GOOD:

ST7701S driver calls an IO abstraction or BSP-provided write operation.

The BSP knows that LCD_CS is PG0.

---

# 13. CMAKELISTS RULES

The root `CMakeLists.txt` is the user customization point.

Custom application files are added using approximately:

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/Src/...
    BSP/Src/...
    Components/.../*.c
)

Include directories are added with:

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App/Inc
    BSP/Inc
    Components/...
)

Do not modify generated CMake files unnecessarily:

cmake/stm32cubemx/...
cmake/touchgfx/...

Do not use recursive file GLOBs without a strong reason.

Prefer explicit source lists in embedded projects.

If new custom `.c` files are created, verify that they are included in the build.

Headers do not generally need to be listed as compiled sources.

---

# 14. CLOCK CONFIGURATION

Current intended clock tree:

HSE:
25 MHz external crystal

LSE:
32.768 kHz external crystal

SYSCLK:
480 MHz

HCLK / AXI / AHB:
240 MHz

APB buses:
approximately 120 MHz

PLL2P:
approximately 49.152 MHz
used by audio/SPI123 clock domain

PLL3R:
16 MHz
used as LTDC pixel clock

QSPI kernel:
HCLK3 approximately 240 MHz

Power:
VOS0

Flash latency:
4 WS

Do not change clock tree without explicit approval.

---

# 15. TOUCHGFX

TouchGFX version currently approximately:

4.26.1

Display configuration:

Interface:
Parallel RGB / LTDC

Resolution:
480 x 480

TouchGFX framebuffer pixel format:
RGB565

Framebuffer size:

480 x 480 x 2 bytes
= 460,800 bytes
= 450 KiB

Framebuffer strategy:
Single Buffer

Buffer location:
By Allocation

DMA2D / Chrom-ART:
Enabled

Application Tick Source:
LTDC

RTOS:
CMSIS-RTOS v2

No external SDRAM exists.

Double full-frame buffering does not fit.

Important workflow:

After CubeMX code regeneration, TouchGFX Designer may need to be opened and "Generate Code" executed before the CMake project builds correctly.

Do not modify generated TouchGFX files under generated directories.

TouchGFX-specific custom code should use the intended user-editable classes/files.

---

# 16. FRAMEBUFFER / LINKER

An important linker correction has already been made.

Initially TouchGFX_Framebuffer was being placed in DTCMRAM and caused:

DTCMRAM overflow by approximately 438 kB.

This was fixed by placing:

TouchGFX_Framebuffer

in AXI SRAM D1:

0x24000000

AXI SRAM size:
512 KiB

Framebuffer:
450 KiB

The linker now successfully places the framebuffer there.

DO NOT accidentally move the framebuffer back into DTCM.

Current memory intent:

0x24000000 AXI SRAM D1
→ TouchGFX RGB565 framebuffer

0x30000000 SRAM1 D2
→ future audio DMA buffers

0x30020000 SRAM2 D2
→ future Wi-Fi/network DMA buffers

0x30040000 SRAM3 D2
→ future ToF/IMU buffers

0x38000000 SRAM4 D3
→ logs / lower priority data

0x20000000 DTCM
→ CPU stacks / DSP hot-path data

0x00000000 ITCM
→ optional critical code/ISR placement

DMA1/DMA2 cannot access DTCM.

Never place DMA buffers in DTCM.

DTCM usage was already relatively high (~83%) during the current successful build, so avoid large new static arrays there.

---

# 17. MPU / CACHE STATUS

Current MPU work has started.

Known intended/current configuration:

Region 0:
- Base 0x00000000
- Size 4 GB
- protection/safety region
- No Access
- XN
- non-cacheable
- SRD approximately 0x87

Region 1:
- QSPI memory mapped region
- Base 0x90000000
- Size 16 MB
- read-only
- execute-never
- cacheable
- non-shareable
- TEX level 0
- bufferable disabled

Higher-number MPU regions override lower-number overlapping regions.

Future MPU regions were planned for framebuffer and DMA buffers, but do NOT assume they have been implemented.

Current I-Cache/D-Cache have intentionally remained disabled during early bring-up.

Do not enable D-Cache or redesign MPU regions without explicit approval.

---

# 18. EXTERNAL QSPI FLASH

IMPORTANT STALE-DOCUMENT WARNING:

An old Pin Map document refers to W25Q32 / 4 MB.

That information is stale.

The current hardware/schematic uses a 128-Mbit QSPI flash, i.e. 16 MB.

Treat current schematic / current `.ioc` / current QSPI FSIZE as source of truth.

Expected current part is in the W25Q128 family; verify exact suffix from schematic/BOM before implementing component-specific behavior.

Current QSPI mapping:

PB2  → QUADSPI_CLK
PG6  → BK1_NCS
PF8  → IO0
PF9  → IO1
PF7  → IO2
PF6  → IO3

Current intended memory-mapped base:

0x90000000

Current FSIZE corresponds to 16 MB.

Current effective QSPI clock target is approximately 120 MHz after bring-up validation.

Do not use old 4 MB assumptions.

---

# 19. LCD / DISPLAY HARDWARE

This is the CURRENT FIRST DEVELOPMENT TARGET.

Display:

Circular TFT
480 x 480
parallel RGB interface

Project documentation refers to module model approximately:

LI48480T028BA3098
or
LLI48480T028BA3098

VERIFY THE EXACT MANUFACTURER PART NUMBER FROM THE CURRENT DATASHEET/REPOSITORY BEFORE IMPLEMENTATION.

Do not assume the spelling above is exact.

Controller is believed/configured around:

ST7701S

Again, verify against the actual module datasheet.

Pixel data is NOT sent through the serial control interface.

Pixel path:

STM32H743
    ↓
LTDC parallel RGB bus
    ↓
LCD panel

The serial pins are only for panel controller initialization/configuration.

---

# 20. LCD GPIO CONTROL

Current CubeMX configuration uses GPIO bit-banging for LCD controller serial setup.

IMPORTANT:

Old project notes mention I2C3.

The CURRENT generated configuration uses:

PG0  → LCD_CS
PG1  → LCD_BL
PE13 → LCD_RST
PA8  → LCD_SCL
PC9  → LCD_SDA

PA8 and PC9 are currently configured as GPIO outputs, NOT normal I2C3 peripheral operation.

Do not convert these pins back to I2C unless the actual module datasheet and user explicitly require it.

Current intended states:

LCD_RST:
PE13
active LOW

LCD_CS:
PG0
active LOW
idle HIGH

LCD_BL:
PG1
active HIGH

LCD_SCL:
PA8

LCD_SDA:
PC9

Determine the exact controller serial protocol from the display/controller datasheet before implementing it.

Do NOT guess whether the protocol is:

- I2C
- SPI
- 3-wire SPI
- 9-bit serial
- command/data encoded serial

Read the exact datasheet.

---

# 21. LTDC CONFIGURATION

Physical LTDC interface:

RGB888 / 24-bit bus

TouchGFX framebuffer:
RGB565

Current starting LTDC timing:

HSYNC width = 8
HBP = 20
Active Width = 480
HFP = 20

VSYNC = 4
VBP = 16
Active Height = 480
VFP = 16

Approximate pixel clock:
16 MHz

Current signal polarities:

HSYNC Active Low
VSYNC Active Low
DE Active Low
Pixel clock / data sampling: current configuration uses normal/rising behavior

These timing values were selected as bring-up starting values.

They MUST be verified against the exact LCD module datasheet before considering the display production-ready.

Do not silently change them.

---

# 22. LCD PARALLEL RGB PINOUT

Current LTDC bus includes approximately:

RED
PG13 R0
PA2  R1
PA1  R2
PB0  R3
PA5  R4
PC0  R5
PB1  R6
PE15 R7

GREEN
PE5  G0
PE6  G1
PA6  G2
PE11 G3
PB10 G4
PB11 G5
PC7  G6
PG8  G7

BLUE
PG14 B0
PA10 B1
PD6  B2
PD10 B3
PE12 B4
PA3  B5
PB8  B6
PB9  B7

SYNC
PE14 LTDC_CLK
PC6  LTDC_HSYNC
PA4  LTDC_VSYNC
PF10 LTDC_DE

Verify current generated source if any discrepancy exists.

---

# 23. EXPECTED LCD BRING-UP SEQUENCE

Conceptual intended sequence:

1. HAL initialization
2. clock configuration
3. GPIO initialization
4. keep LCD reset asserted and backlight OFF during startup
5. release LCD reset according to datasheet timing
6. send controller initialization sequence through LCD_CS/LCD_SCL/LCD_SDA
7. configure/use LTDC
8. configure/use DMA2D
9. clear/fill framebuffer
10. verify basic colors/test pattern
11. turn backlight ON
12. only after low-level display is stable, bring TouchGFX into the test

Exact ordering relative to generated MX_LTDC_Init() must be determined by inspecting the current `main.c`.

Do not restructure CubeMX-generated startup unnecessarily.

Use USER CODE sections and/or APP_Init() to integrate application initialization.

---

# 24. FREERTOS

FreeRTOS:

CMSIS-RTOS v2
FreeRTOS 10.6.x approximately

Preemption enabled
Tick approximately 1000 Hz
heap_4
dynamic and static allocation enabled
timers enabled

Current total RTOS heap was approximately:

96,000 bytes

Current tasks created during CubeMX planning include:

SystemTask
priority Normal
stack ~1024

TouchGFXTask
priority Normal
stack ~4096

AudioInputTask
priority High
stack ~2048

AudioOutputTask
priority High
stack ~1024

WiFiTask
priority AboveNormal
stack ~2048

SensorTask
priority Normal
stack ~1024

PowerTask
priority Low
stack ~512

LEDTask
priority BelowNormal
stack ~512

LoggingTask
priority Low
stack ~1024

Inspect current `freertos.c` because generated values are the source of truth.

Do not put large application implementations directly inside generated FreeRTOS task functions.

Preferred future pattern:

generated task wrapper
    ↓
APP_xxxTask()
    ↓
BSP
    ↓
driver

---

# 25. INTERRUPT / NVIC CONSTRAINTS

Important current priority concept:

configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5

Known configured priority scheme approximately:

TIM6 HAL timebase → 4

I2S TX DMA → 5
SAI RX DMA → 5
SPI1 → 5
LTDC → 5

ST67 SPI RX DMA → 6
ST67 SPI TX DMA → 6
SPI2 → 6
WIFI_RDY EXTI4 → 6
SAI1 → 6
DMA2D → 6

WS2812 DMA → 7
ToF EXTI3 → 7

IMU EXTI groups → 8

SysTick/PendSV → 15

Do not change NVIC priorities casually.

Any ISR calling FreeRTOS FromISR APIs must respect the FreeRTOS interrupt-priority restrictions.

---

# 26. AUDIO INPUT

Two PDM MEMS microphones.

Current hardware:

both microphones share data pin PC1
SAI1_D1

clock:
PE2
SAI1_CK1

SAI mode:
PDM microphone mode
MicPairsNbr = 1
CK1 enabled

The two microphones are distinguished by their L/R configuration / opposite clock edges.

An old configuration report had incorrect SAI settings.

THE SAI CONFIGURATION WAS MANUALLY CORRECTED AFTER THAT REPORT.

Therefore:

current `.ioc` and generated `sai.c` are authoritative.

Do not restore old SAI settings from old documentation.

Target PDM clock is approximately:

2.048 MHz

DMA:
circular
peripheral → memory
halfword
high priority

Audio DMA buffers should eventually reside in D2 SRAM, not DTCM.

---

# 27. AUDIO OUTPUT

MAX98357A class-D I2S amplifier.

Current I2S1 pins:

PG10 → I2S1_WS
PG11 → I2S1_CK
PB5  → I2S1_SDO

PB6:
I2S_SDMODE GPIO

Current target:

48 kHz
16-bit
I2S master transmit

DMA:
circular
memory → peripheral
halfword
high priority

Known REV01 hardware issue:

the MAX98357A SD_MODE resistor network may prevent true GPIO shutdown.

Do not assume PB6 can fully disable the amplifier.

Stopping BCLK may be required to mute/self-shutdown on REV01.

Do not redesign hardware behavior in firmware without noting this limitation.

---

# 28. IMU

Component:

ISM330DLC

Interface:

I2C4

Pins:

PF14 → SCL
PF15 → SDA

Interrupts:

PF13 → IMU_INT1
PE7  → IMU_INT2

Expected WHO_AM_I:
0x6A

Known hardware issue:

SA0 may be at an undefined intermediate voltage due to resistor configuration.

Therefore firmware may need to probe addresses:

0x6A / 0x6B

before assuming a fixed address.

Prefer ST's official component driver when appropriate rather than recreating every register definition manually.

---

# 29. TOF SENSOR

Component:

VL53L5CX

Interface:

I2C2

PF0 → SDA
PF1 → SCL
PF2 → TOF_LPn
PF3 → TOF_INT

TOF_INT:
falling-edge EXTI

Expected I2C address around:
0x29

Use ST's official VL53L5CX ULD/driver where practical.

Do not rewrite the full ST driver from scratch unnecessarily.

---

# 30. RGB LED

10x WS2812C-2020 style LEDs.

Data:

PA9
TIM1_CH2

Timer clock:
approximately 240 MHz

Current PWM timing:

PSC = 0
ARR = 299

Resulting bit cell:
approximately 800 kHz

DMA:
memory → peripheral
normal mode
halfword
medium priority

Known hardware caveat:

5 V WS2812 logic VIH may be marginal with 3.3 V STM32 output.

Firmware cannot fully solve an electrical threshold problem.

If failures occur, identify this as a possible hardware cause.

---

# 31. POWER / BATTERY

Battery ADC:

PC5
ADC1_INP8

ADC:
16-bit
oversampling approximately x16
right shift 4
long sampling time

Power status:

PA7 → CHARGER
PC4 → USB_STATUS

These are active-low style charger status signals from the power board.

Known hardware caveat:

REV01 charger status pull-ups may reference 5 V and are not ideal for STM32 pins.

Do not hide this issue in firmware.

---

# 32. ST67W611 Wi-Fi / BLE

Connectivity module:

ST67W611M1 family

Official middleware:

X-CUBE-ST67W61 v1.3.x approximately

Architecture:
T01

Enabled middleware components:

ATDriver
ServiceAPI

ServiceShell:
disabled

Device Application:
Not Selected

Reason:
TouchGFX already occupies the CubeMX "Application" slot.

This is intentional.

Do not remove TouchGFX to select the ST67 User Application.

Aura provides its own WiFiTask/application integration.

Enabled ST67 functional modules:

WIFI_STATION = Yes
WIFI_SAP = No
BLE = Yes
NET = Yes
MQTT = No initially

Power-save auto:
disabled for initial bring-up

32 kHz clock:
External passive crystal

The PCB contains a 32.768 kHz passive crystal on the ST67.

DHCP:
STA client

Wi-Fi autoconnect:
disabled during initial development

BLE hostname:
AuraAssistant

Network hostname:
should also be AuraAssistant

---

# 33. ST67 SPI INTERFACE

Primary operational interface:

SPI2

PB13 → SPI2_SCK
PB14 → SPI2_MISO
PB15 → SPI2_MOSI

PB12 → WIFI_CS
manual GPIO chip-select

PB4 → WIFI_RDY
EXTI rising/falling

PB3 → WIFI_EN / CHIP_EN

PD8 → WIFI_BOOT

PF11 → WIFI_PWR_EN

UART4:

PD0 RX
PD1 TX

UART is intended mainly for flashing/manufacturing/debug, NOT the normal data path.

SPI2 configuration approximately:

Full Duplex Master
8 bit
MSB First
CPOL Low
CPHA first edge / Mode 0
NSS Software

Kernel:
PLL2P ~49.152 MHz

Prescaler /2 currently gives approximately:

24.576 MHz

This is below the ST67 maximum SPI clock.

DMA:

DMA1 Stream2 → SPI2 RX
Peripheral → Memory
Normal
Byte
High

DMA1 Stream3 → SPI2 TX
Memory → Peripheral
Normal
Byte
High

---

# 34. ST67 CUSTOM GPIO ALIAS FIX

The ST middleware initially expected generic board symbols:

CHIP_EN_Pin
CHIP_EN_GPIO_Port

SPI_RDY_Pin
SPI_RDY_GPIO_Port

SPI_CS_Pin
SPI_CS_GPIO_Port

The custom board uses CubeMX labels:

WIFI_EN_Pin
WIFI_EN_GPIO_Port

WIFI_RDY_Pin
WIFI_RDY_GPIO_Port

WIFI_CS_Pin
WIFI_CS_GPIO_Port

The build was repaired by adding aliases inside a USER CODE section of `spi_port.c`, conceptually:

#define CHIP_EN_Pin       WIFI_EN_Pin
#define CHIP_EN_GPIO_Port WIFI_EN_GPIO_Port

#define SPI_RDY_Pin       WIFI_RDY_Pin
#define SPI_RDY_GPIO_Port WIFI_RDY_GPIO_Port

#define SPI_CS_Pin        WIFI_CS_Pin
#define SPI_CS_GPIO_Port  WIFI_CS_GPIO_Port

Do not delete this adaptation unless replacing it with a cleaner equivalent.

Keep it regeneration-safe.

---

# 35. ST67 BOOT INTENT

Conceptual boot:

WIFI_PWR_EN low initially
WIFI_EN low initially
WIFI_BOOT low for mission mode
WIFI_CS inactive

Then:

PF11 WIFI_PWR_EN high
wait >= ~5 ms
PB3 WIFI_EN high
wait for PB4 WIFI_RDY / module ready event

Then middleware initialization and AT communication.

Do not enable Wi-Fi during LCD implementation unless needed.

---

# 36. DEBUG

Hardware has not arrived yet.

There is currently no PCB/ST-LINK connected.

Therefore current validation is limited to:

- compilation
- linking
- static inspection
- unit-testable host-independent code
- memory-map review
- generated-code review

Do not claim hardware behavior has been validated.

When hardware arrives, primary debugger will be:

ST-LINK
SWD

PA13 → SWDIO
PA14 → SWCLK

PB3 is used by WIFI_EN, therefore SWO is not normally available.

---

# 37. CURRENT PROJECT PHASE

Folder architecture, CubeMX generation, CMake integration, TouchGFX
generation, ST67 middleware integration, linker correction and Git workflow
are complete.

LCD bring-up is complete. The display runs and the TouchGFX simulator builds
and runs on Windows.

The UI layer is implemented and running: three custom widgets draw the entire
interface (see section 38). Assets are generated by Python scripts outside the
TouchGFX project and installed into it (see section 41).

Remaining work, roughly in order:

1. Content for the five menu screens. They currently render "NO CONTENT YET".
2. Audio drivers (SAI in/out) and Wi-Fi (ST67 over SPI).
3. UI sound effects stored in QSPI. There are ~14.5 MB free; PCM 16-bit
   16 kHz mono is adequate and can be DMA'd straight from memory-mapped
   flash without a decoder.
4. Measure per-frame render time and evaluate tearing. See section 40.

---

# 38. UI ARCHITECTURE

The entire interface is drawn by three custom widgets in
`Firmware/STM32H743/TouchGFX/gui/{include/gui,src}/common/`:

**`ParticleField`** — background stars, galaxy dust, the particle sphere,
the golden core, nebula pulses, and the cyan rim brightness.

**`OrbitalMenu`** — five-option carousel, gestures, screen navigation, and
the icons that drift across the idle screen.

**`StatusBar`** — breathing logo with arc, Wi-Fi level, battery.

`InitialView` only instantiates, positions and forwards inputs. **Nothing is
added through TouchGFX Designer** — every widget is added in code so the
coordinates live in one place.

## Draw order (this is the order of `add()`)

```
Box #000818 → nebulas → galaxy disc → OrbitalMenu → ParticleField
→ cyan rim → StatusBar
```

## States

| State | Behaviour |
|---|---|
| Idle | galaxy visible, stars bright, menu icons drifting |
| Hand present | galaxy slides down and fades, sphere forms, carousel appears |
| Menu open | carousel hides, title centred, context label under the logo |

## System inputs

```cpp
view.setHand(present, x, y);      // x,y in -1..+1, from the ToF
view.setVoiceActive(bool);        // wake word
view.setSpeaking(bool);           // TTS speaking -> rim breathes
view.setSpeakingLevel(0..1);      // better: real RMS of the audio buffer
view.setWifiLevel(0..3);
view.setBatteryLevel(0..100);
view.setChargerStatus(bool);
```

## Interaction parameters (validated in the Python simulator)

```cpp
DEAD_ZONE   0.13    // a trembling hand rotates nothing
SPIN_GAIN  -0.055   // negative = inverted direction
SNAP_K      0.16    // magnet that aligns to the nearest option
HYST        0.60    // prevents flicker at the boundary between options
DWELL_N     72      // 1.2 s of dwell opens the option
ACT_UP/DOWN ±0.55   // vertical gesture threshold
ACT_HOLD    12      // ticks above threshold before firing
```

The `armed` flag blocks a new confirmation after a cancel, until the user
either rotates the carousel or removes their hand. Without it the dwell timer
kept running and re-opened the very menu the user had just rejected.

---

# 39. UI PITFALLS ALREADY DISCOVERED

These cost real debugging time. Do not repeat them.

1. **`near`, `far` and `CY` are Windows macros/types** (`windef.h`), and the
   simulator's MinGW pulls them in through `HAL.hpp`. Short identifiers like
   these collide. The error appears **only in the simulator** — compiling for
   the target passes silently.

2. **`TextureMapper` depends on optional framework features.** Support for
   each image format is toggled in Config → Framework Features. With
   L8_ARGB8888 disabled it draws nothing — no error, no warning. For this
   reason **no widget here uses TextureMapper**. Scaling comes from sprites
   pre-rendered at several sizes, with the code picking the nearest.

3. **Compressed images do not work with all widgets** and disable the pure
   DMA2D path. Compression is deliberately off.

4. **Text is sprites, not `TextArea`.** A TextArea would require creating each
   string and configuring a font in Designer; configuration dependencies have
   already failed silently here. The strings are fixed and few.

5. **Sprites need alpha 0 at their edges.** A glow clipped by the sprite
   boundary leaves the whole rectangle faintly opaque — it shows as a box
   around the object against the dark background.

6. **L8_ARGB8888 has a 256-colour palette.** Mixing two colour ramps freely
   overflows it. Derive everything from one quantised scalar.

7. **Bitmap IDs must be consecutive** wherever the code does
   `BITMAP_X_00_ID + i`. Each sequence lives in its own folder for this
   reason; a stray file in that folder breaks the arithmetic with no compile
   error.

8. **When an asset is renamed, three places need updating**: the generator,
   the Python simulator and the firmware. The simulator's `try/except` hides
   the failure and the scene simply renders incomplete.

---

# 40. RENDER BUDGET AND TEARING

At a 16 MHz pixel clock with typical blanking (540x525):

| | |
|---|---:|
| Frame | 17.7 ms |
| Active area (scanline crossing the screen) | 16.2 ms |
| Vertical blanking | 1.5 ms |

With a single framebuffer, blanking is the only window where drawing is
guaranteed safe, and the scene needs 5–11 ms. So drawing **will** overlap the
scanline; the question is whether it stays ahead of it.

Estimated load: ~565 k px per frame, of which ~448 k px is background
(nebulas, galaxy, rim) that is **static and redrawn unnecessarily** —
`ParticleField` currently invalidates the whole screen every tick.

Highest-value optimisation available: **region-based invalidation**. It should
remove most of that 80% without touching the pixel clock.

Fallbacks if that is not enough:
- 12 MHz pixel clock → 42 fps and double the margin
- external SDRAM in a board revision, for double buffering

---

# 41. ASSET PIPELINE

Assets are generated by Python scripts in `Firmware/STM32H743/aura_assets/`,
**outside** the TouchGFX project.

```
Aura-Assistant/
├── CLAUDE.md                       <- this file, repository root
├── AGENTS.md   README.md
├── Documentation/  Hardware/  Mechanical/
└── Firmware/
    └── STM32H743/
        ├── AuraAssistant.ioc
        ├── CMakeLists.txt
        ├── STM32H743xx_FLASH.ld
        ├── App/  BSP/  Components/  Core/  Drivers/
        ├── Middlewares/  ST67W6X_Network_Driver/
        ├── TouchGFX/
        │   ├── assets/images/aura/ <- only the installed PNGs
        │   └── gui/                <- the widgets that actually compile
        └── aura_assets/            <- generators, simulators, source PNGs
```

`aura_assets/` sits beside `TouchGFX/`, inside `Firmware/STM32H743/`, because
it is specific to this firmware target.

It is **not part of the build**. Keep it out of `TouchGFX/`: the
Designer scans `assets/` and `gui/`, and the build walks `gui/src/`
recursively — source PNGs and reference copies of the widgets there would be
imported and compiled by mistake.

| Script | Generates |
|---|---|
| `gen_stars.py` | particle sprites (5 types x 10 frames) |
| `gen_nebula.py` | 3 nebulas + 6 pulse stages each |
| `gen_galaxy.py` | spiral disc + core at 7 sizes |
| `gen_icons.py` | 5 icons x 3 sizes, progress ring, arrows |
| `gen_status.py` | Wi-Fi (4 levels), battery frame |
| `gen_logo.py` | logo + arc, 20 breathing frames |
| `gen_text.py` | labels and messages as sprites |
| `gen_rim.py` | cyan display rim |

Install into the project:

Run from inside `aura_assets/`, pointing at the TouchGFX project:

```
cd Firmware/STM32H743/aura_assets
python instalar_touchgfx.py ../TouchGFX   # copies PNGs to assets/images/aura/
python gerar_config.py     ../TouchGFX   # writes application.config
```

Current asset usage: ~1.6 MB of the 16 MB QSPI.

## Python simulators

`sim_particles.py` and `sim_menu.py` reproduce the physics and interaction in
Python using **the same constants** as the C++. They exist so parameters can
be tested in seconds instead of a compile-and-flash cycle.

**If you change a constant in C++, change it in the simulator too** — otherwise
the preview lies. This has already caused confusion several times: status bar
positions, core sprite names, carousel direction.

---

# 42. BUILD VALIDATION

After modifications, run the project's existing Debug CMake build.

Determine the existing preset rather than inventing a new build system.

Expected workflow is similar to:

cmake --preset Debug
cmake --build --preset Debug

or the equivalent already defined in this repository.

The existing project currently builds successfully.

Your changes must preserve that.

If compilation fails:

- investigate
- fix only problems introduced or exposed by this work
- do not mask warnings/errors unnecessarily

Do not modify compiler flags globally just to silence an issue.

---

# 43. TASK COMPLETION REPORT

At completion report:

## Files inspected

## Files modified

## LCD hardware configuration confirmed

## Datasheet / initialization sources used

## Any unresolved assumptions

## Architecture implemented

## Build command

## Build result

## Memory impact if meaningful

## Hardware tests to perform when PCB arrives

## Recommended next step

Do not state that the LCD works physically because hardware is not currently connected.

"Build successful" and "hardware validated" are different milestones.

---

# 44. GENERAL ENGINEERING STYLE

Use professional embedded-C conventions.

Priorities:

- readability
- deterministic behavior
- clear ownership
- explicit errors
- no unnecessary dynamic memory
- no hidden hardware assumptions
- reusable drivers
- compile-time checks where useful
- fixed-width integer types for hardware-facing code
- const-correct tables
- minimal ISR work
- DMA-safe memory placement
- maintainability through CubeMX regeneration

Avoid overengineering.

Do not introduce elaborate design patterns where a simple C interface is sufficient.

Do not write code just because a placeholder file exists.

Implement one hardware vertical slice at a time and preserve a working build.

---

# 45. IMPORTANT FINAL INSTRUCTION

This document provides historical context, but the CURRENT REPOSITORY IS ALWAYS THE FINAL SOURCE OF TRUTH.

Before acting on any value from this document, compare it against current:

- schematic
- `.ioc`
- generated code
- linker script
- CMake
- datasheets

If they disagree, do not silently choose one.

Tell the user exactly what conflicts and why.
