#include "quadspi.h"
#include "stm32h7xx_hal.h"

#include <stdint.h>

#define LOADER_KEEP __attribute__((used, noinline))
#define QSPI_BASE_ADDRESS 0x90000000UL
#define QSPI_ADDRESS_MASK 0x00FFFFFFUL
#define W25Q128_BLOCK_SIZE_64K 0x00010000UL
#define W25Q128_CMD_CHIP_ERASE 0xC7U
#define W25Q128_CMD_READ_STATUS_1 0x05U
#define W25Q128_STATUS_BUSY 0x01U
#define W25Q128_STATUS_WEL 0x02U
#define LOADER_CHIP_ERASE_TIMEOUT_MS 210000UL
#define LOADER_INIT_ERROR_HAL 0xE1
#define LOADER_INIT_ERROR_CLOCK 0xE2
#define LOADER_INIT_ERROR_QSPI 0xE3
#define LOADER_INIT_ERROR_FLASH 0xE4
#define LOADER_INIT_ERROR_MEMORY_MAPPED 0xE5
#define LOADER_ERASE_ERROR_WRITE_ENABLE 0xF1
#define LOADER_ERASE_ERROR_COMMAND 0xF2
#define LOADER_ERASE_ERROR_STATUS_COMMAND 0xF3
#define LOADER_ERASE_ERROR_STATUS_RECEIVE 0xF4
#define LOADER_ERASE_ERROR_TIMEOUT 0xF5
#define W25Q128_CMD_PAGE_PROGRAM 0x02U
#define LOADER_PAGE_PROGRAM_TIMEOUT_MS 100UL

QSPI_HandleTypeDef hqspi;

static uint32_t loader_tick_ms;
static int loader_msp_ready;

static int QSPI_PeripheralInit(void);
static int QSPI_EnableMemoryMapped(void);
static int QSPI_ExitMemoryMapped(void);
static int QSPI_WriteEnable(void);
static int QSPI_WaitReady(uint32_t timeout);
static void Loader_TimeInit(void);

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    (void)TickPriority;
    return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
    if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0UL)
    {
        ++loader_tick_ms;
    }
    return loader_tick_ms;
}

void HAL_Delay(uint32_t Delay)
{
    uint32_t start = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - start) < Delay)
    {
        __NOP();
    }
}

void Error_Handler(void)
{
    for (;;)
    {
    }
}

LOADER_KEEP int Init(void)
{
    __disable_irq();
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    Loader_TimeInit();
    if (!QSPI_PeripheralInit())
    {
        return LOADER_INIT_ERROR_QSPI;
    }
    /* Erase/program/verify use direct QSPI commands below.  Do not run the
       component driver's stateful initialization here: CubeProgrammer calls
       Init again before verification, after the flash has already been
       programmed. */
    if (QSPI_WaitReady(100UL) != 1)
    {
        return LOADER_INIT_ERROR_FLASH;
    }
    /* CubeProgrammer verifies external flash by reading its mapped address
       directly after Init(), rather than necessarily calling Verify(). */
    return QSPI_EnableMemoryMapped();
}

LOADER_KEEP int Write(uint32_t Address, uint32_t Size, uint8_t *buffer)
{
    uint32_t offset = Address & QSPI_ADDRESS_MASK;
    QSPI_CommandTypeDef command = {0};

    if ((buffer == NULL) || (Size == 0UL) || (Size > 0x01000000UL) ||
        (offset > (0x01000000UL - Size)))
    {
        return 0;
    }
    if (!QSPI_ExitMemoryMapped())
    {
        return 0;
    }
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    /* Use the standard 1-1-1 Page Program command.  The former 0x32
       command depends on the non-volatile QE bit and made programming
       sensitive to the flash state left by a previous loader/application.
       Erase does not exercise that path, which is why erase could remain
       reliable while the second CubeProgrammer write buffer failed. */
    command.Instruction = W25Q128_CMD_PAGE_PROGRAM;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.DummyCycles = 0U;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    while (Size != 0UL)
    {
        uint32_t page_remaining = 256UL - (offset & 0xFFUL);
        uint32_t chunk = (Size < page_remaining) ? Size : page_remaining;
        int ready_status;

        if (!QSPI_WriteEnable())
        {
            return 0;
        }
        command.Address = offset;
        command.NbData = chunk;
        if (HAL_QSPI_Command(&hqspi, &command,
                             HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        {
            return 0;
        }
        if (HAL_QSPI_Transmit(&hqspi, buffer,
                              HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        {
            return 0;
        }
        ready_status = QSPI_WaitReady(LOADER_PAGE_PROGRAM_TIMEOUT_MS);
        if (ready_status != 1)
        {
            return 0;
        }
        offset += chunk;
        buffer += chunk;
        Size -= chunk;
    }
    return 1;
}

LOADER_KEEP int SectorErase(uint32_t EraseStartAddress,
                            uint32_t EraseEndAddress)
{
    QSPI_CommandTypeDef command = {0};
    uint32_t address = (EraseStartAddress & QSPI_ADDRESS_MASK) &
                       ~(W25Q128_BLOCK_SIZE_64K - 1UL);
    uint32_t end = EraseEndAddress & QSPI_ADDRESS_MASK;

    if (!QSPI_ExitMemoryMapped())
    {
        return 0;
    }

    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = 0xD8U;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_NONE;
    command.DummyCycles = 0U;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    while (address <= end)
    {
        if (!QSPI_WriteEnable())
        {
            return LOADER_ERASE_ERROR_WRITE_ENABLE;
        }
        command.Address = address;
        if (HAL_QSPI_Command(&hqspi, &command,
                             HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        {
            return LOADER_ERASE_ERROR_COMMAND;
        }
        {
            int ready_status = QSPI_WaitReady(3000UL);
            if (ready_status == -1)
            {
                return LOADER_ERASE_ERROR_STATUS_COMMAND;
            }
            if (ready_status == -2)
            {
                return LOADER_ERASE_ERROR_STATUS_RECEIVE;
            }
            if (ready_status != 1)
            {
                return LOADER_ERASE_ERROR_TIMEOUT;
            }
        }
        if (address > (QSPI_ADDRESS_MASK - W25Q128_BLOCK_SIZE_64K))
        {
            break;
        }
        address += W25Q128_BLOCK_SIZE_64K;
    }
    return 1;
}

LOADER_KEEP int MassErase(uint32_t Parallelism)
{
    QSPI_CommandTypeDef command = {0};
    (void)Parallelism;

    if (!QSPI_ExitMemoryMapped() || !QSPI_WriteEnable())
    {
        return 0;
    }
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = W25Q128_CMD_CHIP_ERASE;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_NONE;
    command.DummyCycles = 0U;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    if ((HAL_QSPI_Command(&hqspi, &command, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) ||
        (QSPI_WaitReady(LOADER_CHIP_ERASE_TIMEOUT_MS) != 1))
    {
        return 0;
    }
    return 1;
}

LOADER_KEEP uint32_t CheckSum(uint32_t StartAddress, uint32_t Size,
                              uint32_t InitVal)
{
    const uint8_t *memory = (const uint8_t *)(uintptr_t)StartAddress;
    uint32_t index;

    if (!QSPI_EnableMemoryMapped())
    {
        return 0UL;
    }
    for (index = 0UL; index < Size; ++index)
    {
        InitVal += memory[index];
    }
    return InitVal;
}

LOADER_KEEP uint64_t Verify(uint32_t MemoryAddr, uint32_t RAMBufferAddr,
                            uint32_t Size, uint32_t missalignement)
{
    const uint8_t *memory = (const uint8_t *)(uintptr_t)MemoryAddr;
    const uint8_t *buffer = (const uint8_t *)(uintptr_t)RAMBufferAddr;
    uint32_t byte_count = Size * 4UL;
    uint32_t leading = missalignement & 0xFUL;
    uint32_t trailing = (missalignement >> 16) & 0xFUL;
    uint32_t checksum;
    uint32_t index;

    if (!QSPI_EnableMemoryMapped())
    {
        return (uint64_t)MemoryAddr;
    }
    if (trailing > byte_count)
    {
        return (uint64_t)MemoryAddr;
    }
    checksum = 0UL;
    for (index = leading; index < (byte_count - trailing); ++index)
    {
        checksum += memory[index];
    }

    for (index = 0UL; index < byte_count; ++index)
    {
        if (memory[index] != buffer[index])
        {
            return ((uint64_t)checksum << 32) |
                   (uint64_t)(MemoryAddr + index);
        }
    }
    return (uint64_t)checksum << 32;
}

static int QSPI_PeripheralInit(void)
{
    /* External loaders do not pass through the C runtime startup code, so
       .bss contents must not be assumed to be zero. HAL_QSPI_Init() only
       calls the MSP initializer when State is HAL_QSPI_STATE_RESET. */
    hqspi = (QSPI_HandleTypeDef){0};
    loader_msp_ready = 0;
    hqspi.Instance = QUADSPI;
    hqspi.Init.ClockPrescaler = 3U; /* Reset HSI/HCLK 64 MHz / 4 = 16 MHz. */
    hqspi.Init.FifoThreshold = 4U;
    hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hqspi.Init.FlashSize = 23U;
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_7_CYCLE;
    hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
    hqspi.Init.FlashID = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
    return (HAL_QSPI_Init(&hqspi) == HAL_OK) && loader_msp_ready;
}

void HAL_QSPI_MspInit(QSPI_HandleTypeDef *handle)
{
    GPIO_InitTypeDef gpio = {0};

    if (handle->Instance != QUADSPI)
    {
        return;
    }

    /* D1HCLK is QSPISEL=0. Select it directly so loader bring-up does not
       depend on unrelated fields of RCC_PeriphCLKInitTypeDef. */
    CLEAR_BIT(RCC->D1CCIPR, RCC_D1CCIPR_QSPISEL);
    __HAL_RCC_QSPI_CLK_ENABLE();
    __HAL_RCC_QSPI_FORCE_RESET();
    __NOP();
    __HAL_RCC_QSPI_RELEASE_RESET();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOF, &gpio);
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOF, &gpio);
    gpio.Pin = GPIO_PIN_2;
    gpio.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_6;
    gpio.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOG, &gpio);
    loader_msp_ready = 1;
}

static int QSPI_EnableMemoryMapped(void)
{
    QSPI_CommandTypeDef command = {0};
    QSPI_MemoryMappedTypeDef memory_mapped = {0};

    if (hqspi.State == HAL_QSPI_STATE_BUSY_MEM_MAPPED)
    {
        return 1;
    }

    /* 0x0B Fast Read uses one line for instruction, address and data.  It
       therefore does not depend on the flash QE bit during verification. */
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = 0x0BU;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.DummyCycles = 8U;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    memory_mapped.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    memory_mapped.TimeOutPeriod = 0U;

    return HAL_QSPI_MemoryMapped(&hqspi, &command, &memory_mapped) == HAL_OK;
}

static int QSPI_ExitMemoryMapped(void)
{
    return HAL_QSPI_Abort(&hqspi) == HAL_OK;
}

static int QSPI_WriteEnable(void)
{
    QSPI_CommandTypeDef command = {0};
    uint8_t status;
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = 0x06U;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_NONE;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    if (HAL_QSPI_Command(&hqspi, &command,
                         HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return 0;
    }
    command.Instruction = W25Q128_CMD_READ_STATUS_1;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = 1U;
    if ((HAL_QSPI_Command(&hqspi, &command,
                          HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) ||
        (HAL_QSPI_Receive(&hqspi, &status,
                          HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK))
    {
        return 0;
    }
    return (status & W25Q128_STATUS_WEL) != 0U;
}

static int QSPI_WaitReady(uint32_t timeout_ms)
{
    QSPI_CommandTypeDef command = {0};
    uint8_t status;
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = W25Q128_CMD_READ_STATUS_1;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = 1U;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    uint32_t start = HAL_GetTick();
    for (;;)
    {
        if (HAL_QSPI_Command(&hqspi, &command,
                             HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        {
            return -1;
        }
        if (HAL_QSPI_Receive(&hqspi, &status,
                             HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        {
            return -2;
        }
        if ((status & W25Q128_STATUS_BUSY) == 0U)
        {
            return 1;
        }
        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
        {
            return -3;
        }
        /* Do not sleep between page-program status reads. CubeProgrammer
           uploads the next ping-pong buffer while Write() runs and expects
           this buffer to complete in roughly the SWD upload interval. */
    }
}

static void Loader_TimeInit(void)
{
    uint32_t cycles_per_ms = SystemCoreClock / 1000UL;

    if (cycles_per_ms == 0UL)
    {
        cycles_per_ms = 1UL;
    }
    SysTick->CTRL = 0UL;
    SysTick->LOAD = cycles_per_ms - 1UL;
    SysTick->VAL = 0UL;
    loader_tick_ms = 0UL;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}
