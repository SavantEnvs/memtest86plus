// USB stick filesystem: fat32_mount() (MBR/GPT partition table, FAT12/16/32 BPB), then what
// memtest86+ does when saving a report: fat32_next_filename() (root directory scan) and
// fat32_write_file() (cluster allocation, FAT chain and directory entry update).
//
// Input: the disk image, 512-byte sectors (a trailing partial sector is not part of the disk).
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "fat32.h"

static const char report[] =
    "Memtest86+ report\r\nPass: 1  Errors: 0\r\n"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\r\n";

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    size_t disk_len = size - size % 512;
    if (disk_len == 0) return -1;
    uint8_t *disk = malloc(disk_len);
    memcpy(disk, data, disk_len);
    disk_set(disk, disk_len);

    usb_msd_t msd;
    memset(&msd, 0, sizeof msd);
    msd.block_size = 512;
    msd.block_count = disk_len / 512;

    uint8_t *buf = malloc(512);
    fat32_fs_t fs;
    memset(&fs, 0, sizeof fs);
    if (fat32_mount(&fs, &msd, buf)) {
        char name[13];
        memset(name, 0, sizeof name);
        if (fat32_next_filename(&fs, name)) {
            fat32_write_file(&fs, name, report, sizeof report - 1);
        }
    }
    free(buf);
    disk_set(NULL, 0);
    free(disk);
    return 0;
}
