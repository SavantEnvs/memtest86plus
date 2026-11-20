// ACPI tables from firmware: acpi_init() — RSDP, RSDT/XSDT walk and checksums, FADT parsing,
// discovery of MADT/HPET/SRAT/MCFG — then the consumers of the tables it located: the MADT
// (CPU list) and SRAT (NUMA ranges) parsers of system/smp.c (smp_tables.c).
//
// Input: byte 0 bit 0 picks the EFI config table GUID (0 = ACPI 2.0 RSDP, 1 = ACPI 1.0 RSDP);
// bytes 1.. are firmware memory at FAKEPHYS_BASE, where the RSDP pointer points (RSDP at offset
// 0; RSDT/XSDT/table pointers are FAKEPHYS_BASE-relative physical addresses).
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "fakephys.h"
#include "efi.h"
#include "acpi.h"

void mayhem_smp_parse_acpi_tables(void);   // smp_tables.c

static const efi_guid_t ACPI1_GUID = { 0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d} };
static const efi_guid_t ACPI2_GUID = { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81} };

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    fakephys_map_bios_area();
    if (!fakephys_load(data + 1, size - 1)) return -1;
    fakephys_setup_efi((data[0] & 1) ? &ACPI1_GUID : &ACPI2_GUID, FAKEPHYS_BASE);
    memset(&acpi_config, 0, sizeof acpi_config);

    acpi_init();
    mayhem_smp_parse_acpi_tables();

    fakephys_reset();
    return 0;
}
