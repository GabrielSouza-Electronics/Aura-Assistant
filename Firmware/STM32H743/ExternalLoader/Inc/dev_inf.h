#ifndef AURA_EXTERNAL_LOADER_DEV_INF_H
#define AURA_EXTERNAL_LOADER_DEV_INF_H

#include <stdint.h>

#define NOR_FLASH 3U
#define SECTOR_NUM 10U

struct DeviceSectors
{
    uint32_t SectorNum;
    uint32_t SectorSize;
};

struct StorageInfo
{
    char DeviceName[100];
    uint16_t DeviceType;
    uint32_t DeviceStartAddress;
    uint32_t DeviceSize;
    uint32_t PageSize;
    uint8_t EraseValue;
    struct DeviceSectors sectors[SECTOR_NUM];
};

#endif
