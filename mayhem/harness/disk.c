// USB mass-storage sector I/O over an in-memory disk image (see disk_set() in platform.h); used
// by every target except fuzz_usbmsd, which links the real system/usbmsd.c instead.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "platform.h"
#include "usbmsd.h"

static uint8_t *disk;
static size_t disk_len;
void disk_set(uint8_t *data, size_t len) { disk = data; disk_len = len; }
static bool disk_range(uint64_t lba, uint32_t count, size_t *off, size_t *len)
{
    uint64_t sectors = disk_len / 512;
    if (lba > sectors || count > sectors - lba) return false;
    *off = (size_t)lba * 512; *len = (size_t)count * 512;
    return true;
}
bool msd_read_sectors(usb_msd_t *msd, uint64_t lba, uint32_t count, void *buffer)
{
    (void)msd; size_t off, len;
    if (!disk_range(lba, count, &off, &len)) return false;
    memcpy(buffer, disk + off, len);
    return true;
}
bool msd_write_sectors(usb_msd_t *msd, uint64_t lba, uint32_t count, const void *buffer)
{
    (void)msd; size_t off, len;
    if (!disk_range(lba, count, &off, &len)) return false;
    memcpy(disk + off, buffer, len);
    return true;
}

