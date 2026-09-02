#ifndef KAON_DISK_H
#define KAON_DISK_H

#include <stdbool.h>
#include <stdint.h>

enum {
    DISK_SECTOR_SIZE = 512
};

bool disk_read_sectors(uint32_t lba, uint8_t sector_count, void *buffer);
bool disk_write_sectors(uint32_t lba, uint8_t sector_count,
                        const void *buffer);

#endif
