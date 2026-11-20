// BIOS/bootloader e820 memory map: pmem_init() — sanitize_e820_map (overlap resolution),
// init_pm_map, sort_pm_map.
//
// Input: byte 0 = boot_params.e820_entries; bytes 1.. = boot_params.e820_map[] (20-byte packed
// addr/size/type entries), zero-filled past the input.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "fakephys.h"
#include "bootparams.h"
#include "pmem.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    boot_params_t *bp = fakephys_boot_params();
    memset(bp, 0, sizeof *bp);
    bp->e820_entries = data[0];
    size_t n = size - 1;
    if (n > sizeof bp->e820_map) n = sizeof bp->e820_map;
    memcpy(bp->e820_map, data + 1, n);

    pmem_init();
    return 0;
}
