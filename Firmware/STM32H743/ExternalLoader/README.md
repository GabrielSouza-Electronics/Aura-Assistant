# Aura W25Q128 STM32CubeProgrammer External Loader

External loader for the AuraAssistant custom board:

- MCU: STM32H743ZIT6
- Memory: W25Q128JVPIM, JEDEC ID `EF 70 18`
- Capacity: 16 MiB
- Mapped address: `0x90000000`
- Erase block exposed to STM32CubeProgrammer: 64 KiB
- CubeProgrammer page: 4 KiB (the driver still programs 256-byte flash pages)
- Loader QSPI clock: 24 MHz
- Loader execution RAM: DTCM at `0x20000004`

The loader uses the board pinout from `AuraAssistant.ioc`: PB2 CLK, PG6 NCS,
PF8 IO0, PF9 IO1, PF7 IO2 and PF6 IO3.

The flash also supports 4 KiB erase sectors, but the external loader advertises
64 KiB blocks and uses block erase (`0xD8`). This avoids STM32CubeProgrammer's
operation timeout when programming multi-megabyte TouchGFX asset images.

Loader timeouts use SysTick in polling mode, without interrupts. DWT/CYCCNT is
not used because its counting state is not guaranteed while CubeProgrammer
controls the core; a stopped DWT makes HAL timeout loops permanent.

For bring-up diagnostics, `Init()` returns `0xE1` through `0xE5` for HAL,
clock, QSPI peripheral, flash component and memory-mapped setup failures,
respectively. STM32CubeProgrammer requires the success value to be exactly 1,
and its verbosity-level-3 register dump exposes the failure code in R0.

QSPI uses the reset-default D1HCLK kernel source (`QSPISEL=0`). The loader
selects it directly and resets the QSPI peripheral before HAL initialization,
preventing a stale BUSY state after attaching to a previously running target.
The HAL QSPI handle is also cleared explicitly because an external loader does
not execute the normal C runtime that guarantees zero-initialized `.bss`.
The loader likewise resets the BSP flash object on every `Init()` so stale RAM
from a prior loader invocation cannot make BSP initialization short-circuit.
It does not call `SystemInit()` or `HAL_Init()`: interrupts are disabled and the
loader initializes only its polling timebase and QSPI. This avoids redirecting
VTOR to erased internal flash while CubeProgrammer owns execution.

Code and state live in DTCM at `0x20000004`; STM32CubeProgrammer keeps its stack
and two large download buffers in AXI SRAM. The ELF uses a single RWX load
segment with four-byte alignment (`nmagic`), matching ST's official external
loader format. Page size is advertised as 4 KiB, as in the official W25Q128
loader, while the component driver still splits writes at the physical
256-byte program-page boundaries.

Init, erase and write keep QSPI in indirect mode. Memory-mapped mode is enabled
only on demand by `Verify()` and `CheckSum()`. This avoids aborting an active
memory-mapped transaction at the start of every operation and keeps the erase
and programming paths deterministic.

During bring-up, `SectorErase()` returns `0xF1` for Write Enable, `0xF2` for
the `0xD8` command, `0xF3` for the Read Status command, `0xF4` for status-byte
reception, or `0xF5` for a real BUSY timeout.

`Write()` diagnostics encode the failing flash offset in the low 24 bits. The
high byte is `A1` for Write Enable, `A2` for Page Program command, `A3` for data
transmit, `A4` for Read Status command, `A5` for status receive, or `A6` for a
page-program BUSY timeout.
Page-program BUSY polling is intentionally continuous rather than delayed by
1 ms per page, so a 255 KiB write buffer finishes before CubeProgrammer's
concurrent SWD upload of the next ping-pong buffer.

## Build

Configure this directory with the repository GNU Arm toolchain file:

```powershell
cmake -S ExternalLoader -B ExternalLoader/build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=gcc-arm-none-eabi.cmake
cmake --build ExternalLoader/build
```

The output is `ExternalLoader/build/AuraW25Q128_STM32H743.stldr`.

## STM32CubeProgrammer

Copy the `.stldr` to the CubeProgrammer `bin/ExternalLoader` directory or
select it explicitly in the External Loaders dialog. Program
`build/Debug/AuraAssistant_ExternalFlash.bin` at `0x90000000`.
