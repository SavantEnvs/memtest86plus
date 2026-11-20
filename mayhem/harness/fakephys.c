#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fakephys.h"

uintptr_t boot_params_addr;

static uint8_t *region;
static size_t   used;
#define MAX_COPIES 4096
static void    *copies[MAX_COPIES];
static int      ncopies;
#define MAX_HOST 8
static struct { uintptr_t start; size_t len; } host[MAX_HOST];
static int      nhost;

static void map_fixed(uintptr_t addr, size_t len)
{
    void *p = mmap((void *)addr, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p != (void *)addr) {
        fprintf(stderr, "fakephys: cannot map %#lx (+%#zx)\n", (unsigned long)addr, len);
        abort();
    }
}

bool fakephys_load(const uint8_t *data, size_t size)
{
    if (!region) {
        map_fixed(FAKEPHYS_BASE, FAKEPHYS_SIZE);
        region = (uint8_t *)FAKEPHYS_BASE;
    }
    if (size > FAKEPHYS_SIZE) return false;
    memset(region, 0, used);
    memcpy(region, data, size);
    used = size;
    return true;
}

void fakephys_reset(void)
{
    for (int i = 0; i < ncopies; i++) free(copies[i]);
    ncopies = 0;
}

void fakephys_map_bios_area(void)
{
    static bool done;
    if (!done) { map_fixed(0xE0000, 0x120000); done = true; }
}

uintptr_t map_region(uintptr_t base_addr, size_t size, bool only_for_startup)
{
    (void)only_for_startup;
    if (base_addr >= FAKEPHYS_BASE && base_addr < FAKEPHYS_BASE + FAKEPHYS_SIZE) {
        size_t off = base_addr - FAKEPHYS_BASE;
        if (size > FAKEPHYS_SIZE - off || ncopies >= MAX_COPIES) return 0;
        void *p = malloc(size ? size : 1);
        memcpy(p, region + off, size);
        copies[ncopies++] = p;
        return (uintptr_t)p;
    }
    if (base_addr < 0x100000) {
        // Legacy BIOS area: identity-mapped zero pages (see fakephys_map_bios_area).
        return base_addr >= 0xE0000 ? base_addr : 0;
    }
    // Harness-owned host memory (EFI system table, config tables): passthrough.
    for (int i = 0; i < nhost; i++) {
        if (base_addr >= host[i].start && size <= host[i].len && base_addr - host[i].start <= host[i].len - size)
            return base_addr;
    }
    // Any other firmware physical address: nothing is there in this model.
    return 0;
}

void fakephys_register_host(const void *p, size_t len)
{
    if (nhost < MAX_HOST) { host[nhost].start = (uintptr_t)p; host[nhost].len = len; nhost++; }
}

#include "bootparams.h"
#include "efi.h"

// A UEFI boot: boot_params says "EFI64 loader", its system table has one configuration table
// entry (guid -> table_phys), the way efisetup.c hands the firmware tables to the rest of
// memtest86+.
static boot_params_t        *bp;
static efi64_system_table_t *st;
static efi64_config_table_t *ct;

void fakephys_setup_efi(const void *guid, uintptr_t table_phys)
{
    if (!bp) {
        bp = calloc(1, sizeof *bp);
        st = calloc(1, sizeof *st);
        ct = calloc(1, sizeof *ct);
        fakephys_register_host(st, sizeof *st);
        fakephys_register_host(ct, sizeof *ct);
    }
    memset(bp, 0, sizeof *bp);
    memset(st, 0, sizeof *st);
    memset(ct, 0, sizeof *ct);
    memcpy(&ct->guid, guid, sizeof ct->guid);
    ct->table = table_phys;
    st->num_config_tables = 1;
    st->config_tables = (uintptr_t)ct;
    bp->efi_info.loader_signature = EFI64_LOADER_SIGNATURE;
    bp->efi_info.sys_tab    = (uint32_t)(uintptr_t)st;
    bp->efi_info.sys_tab_hi = (uint32_t)((uint64_t)(uintptr_t)st >> 32);
    boot_params_addr = (uintptr_t)bp;
}

void *fakephys_boot_params(void)
{
    if (!bp) bp = calloc(1, sizeof *bp);
    boot_params_addr = (uintptr_t)bp;
    return bp;
}
