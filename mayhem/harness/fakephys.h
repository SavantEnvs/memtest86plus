// Host model of the firmware-provided physical memory memtest86+ reads at boot.
//
// The fuzz input is placed at the fixed "physical" address FAKEPHYS_BASE, identity-mapped
// (mmap'd once at that address), because some upstream code dereferences firmware physical
// addresses directly (memtest runs with low memory identity-mapped). map_region() — the
// upstream API for touching firmware memory — returns an exact-size heap copy of the
// requested range instead, so ASan reports any read past the length the code asked for.
// Addresses outside the fake region (the harness-owned boot_params / EFI system table) are
// passed through unchanged.
#ifndef MAYHEM_FAKEPHYS_H
#define MAYHEM_FAKEPHYS_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define FAKEPHYS_BASE  0x10000000ul
#define FAKEPHYS_SIZE  0x00200000ul   // 2 MiB of fake firmware memory (zero-filled past the input)

extern uintptr_t boot_params_addr;

// Load `size` bytes of input at FAKEPHYS_BASE (rest of the region zeroed). Returns false if the
// input does not fit in the region.
bool fakephys_load(const uint8_t *data, size_t size);
// Free the per-input map_region copies.
void fakephys_reset(void);
// Map zero-filled pages over the x86 legacy BIOS area (0xE0000-0x1FFFFF) so the upstream
// legacy-table scans read "no table here" instead of faulting on an unmapped host page.
void fakephys_map_bios_area(void);
// Let map_region() pass through a harness-owned host object (e.g. the EFI system table).
void fakephys_register_host(const void *p, size_t len);
// Describe a UEFI boot whose system table has one config table entry `guid` (an efi_guid_t)
// pointing at the fake physical address `table_phys`; sets boot_params_addr.
void fakephys_setup_efi(const void *guid, uintptr_t table_phys);
// A zeroed-once boot_params_t (as void *) owned by the harness; sets boot_params_addr.
void *fakephys_boot_params(void);
#endif
