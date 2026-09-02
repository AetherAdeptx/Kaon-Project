#include <stddef.h>

#include "Disk.h"
#include "io.h"

enum {
    ATA_DATA = 0x1F0,
    ATA_SECTOR_COUNT = 0x1F2,
    ATA_LBA_LOW = 0x1F3,
    ATA_LBA_MID = 0x1F4,
    ATA_LBA_HIGH = 0x1F5,
    ATA_DRIVE = 0x1F6,
    ATA_STATUS_COMMAND = 0x1F7,
    ATA_ALTERNATE_STATUS = 0x3F6,
    ATA_STATUS_ERROR = 0x01,
    ATA_STATUS_DATA_REQUEST = 0x08,
    ATA_STATUS_DEVICE_FAULT = 0x20,
    ATA_STATUS_BUSY = 0x80,
    ATA_COMMAND_READ = 0x20,
    ATA_COMMAND_WRITE = 0x30,
    ATA_COMMAND_CACHE_FLUSH = 0xE7,
    ATA_POLL_LIMIT = 1000000
};

static void ata_delay(void)
{
    for (size_t index = 0; index < 4; ++index) {
        (void)io_read8(ATA_ALTERNATE_STATUS);
    }
}

static bool ata_wait(bool require_data)
{
    for (size_t attempt = 0; attempt < ATA_POLL_LIMIT; ++attempt) {
        const uint8_t status = io_read8(ATA_STATUS_COMMAND);

        if ((status & (ATA_STATUS_ERROR | ATA_STATUS_DEVICE_FAULT)) != 0) {
            return false;
        }

        if ((status & ATA_STATUS_BUSY) == 0
            && (!require_data || (status & ATA_STATUS_DATA_REQUEST) != 0)) {
            return true;
        }
    }

    return false;
}

static bool ata_begin(uint32_t lba, uint8_t sector_count, uint8_t command)
{
    if (sector_count == 0 || lba > 0x0FFFFFFF
        || (uint32_t)(sector_count - 1) > 0x0FFFFFFF - lba
        || !ata_wait(false)) {
        return false;
    }

    io_write8(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    ata_delay();
    io_write8(ATA_SECTOR_COUNT, sector_count);
    io_write8(ATA_LBA_LOW, (uint8_t)lba);
    io_write8(ATA_LBA_MID, (uint8_t)(lba >> 8));
    io_write8(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    io_write8(ATA_STATUS_COMMAND, command);
    return true;
}

bool disk_read_sectors(uint32_t lba, uint8_t sector_count, void *buffer)
{
    uint16_t *words = buffer;

    if (buffer == NULL || !ata_begin(lba, sector_count, ATA_COMMAND_READ)) {
        return false;
    }

    for (size_t sector = 0; sector < sector_count; ++sector) {
        if (!ata_wait(true)) {
            return false;
        }

        for (size_t word = 0; word < DISK_SECTOR_SIZE / 2; ++word) {
            *words++ = io_read16(ATA_DATA);
        }
        ata_delay();
    }

    return true;
}

bool disk_write_sectors(uint32_t lba, uint8_t sector_count,
                        const void *buffer)
{
    const uint16_t *words = buffer;

    if (buffer == NULL || !ata_begin(lba, sector_count, ATA_COMMAND_WRITE)) {
        return false;
    }

    for (size_t sector = 0; sector < sector_count; ++sector) {
        if (!ata_wait(true)) {
            return false;
        }

        for (size_t word = 0; word < DISK_SECTOR_SIZE / 2; ++word) {
            io_write16(ATA_DATA, *words++);
        }
        ata_delay();
    }

    io_write8(ATA_STATUS_COMMAND, ATA_COMMAND_CACHE_FLUSH);
    return ata_wait(false);
}
