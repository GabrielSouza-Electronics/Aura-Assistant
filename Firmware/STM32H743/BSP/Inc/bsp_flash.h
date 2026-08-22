#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define BSP_FLASH_MAPPED_ADDRESS 0x90000000UL
#define BSP_FLASH_SIZE_BYTES     (16UL * 1024UL * 1024UL)

typedef enum
{
    BSP_FLASH_OK = 0,
    BSP_FLASH_ERROR_NOT_INITIALIZED,
    BSP_FLASH_ERROR_INVALID_ARGUMENT,
    BSP_FLASH_ERROR_COMPONENT
} BSP_FLASH_Status_t;

BSP_FLASH_Status_t BSP_FLASH_Init(void);
void BSP_FLASH_ResetState(void);
BSP_FLASH_Status_t BSP_FLASH_ReadJEDECID(uint8_t id[3]);
BSP_FLASH_Status_t BSP_FLASH_Read(uint32_t address, uint8_t *data, size_t length);
BSP_FLASH_Status_t BSP_FLASH_Program(uint32_t address,
                                     const uint8_t *data,
                                     size_t length);
BSP_FLASH_Status_t BSP_FLASH_EraseSector(uint32_t address);
BSP_FLASH_Status_t BSP_FLASH_EraseBlock64K(uint32_t address);
BSP_FLASH_Status_t BSP_FLASH_EnableMemoryMappedMode(void);
BSP_FLASH_Status_t BSP_FLASH_DisableMemoryMappedMode(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_FLASH_H */
