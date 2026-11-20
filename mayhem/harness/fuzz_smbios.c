// SMBIOS/DMI tables from firmware: smbios_init() (entry point checksum, DMI struct walk, string
// capture) then the two upstream consumers that print what it found.
//
// Input: byte 0 bit 0 picks the EFI config table GUID the firmware advertises (0 = SMBIOS 2.x
// "_SM_" entry point, 1 = SMBIOS 3.x "_SM3_" entry point); bytes 1.. are firmware memory at
// FAKEPHYS_BASE, where that config table points (so the entry point is at offset 0, and table
// addresses inside it are FAKEPHYS_BASE-relative physical addresses).
#include <stdint.h>
#include <stddef.h>
#include "fakephys.h"
#include "platform.h"
#include "efi.h"
#include "smbios.h"

static const efi_guid_t SMBIOS2_GUID = { 0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d} };
static const efi_guid_t SMBIOS3_GUID = { 0xf2fd1544, 0x9794, 0x4a2c, {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94} };

// Exported by smbios.c; they point into the (per-input) table copy, so reset them each input.
extern struct bios_info *dmi_bios_info;
extern struct system_info *dmi_system_info;
extern struct baseboard_info *dmi_baseboard_info;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    fakephys_map_bios_area();
    if (!fakephys_load(data + 1, size - 1)) return -1;
    fakephys_setup_efi((data[0] & 1) ? &SMBIOS3_GUID : &SMBIOS2_GUID, FAKEPHYS_BASE);

    dmi_bios_info = NULL; dmi_system_info = NULL; dmi_baseboard_info = NULL;
    dmi_memory_device = NULL; dmi_memory_device_type = 0; dmi_cpu_info = NULL;
    dmi_num_memory_devices = 0;
    screen_reset();

    if (smbios_init() == 0) {
        print_smbios_startup_info();
        print_dmi_memory_info();
    }
    fakephys_reset();
    return 0;
}
