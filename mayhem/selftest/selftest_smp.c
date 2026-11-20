// Known-answer tests for the ACPI MADT/SRAT consumers in system/smp.c, linked like fuzz_acpi
// (mayhem/harness/smp_tables.c), built with normal flags and run by mayhem/test.sh. The committed
// seeds acpi_numa_{xsdt,rsdt}.bin (mayhem/tools/make_seeds.py) describe 3 CPUs (APIC IDs 0, 2, 4)
// and 2 NUMA domains: [0, 2 GiB) with APIC 0, [2 GiB, 4 GiB) with APICs 2 and 4. Prints one
// "PASS <name>" / "FAIL <name>" line per check.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "fakephys.h"
#include "efi.h"
#include "acpi.h"
#include "smp.h"

void mayhem_smp_parse_acpi_tables(void);

int puts(const char *s);
typedef struct _IO_FILE FILE;
FILE *fopen(const char *path, const char *mode);
size_t fread(void *p, size_t s, size_t n, FILE *f);
int fclose(FILE *f);
int snprintf(char *buf, size_t n, const char *fmt, ...);

static int passed, failed;
static const char *dir = ".";

static void check(const char *name, bool ok)
{
    char line[256];
    snprintf(line, sizeof line, "%s %s", ok ? "PASS" : "FAIL", name);
    puts(line);
    if (ok) passed++; else failed++;
}

static size_t load(const char *rel, uint8_t *buf, size_t cap)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, rel);
    FILE *f = fopen(path, "rb");
    if (!f) { check(rel, false); return 0; }
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

static const efi_guid_t ACPI1_GUID = { 0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d} };
static const efi_guid_t ACPI2_GUID = { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81} };

static void test_numa(const char *file, const char *tag)
{
    static uint8_t buf[1 << 16];
    size_t n = load(file, buf, sizeof buf);
    if (n < 1) return;
    char name[128];
    fakephys_load(buf + 1, n - 1);
    fakephys_setup_efi((buf[0] & 1) ? &ACPI1_GUID : &ACPI2_GUID, FAKEPHYS_BASE);
    memset(&acpi_config, 0, sizeof acpi_config);
    acpi_init();
    snprintf(name, sizeof name, "smp_%s_tables", tag);
    check(name, acpi_config.madt_addr == FAKEPHYS_BASE + 0x400 && acpi_config.srat_addr == FAKEPHYS_BASE + 0x500);

    mayhem_smp_parse_acpi_tables();
    snprintf(name, sizeof name, "smp_%s_cpus", tag);
    check(name, num_available_cpus == 3 && smp_get_proximity_domain_idx(0) == 0
                && smp_get_proximity_domain_idx(1) == 1 && smp_get_proximity_domain_idx(2) == 1);

    uint32_t dom = 99;
    uint64_t s = 0, e = 0;
    int r = smp_narrow_to_proximity_domain(0x7FFF0000ull, 0x80010000ull, &dom, &s, &e);
    snprintf(name, sizeof name, "smp_%s_numa", tag);
    check(name, num_proximity_domains == 2 && !smp_topology_too_large
                && smp_domain_has_memory(0) && smp_domain_has_memory(1)
                && r == 1 && dom == 0 && s == 0x7FFF0000ull && e == 0x80000000ull);
    fakephys_reset();
}

int main(int argc, char **argv)
{
    if (argc > 1) dir = argv[1];
    test_numa("mayhem/fuzz_acpi/testsuite/acpi_numa_xsdt.bin", "xsdt");
    test_numa("mayhem/fuzz_acpi/testsuite/acpi_numa_rsdt.bin", "rsdt");
    char line[64];
    snprintf(line, sizeof line, "SUMMARY passed=%d failed=%d", passed, failed);
    puts(line);
    return failed != 0;
}
