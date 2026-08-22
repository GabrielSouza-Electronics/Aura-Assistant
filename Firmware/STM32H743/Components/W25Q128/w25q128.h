#ifndef W25Q128_H
#define W25Q128_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define W25Q128_CAPACITY_BYTES       (16UL * 1024UL * 1024UL)
#define W25Q128_PAGE_SIZE_BYTES      256UL
#define W25Q128_SECTOR_SIZE_BYTES    4096UL
#define W25Q128_BLOCK_SIZE_BYTES     (64UL * 1024UL)
#define W25Q128_JEDEC_MANUFACTURER   0xEFU
#define W25Q128_JEDEC_MEMORY_TYPE    0x70U
#define W25Q128_JEDEC_CAPACITY       0x18U

typedef enum
{
    W25Q128_OK = 0,
    W25Q128_ERROR_INVALID_ARGUMENT,
    W25Q128_ERROR_NOT_INITIALIZED,
    W25Q128_ERROR_IO,
    W25Q128_ERROR_TIMEOUT,
    W25Q128_ERROR_ID,
    W25Q128_ERROR_OUT_OF_RANGE,
    W25Q128_ERROR_MEMORY_MAPPED
} W25Q128_Status_t;

typedef enum
{
    W25Q128_LINES_NONE = 0,
    W25Q128_LINES_1 = 1,
    W25Q128_LINES_2 = 2,
    W25Q128_LINES_4 = 4
} W25Q128_Lines_t;

typedef struct
{
    uint8_t instruction;
    uint32_t address;
    W25Q128_Lines_t address_lines;
    W25Q128_Lines_t data_lines;
    uint8_t dummy_cycles;
    uint8_t alternate_byte_enabled;
    uint8_t alternate_byte;
} W25Q128_Transfer_t;

typedef int32_t (*W25Q128_Read_Fn)(void *context,
                                   const W25Q128_Transfer_t *transfer,
                                   uint8_t *data,
                                   size_t length,
                                   uint32_t timeout_ms);
typedef int32_t (*W25Q128_Write_Fn)(void *context,
                                    const W25Q128_Transfer_t *transfer,
                                    const uint8_t *data,
                                    size_t length,
                                    uint32_t timeout_ms);
typedef int32_t (*W25Q128_Command_Fn)(void *context,
                                      const W25Q128_Transfer_t *transfer,
                                      uint32_t timeout_ms);
typedef int32_t (*W25Q128_MemoryMapped_Fn)(void *context,
                                           const W25Q128_Transfer_t *transfer);
typedef int32_t (*W25Q128_Abort_Fn)(void *context);
typedef uint32_t (*W25Q128_GetTick_Fn)(void *context);
typedef void (*W25Q128_DelayMs_Fn)(void *context, uint32_t delay_ms);

typedef struct
{
    void *context;
    W25Q128_Read_Fn read;
    W25Q128_Write_Fn write;
    W25Q128_Command_Fn command;
    W25Q128_MemoryMapped_Fn enable_memory_mapped;
    W25Q128_Abort_Fn abort;
    W25Q128_GetTick_Fn get_tick;
    W25Q128_DelayMs_Fn delay_ms;
} W25Q128_IO_t;

typedef struct
{
    W25Q128_IO_t io;
    uint8_t initialized;
    uint8_t memory_mapped;
} W25Q128_Object_t;

W25Q128_Status_t W25Q128_RegisterIO(W25Q128_Object_t *object,
                                    const W25Q128_IO_t *io);
W25Q128_Status_t W25Q128_Init(W25Q128_Object_t *object);
W25Q128_Status_t W25Q128_ReadJEDECID(W25Q128_Object_t *object,
                                     uint8_t id[3]);
W25Q128_Status_t W25Q128_Read(W25Q128_Object_t *object,
                              uint32_t address,
                              uint8_t *data,
                              size_t length);
W25Q128_Status_t W25Q128_Program(W25Q128_Object_t *object,
                                 uint32_t address,
                                 const uint8_t *data,
                                 size_t length);
W25Q128_Status_t W25Q128_EraseSector(W25Q128_Object_t *object,
                                     uint32_t address);
W25Q128_Status_t W25Q128_EraseBlock64K(W25Q128_Object_t *object,
                                       uint32_t address);
W25Q128_Status_t W25Q128_EnableMemoryMappedMode(W25Q128_Object_t *object);
W25Q128_Status_t W25Q128_DisableMemoryMappedMode(W25Q128_Object_t *object);

#ifdef __cplusplus
}
#endif

#endif /* W25Q128_H */
