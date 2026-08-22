#include "bsp_flash.h"

#include "quadspi.h"
#include "w25q128.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

static W25Q128_Object_t flash_device;
static bool flash_initialized;
static bool flash_memory_mapped;

void BSP_FLASH_ResetState(void)
{
    (void)memset(&flash_device, 0, sizeof(flash_device));
    flash_initialized = false;
    flash_memory_mapped = false;
}

static uint32_t BSP_FLASH_HALAddressMode(W25Q128_Lines_t lines)
{
    switch (lines)
    {
        case W25Q128_LINES_1:
            return QSPI_ADDRESS_1_LINE;
        case W25Q128_LINES_2:
            return QSPI_ADDRESS_2_LINES;
        case W25Q128_LINES_4:
            return QSPI_ADDRESS_4_LINES;
        case W25Q128_LINES_NONE:
        default:
            return QSPI_ADDRESS_NONE;
    }
}

static uint32_t BSP_FLASH_HALDataMode(W25Q128_Lines_t lines)
{
    switch (lines)
    {
        case W25Q128_LINES_1:
            return QSPI_DATA_1_LINE;
        case W25Q128_LINES_2:
            return QSPI_DATA_2_LINES;
        case W25Q128_LINES_4:
            return QSPI_DATA_4_LINES;
        case W25Q128_LINES_NONE:
        default:
            return QSPI_DATA_NONE;
    }
}

static void BSP_FLASH_BuildCommand(const W25Q128_Transfer_t *transfer,
                                   size_t length,
                                   QSPI_CommandTypeDef *command)
{
    *command = (QSPI_CommandTypeDef){0};
    command->InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command->Instruction = transfer->instruction;
    command->AddressMode = BSP_FLASH_HALAddressMode(transfer->address_lines);
    command->AddressSize = QSPI_ADDRESS_24_BITS;
    command->Address = transfer->address;
    command->AlternateByteMode = transfer->alternate_byte_enabled
                                     ? QSPI_ALTERNATE_BYTES_4_LINES
                                     : QSPI_ALTERNATE_BYTES_NONE;
    command->AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    command->AlternateBytes = transfer->alternate_byte;
    command->DataMode = BSP_FLASH_HALDataMode(transfer->data_lines);
    command->DummyCycles = transfer->dummy_cycles;
    command->NbData = (uint32_t)length;
    command->DdrMode = QSPI_DDR_MODE_DISABLE;
    command->DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command->SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
}

static int32_t BSP_FLASH_IO_Read(void *context,
                                 const W25Q128_Transfer_t *transfer,
                                 uint8_t *data,
                                 size_t length,
                                 uint32_t timeout_ms)
{
    QSPI_CommandTypeDef command;
    QSPI_HandleTypeDef *qspi = (QSPI_HandleTypeDef *)context;

    if ((transfer == NULL) || (data == NULL) || (length == 0U) ||
        (length > UINT32_MAX))
    {
        return -1;
    }
    BSP_FLASH_BuildCommand(transfer, length, &command);
    if (HAL_QSPI_Command(qspi, &command, timeout_ms) != HAL_OK)
    {
        return -1;
    }
    return (HAL_QSPI_Receive(qspi, data, timeout_ms) == HAL_OK) ? 0 : -1;
}

static int32_t BSP_FLASH_IO_Write(void *context,
                                  const W25Q128_Transfer_t *transfer,
                                  const uint8_t *data,
                                  size_t length,
                                  uint32_t timeout_ms)
{
    QSPI_CommandTypeDef command;
    QSPI_HandleTypeDef *qspi = (QSPI_HandleTypeDef *)context;

    if ((transfer == NULL) || (data == NULL) || (length == 0U) ||
        (length > UINT32_MAX))
    {
        return -1;
    }
    BSP_FLASH_BuildCommand(transfer, length, &command);
    if (HAL_QSPI_Command(qspi, &command, timeout_ms) != HAL_OK)
    {
        return -1;
    }
    return (HAL_QSPI_Transmit(qspi, (uint8_t *)(uintptr_t)data, timeout_ms) == HAL_OK)
               ? 0
               : -1;
}

static int32_t BSP_FLASH_IO_Command(void *context,
                                    const W25Q128_Transfer_t *transfer,
                                    uint32_t timeout_ms)
{
    QSPI_CommandTypeDef command;

    if (transfer == NULL)
    {
        return -1;
    }
    BSP_FLASH_BuildCommand(transfer, 0U, &command);
    return (HAL_QSPI_Command((QSPI_HandleTypeDef *)context,
                             &command,
                             timeout_ms) == HAL_OK)
               ? 0
               : -1;
}

static int32_t BSP_FLASH_IO_EnableMemoryMapped(
    void *context,
    const W25Q128_Transfer_t *transfer)
{
    QSPI_CommandTypeDef command;
    QSPI_MemoryMappedTypeDef memory_mapped = {0};

    if (transfer == NULL)
    {
        return -1;
    }
    BSP_FLASH_BuildCommand(transfer, 0U, &command);
    memory_mapped.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    memory_mapped.TimeOutPeriod = 0U;
    return (HAL_QSPI_MemoryMapped((QSPI_HandleTypeDef *)context,
                                  &command,
                                  &memory_mapped) == HAL_OK)
               ? 0
               : -1;
}

static int32_t BSP_FLASH_IO_Abort(void *context)
{
    return (HAL_QSPI_Abort((QSPI_HandleTypeDef *)context) == HAL_OK) ? 0 : -1;
}

static uint32_t BSP_FLASH_IO_GetTick(void *context)
{
    (void)context;
    return HAL_GetTick();
}

static void BSP_FLASH_IO_DelayMs(void *context, uint32_t delay_ms)
{
    (void)context;
    HAL_Delay(delay_ms);
}

static BSP_FLASH_Status_t BSP_FLASH_FromComponentStatus(W25Q128_Status_t status)
{
    switch (status)
    {
        case W25Q128_OK:
            return BSP_FLASH_OK;
        case W25Q128_ERROR_INVALID_ARGUMENT:
        case W25Q128_ERROR_OUT_OF_RANGE:
            return BSP_FLASH_ERROR_INVALID_ARGUMENT;
        case W25Q128_ERROR_NOT_INITIALIZED:
            return BSP_FLASH_ERROR_NOT_INITIALIZED;
        default:
            return BSP_FLASH_ERROR_COMPONENT;
    }
}

BSP_FLASH_Status_t BSP_FLASH_Init(void)
{
    const W25Q128_IO_t io = {
        .context = &hqspi,
        .read = BSP_FLASH_IO_Read,
        .write = BSP_FLASH_IO_Write,
        .command = BSP_FLASH_IO_Command,
        .enable_memory_mapped = BSP_FLASH_IO_EnableMemoryMapped,
        .abort = BSP_FLASH_IO_Abort,
        .get_tick = BSP_FLASH_IO_GetTick,
        .delay_ms = BSP_FLASH_IO_DelayMs
    };
    W25Q128_Status_t status;

    if (flash_initialized)
    {
        return BSP_FLASH_OK;
    }

    flash_initialized = false;
    flash_memory_mapped = false;
    status = W25Q128_RegisterIO(&flash_device, &io);
    if (status == W25Q128_OK)
    {
        status = W25Q128_Init(&flash_device);
    }
    if (status != W25Q128_OK)
    {
        return BSP_FLASH_FromComponentStatus(status);
    }
    flash_initialized = true;
    return BSP_FLASH_OK;
}

BSP_FLASH_Status_t BSP_FLASH_ReadJEDECID(uint8_t id[3])
{
    if (!flash_initialized)
    {
        return BSP_FLASH_ERROR_NOT_INITIALIZED;
    }
    return BSP_FLASH_FromComponentStatus(W25Q128_ReadJEDECID(&flash_device, id));
}

BSP_FLASH_Status_t BSP_FLASH_Read(uint32_t address, uint8_t *data, size_t length)
{
    if (!flash_initialized)
    {
        return BSP_FLASH_ERROR_NOT_INITIALIZED;
    }
    return BSP_FLASH_FromComponentStatus(
        W25Q128_Read(&flash_device, address, data, length));
}

BSP_FLASH_Status_t BSP_FLASH_Program(uint32_t address,
                                     const uint8_t *data,
                                     size_t length)
{
    if (!flash_initialized)
    {
        return BSP_FLASH_ERROR_NOT_INITIALIZED;
    }
    return BSP_FLASH_FromComponentStatus(
        W25Q128_Program(&flash_device, address, data, length));
}

BSP_FLASH_Status_t BSP_FLASH_EraseSector(uint32_t address)
{
    if (!flash_initialized)
    {
        return BSP_FLASH_ERROR_NOT_INITIALIZED;
    }
    return BSP_FLASH_FromComponentStatus(
        W25Q128_EraseSector(&flash_device, address));
}

BSP_FLASH_Status_t BSP_FLASH_EraseBlock64K(uint32_t address)
{
    if (!flash_initialized)
    {
        return BSP_FLASH_ERROR_NOT_INITIALIZED;
    }
    return BSP_FLASH_FromComponentStatus(
        W25Q128_EraseBlock64K(&flash_device, address));
}

BSP_FLASH_Status_t BSP_FLASH_EnableMemoryMappedMode(void)
{
    if (!flash_initialized)
    {
        return BSP_FLASH_ERROR_NOT_INITIALIZED;
    }
    if (flash_memory_mapped)
    {
        return BSP_FLASH_OK;
    }
    {
        BSP_FLASH_Status_t status = BSP_FLASH_FromComponentStatus(
            W25Q128_EnableMemoryMappedMode(&flash_device));
        if (status == BSP_FLASH_OK)
        {
            flash_memory_mapped = true;
        }
        return status;
    }
}

BSP_FLASH_Status_t BSP_FLASH_DisableMemoryMappedMode(void)
{
    if (!flash_initialized)
    {
        return BSP_FLASH_ERROR_NOT_INITIALIZED;
    }
    if (!flash_memory_mapped)
    {
        return BSP_FLASH_OK;
    }
    {
        BSP_FLASH_Status_t status = BSP_FLASH_FromComponentStatus(
            W25Q128_DisableMemoryMappedMode(&flash_device));
        if (status == BSP_FLASH_OK)
        {
            flash_memory_mapped = false;
        }
        return status;
    }
}
