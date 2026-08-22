#include "w25q128.h"

#include <stdbool.h>
#include <string.h>

#define W25Q128_CMD_WRITE_ENABLE        0x06U
#define W25Q128_CMD_READ_STATUS_1       0x05U
#define W25Q128_CMD_READ_STATUS_2       0x35U
#define W25Q128_CMD_WRITE_STATUS_2      0x31U
#define W25Q128_CMD_JEDEC_ID            0x9FU
#define W25Q128_CMD_READ_DATA           0x03U
#define W25Q128_CMD_QUAD_OUTPUT_READ    0x6BU
#define W25Q128_CMD_PAGE_PROGRAM        0x02U
#define W25Q128_CMD_SECTOR_ERASE        0x20U
#define W25Q128_CMD_BLOCK_ERASE_64K     0xD8U
#define W25Q128_CMD_ENABLE_RESET        0x66U
#define W25Q128_CMD_RESET_DEVICE        0x99U

#define W25Q128_STATUS_1_BUSY           0x01U
#define W25Q128_STATUS_1_WEL            0x02U
#define W25Q128_STATUS_2_QE             0x02U
#define W25Q128_STATUS_2_SUS            0x80U
#define W25Q128_IO_TIMEOUT_MS           100U
#define W25Q128_PAGE_PROGRAM_TIMEOUT_MS 10U
#define W25Q128_STATUS_WRITE_TIMEOUT_MS 25U
#define W25Q128_INIT_READY_TIMEOUT_MS   5000U
#define W25Q128_SECTOR_ERASE_TIMEOUT_MS 5000U
#define W25Q128_BLOCK_ERASE_TIMEOUT_MS  3000U
#define W25Q128_RESET_DELAY_MS          1U

static const W25Q128_Transfer_t fast_read_transfer = {
    /* Quad Output Fast Read: instruction/address remain on IO0 while payload
       data uses IO0..IO3. W25Q128_Init() verifies/enables QE before mapping. */
    .instruction = W25Q128_CMD_QUAD_OUTPUT_READ,
    .address = 0U,
    .address_lines = W25Q128_LINES_1,
    .data_lines = W25Q128_LINES_4,
    .dummy_cycles = 8U,
    .alternate_byte_enabled = 0U,
    .alternate_byte = 0U
};

static const W25Q128_Transfer_t read_data_transfer = {
    .instruction = W25Q128_CMD_READ_DATA,
    .address = 0U,
    .address_lines = W25Q128_LINES_1,
    .data_lines = W25Q128_LINES_1,
    .dummy_cycles = 0U,
    .alternate_byte_enabled = 0U,
    .alternate_byte = 0U
};

static bool W25Q128_IsRangeValid(uint32_t address, size_t length)
{
    return (length <= W25Q128_CAPACITY_BYTES) &&
           (address < W25Q128_CAPACITY_BYTES) &&
           (length <= (W25Q128_CAPACITY_BYTES - address));
}

static W25Q128_Status_t W25Q128_CheckReady(const W25Q128_Object_t *object)
{
    if ((object == NULL) || (object->initialized == 0U))
    {
        return W25Q128_ERROR_NOT_INITIALIZED;
    }
    if (object->memory_mapped != 0U)
    {
        return W25Q128_ERROR_MEMORY_MAPPED;
    }
    return W25Q128_OK;
}

static W25Q128_Status_t W25Q128_ReadStatus(const W25Q128_Object_t *object,
                                           uint8_t instruction,
                                           uint8_t *status)
{
    const W25Q128_Transfer_t transfer = {
        .instruction = instruction,
        .address = 0U,
        .address_lines = W25Q128_LINES_NONE,
        .data_lines = W25Q128_LINES_1,
        .dummy_cycles = 0U,
        .alternate_byte_enabled = 0U,
        .alternate_byte = 0U
    };

    return (object->io.read(object->io.context, &transfer, status, 1U,
                            W25Q128_IO_TIMEOUT_MS) == 0)
               ? W25Q128_OK
               : W25Q128_ERROR_IO;
}

static W25Q128_Status_t W25Q128_WaitWhileBusy(const W25Q128_Object_t *object,
                                               uint32_t timeout_ms)
{
    uint32_t start = object->io.get_tick(object->io.context);

    for (;;)
    {
        uint8_t status;
        W25Q128_Status_t result = W25Q128_ReadStatus(
            object, W25Q128_CMD_READ_STATUS_1, &status);

        if (result != W25Q128_OK)
        {
            return result;
        }
        if ((status & W25Q128_STATUS_1_BUSY) == 0U)
        {
            return W25Q128_OK;
        }
        if ((uint32_t)(object->io.get_tick(object->io.context) - start) >= timeout_ms)
        {
            return W25Q128_ERROR_TIMEOUT;
        }
        object->io.delay_ms(object->io.context, 1U);
    }
}

static W25Q128_Status_t W25Q128_WriteEnable(const W25Q128_Object_t *object)
{
    const W25Q128_Transfer_t transfer = {
        .instruction = W25Q128_CMD_WRITE_ENABLE,
        .address_lines = W25Q128_LINES_NONE,
        .data_lines = W25Q128_LINES_NONE
    };
    uint8_t status;

    if (object->io.command(object->io.context, &transfer,
                           W25Q128_IO_TIMEOUT_MS) != 0)
    {
        return W25Q128_ERROR_IO;
    }
    if (W25Q128_ReadStatus(object, W25Q128_CMD_READ_STATUS_1, &status) != W25Q128_OK)
    {
        return W25Q128_ERROR_IO;
    }
    return ((status & W25Q128_STATUS_1_WEL) != 0U)
               ? W25Q128_OK
               : W25Q128_ERROR_IO;
}

static W25Q128_Status_t W25Q128_EnableQuadMode(const W25Q128_Object_t *object)
{
    uint8_t status_2;
    W25Q128_Status_t status = W25Q128_ReadStatus(
        object, W25Q128_CMD_READ_STATUS_2, &status_2);

    if ((status != W25Q128_OK) || ((status_2 & W25Q128_STATUS_2_QE) != 0U))
    {
        return status;
    }

    status = W25Q128_WriteEnable(object);
    if (status != W25Q128_OK)
    {
        return status;
    }

    status_2 |= W25Q128_STATUS_2_QE;
    {
        const W25Q128_Transfer_t transfer = {
            .instruction = W25Q128_CMD_WRITE_STATUS_2,
            .address_lines = W25Q128_LINES_NONE,
            .data_lines = W25Q128_LINES_1
        };
        if (object->io.write(object->io.context, &transfer, &status_2, 1U,
                             W25Q128_IO_TIMEOUT_MS) != 0)
        {
            return W25Q128_ERROR_IO;
        }
    }

    status = W25Q128_WaitWhileBusy(object, W25Q128_STATUS_WRITE_TIMEOUT_MS);
    if (status != W25Q128_OK)
    {
        return status;
    }
    status = W25Q128_ReadStatus(object, W25Q128_CMD_READ_STATUS_2, &status_2);
    return ((status == W25Q128_OK) && ((status_2 & W25Q128_STATUS_2_QE) != 0U))
               ? W25Q128_OK
               : W25Q128_ERROR_IO;
}

W25Q128_Status_t W25Q128_RegisterIO(W25Q128_Object_t *object,
                                    const W25Q128_IO_t *io)
{
    if ((object == NULL) || (io == NULL) || (io->read == NULL) ||
        (io->write == NULL) || (io->command == NULL) ||
        (io->enable_memory_mapped == NULL) || (io->abort == NULL) ||
        (io->get_tick == NULL) || (io->delay_ms == NULL))
    {
        return W25Q128_ERROR_INVALID_ARGUMENT;
    }

    (void)memset(object, 0, sizeof(*object));
    object->io = *io;
    object->initialized = 1U;
    return W25Q128_OK;
}

W25Q128_Status_t W25Q128_ReadJEDECID(W25Q128_Object_t *object, uint8_t id[3])
{
    const W25Q128_Transfer_t transfer = {
        .instruction = W25Q128_CMD_JEDEC_ID,
        .address_lines = W25Q128_LINES_NONE,
        .data_lines = W25Q128_LINES_1
    };
    W25Q128_Status_t status = W25Q128_CheckReady(object);

    if ((status != W25Q128_OK) || (id == NULL))
    {
        return (id == NULL) ? W25Q128_ERROR_INVALID_ARGUMENT : status;
    }
    return (object->io.read(object->io.context, &transfer, id, 3U,
                            W25Q128_IO_TIMEOUT_MS) == 0)
               ? W25Q128_OK
               : W25Q128_ERROR_IO;
}

W25Q128_Status_t W25Q128_Init(W25Q128_Object_t *object)
{
    const W25Q128_Transfer_t enable_reset = {
        .instruction = W25Q128_CMD_ENABLE_RESET,
        .address_lines = W25Q128_LINES_NONE,
        .data_lines = W25Q128_LINES_NONE
    };
    const W25Q128_Transfer_t reset = {
        .instruction = W25Q128_CMD_RESET_DEVICE,
        .address_lines = W25Q128_LINES_NONE,
        .data_lines = W25Q128_LINES_NONE
    };
    uint8_t id[3];
    uint8_t status_2;
    W25Q128_Status_t status = W25Q128_CheckReady(object);

    if (status != W25Q128_OK)
    {
        return status;
    }
    status = W25Q128_WaitWhileBusy(object, W25Q128_INIT_READY_TIMEOUT_MS);
    if (status != W25Q128_OK)
    {
        return status;
    }
    status = W25Q128_ReadStatus(object, W25Q128_CMD_READ_STATUS_2, &status_2);
    if ((status != W25Q128_OK) || ((status_2 & W25Q128_STATUS_2_SUS) != 0U))
    {
        return (status == W25Q128_OK) ? W25Q128_ERROR_IO : status;
    }
    if ((object->io.command(object->io.context, &enable_reset,
                            W25Q128_IO_TIMEOUT_MS) != 0) ||
        (object->io.command(object->io.context, &reset,
                            W25Q128_IO_TIMEOUT_MS) != 0))
    {
        return W25Q128_ERROR_IO;
    }
    object->io.delay_ms(object->io.context, W25Q128_RESET_DELAY_MS);

    status = W25Q128_WaitWhileBusy(object, W25Q128_IO_TIMEOUT_MS);
    if (status == W25Q128_OK)
    {
        status = W25Q128_ReadJEDECID(object, id);
    }
    if ((status != W25Q128_OK) || (id[0] != W25Q128_JEDEC_MANUFACTURER) ||
        (id[1] != W25Q128_JEDEC_MEMORY_TYPE) ||
        (id[2] != W25Q128_JEDEC_CAPACITY))
    {
        return (status == W25Q128_OK) ? W25Q128_ERROR_ID : status;
    }
    return W25Q128_EnableQuadMode(object);
}

W25Q128_Status_t W25Q128_Read(W25Q128_Object_t *object, uint32_t address,
                              uint8_t *data, size_t length)
{
    W25Q128_Transfer_t transfer = read_data_transfer;
    W25Q128_Status_t status = W25Q128_CheckReady(object);

    if ((data == NULL) || (length == 0U))
    {
        return W25Q128_ERROR_INVALID_ARGUMENT;
    }
    if (status != W25Q128_OK)
    {
        return status;
    }
    if (!W25Q128_IsRangeValid(address, length))
    {
        return W25Q128_ERROR_OUT_OF_RANGE;
    }
    transfer.address = address;
    return (object->io.read(object->io.context, &transfer, data, length,
                            W25Q128_IO_TIMEOUT_MS) == 0)
               ? W25Q128_OK
               : W25Q128_ERROR_IO;
}

W25Q128_Status_t W25Q128_Program(W25Q128_Object_t *object, uint32_t address,
                                 const uint8_t *data, size_t length)
{
    W25Q128_Status_t status = W25Q128_CheckReady(object);

    if ((data == NULL) || (length == 0U))
    {
        return W25Q128_ERROR_INVALID_ARGUMENT;
    }
    if (status != W25Q128_OK)
    {
        return status;
    }
    if (!W25Q128_IsRangeValid(address, length))
    {
        return W25Q128_ERROR_OUT_OF_RANGE;
    }

    while (length != 0U)
    {
        size_t page_remaining = W25Q128_PAGE_SIZE_BYTES -
                                (address % W25Q128_PAGE_SIZE_BYTES);
        size_t chunk = (length < page_remaining) ? length : page_remaining;
        const W25Q128_Transfer_t transfer = {
            .instruction = W25Q128_CMD_PAGE_PROGRAM,
            .address = address,
            .address_lines = W25Q128_LINES_1,
            .data_lines = W25Q128_LINES_1
        };

        status = W25Q128_WriteEnable(object);
        if (status != W25Q128_OK)
        {
            return status;
        }
        if (object->io.write(object->io.context, &transfer, data, chunk,
                             W25Q128_IO_TIMEOUT_MS) != 0)
        {
            return W25Q128_ERROR_IO;
        }
        status = W25Q128_WaitWhileBusy(object, W25Q128_PAGE_PROGRAM_TIMEOUT_MS);
        if (status != W25Q128_OK)
        {
            return status;
        }
        address += (uint32_t)chunk;
        data += chunk;
        length -= chunk;
    }
    return W25Q128_OK;
}

static W25Q128_Status_t W25Q128_Erase(W25Q128_Object_t *object,
                                      uint32_t address,
                                      uint32_t alignment,
                                      uint8_t instruction,
                                      uint32_t timeout_ms)
{
    W25Q128_Status_t status = W25Q128_CheckReady(object);
    W25Q128_Transfer_t transfer = {
        .instruction = instruction,
        .address = address,
        .address_lines = W25Q128_LINES_1,
        .data_lines = W25Q128_LINES_NONE
    };

    if (status != W25Q128_OK)
    {
        return status;
    }
    if ((address >= W25Q128_CAPACITY_BYTES) || ((address % alignment) != 0U))
    {
        return W25Q128_ERROR_INVALID_ARGUMENT;
    }
    status = W25Q128_WriteEnable(object);
    if (status != W25Q128_OK)
    {
        return status;
    }
    if (object->io.command(object->io.context, &transfer,
                           W25Q128_IO_TIMEOUT_MS) != 0)
    {
        return W25Q128_ERROR_IO;
    }
    return W25Q128_WaitWhileBusy(object, timeout_ms);
}

W25Q128_Status_t W25Q128_EraseSector(W25Q128_Object_t *object, uint32_t address)
{
    return W25Q128_Erase(object, address, W25Q128_SECTOR_SIZE_BYTES,
                         W25Q128_CMD_SECTOR_ERASE,
                         W25Q128_SECTOR_ERASE_TIMEOUT_MS);
}

W25Q128_Status_t W25Q128_EraseBlock64K(W25Q128_Object_t *object,
                                       uint32_t address)
{
    return W25Q128_Erase(object, address, W25Q128_BLOCK_SIZE_BYTES,
                         W25Q128_CMD_BLOCK_ERASE_64K,
                         W25Q128_BLOCK_ERASE_TIMEOUT_MS);
}

W25Q128_Status_t W25Q128_EnableMemoryMappedMode(W25Q128_Object_t *object)
{
    W25Q128_Status_t status = W25Q128_CheckReady(object);

    if (status != W25Q128_OK)
    {
        return status;
    }
    if (object->io.enable_memory_mapped(object->io.context,
                                        &fast_read_transfer) != 0)
    {
        return W25Q128_ERROR_IO;
    }
    object->memory_mapped = 1U;
    return W25Q128_OK;
}

W25Q128_Status_t W25Q128_DisableMemoryMappedMode(W25Q128_Object_t *object)
{
    if ((object == NULL) || (object->initialized == 0U))
    {
        return W25Q128_ERROR_NOT_INITIALIZED;
    }
    if (object->memory_mapped == 0U)
    {
        return W25Q128_OK;
    }
    if (object->io.abort(object->io.context) != 0)
    {
        return W25Q128_ERROR_IO;
    }
    object->memory_mapped = 0U;
    return W25Q128_OK;
}
