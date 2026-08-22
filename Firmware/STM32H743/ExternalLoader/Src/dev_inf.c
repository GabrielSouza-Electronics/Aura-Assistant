#include "dev_inf.h"

__attribute__((used, section(".Dev_Info")))
const struct StorageInfo StorageInfo = {
    "W25Q128JVPIM_Aura_STM32H743",
    NOR_FLASH,
    0x90000000UL,
    0x01000000UL,
    0x00001000UL,
    0xFFU,
    {
        {256UL, 0x00010000UL},
        {0UL, 0UL}
    }
};
