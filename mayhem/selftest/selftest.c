// Known-answer tests for the memtest86+ firmware/device parsers, built with normal flags and run
// by mayhem/test.sh. Each test feeds a committed, well-formed input (mayhem/<target>/testsuite/,
// produced by mayhem/tools/make_seeds.py) through the same host platform model the fuzz harnesses
// use, and asserts what upstream parsed out of it. Prints one "PASS <name>" / "FAIL <name>" line
// per check.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "fakephys.h"
#include "platform.h"
#include "efi.h"
#include "bootparams.h"
#include "smbios.h"
#include "acpi.h"
#include "pmem.h"
#include "spd.h"
#include "fat32.h"
#include "config.h"
#include "keyboard.h"
#include "usbhcd.h"
#include "tests.h"

void config_stubs_reset(void);

int puts(const char *s);
int strcmp(const char *a, const char *b);
int fflush(void *f);
typedef struct _IO_FILE FILE;
FILE *fopen(const char *path, const char *mode);
size_t fread(void *p, size_t s, size_t n, FILE *f);
int fclose(FILE *f);
int snprintf(char *buf, size_t n, const char *fmt, ...);

extern struct bios_info *dmi_bios_info;
extern struct system_info *dmi_system_info;
extern struct baseboard_info *dmi_baseboard_info;

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

static bool streq(const char *a, const char *b) { return a && b && strcmp(a, b) == 0; }

static const efi_guid_t SMBIOS2_GUID = { 0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d} };
static const efi_guid_t SMBIOS3_GUID = { 0xf2fd1544, 0x9794, 0x4a2c, {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94} };
static const efi_guid_t ACPI1_GUID = { 0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d} };
static const efi_guid_t ACPI2_GUID = { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81} };

static void test_smbios(const char *file, const char *tag)
{
    static uint8_t buf[1 << 16];
    size_t n = load(file, buf, sizeof buf);
    if (n < 1) return;
    char name[128];
    fakephys_map_bios_area();
    fakephys_load(buf + 1, n - 1);
    fakephys_setup_efi((buf[0] & 1) ? &SMBIOS3_GUID : &SMBIOS2_GUID, FAKEPHYS_BASE);
    dmi_bios_info = NULL; dmi_system_info = NULL; dmi_baseboard_info = NULL;
    dmi_memory_device = NULL; dmi_memory_device_type = 0; dmi_cpu_info = NULL; dmi_num_memory_devices = 0;
    screen_reset();

    snprintf(name, sizeof name, "smbios_%s_init", tag);
    check(name, smbios_init() == 0);
    print_smbios_startup_info();
    print_dmi_memory_info();

    const char *man = NULL, *prod = NULL, *sser = NULL, *bser = NULL, *ver = NULL, *date = NULL;
    get_smbios_board_info(&man, &prod);
    get_smbios_serial_info(&sser, &bser);
    get_smbios_bios_info(&ver, &date);
    snprintf(name, sizeof name, "smbios_%s_board", tag);
    check(name, streq(man, "ACME") && streq(prod, "Board1"));
    snprintf(name, sizeof name, "smbios_%s_serials", tag);
    check(name, streq(sser, "SN123") && streq(bser, "BSN42"));
    snprintf(name, sizeof name, "smbios_%s_bios", tag);
    check(name, streq(ver, "1.23") && streq(date, "01/02/2024"));
    snprintf(name, sizeof name, "smbios_%s_memdev", tag);
    check(name, dmi_num_memory_devices == 1 && dmi_memory_device_type == DMI_DDR4
                && dmi_memory_devices[0]->speed == 3200);
    snprintf(name, sizeof name, "smbios_%s_screen", tag);
    check(name, screen_contains("ACME Board1") && screen_contains("M393A1K43BB1"));
    fakephys_reset();
}

static void test_acpi(const char *file, const char *tag)
{
    static uint8_t buf[1 << 16];
    size_t n = load(file, buf, sizeof buf);
    if (n < 1) return;
    char name[128];
    fakephys_load(buf + 1, n - 1);
    fakephys_setup_efi((buf[0] & 1) ? &ACPI1_GUID : &ACPI2_GUID, FAKEPHYS_BASE);
    memset(&acpi_config, 0, sizeof acpi_config);
    acpi_init();
    snprintf(name, sizeof name, "acpi_%s_rsdp", tag);
    check(name, acpi_config.rsdp_addr == FAKEPHYS_BASE);
    snprintf(name, sizeof name, "acpi_%s_tables", tag);
    check(name, acpi_config.fadt_addr == FAKEPHYS_BASE + 0x100 && acpi_config.madt_addr == FAKEPHYS_BASE + 0x200
                && acpi_config.hpet_addr == FAKEPHYS_BASE + 0x300 && acpi_config.srat_addr == 0
                && acpi_config.mcfg_addr == 0);
    snprintf(name, sizeof name, "acpi_%s_fadt", tag);
    check(name, acpi_config.ver_maj == 6 && acpi_config.ver_min == 4 && acpi_config.pm_addr == 0x1008
                && acpi_config.pm_is_io);
    fakephys_reset();
}

static void test_pmem(void)
{
    static uint8_t buf[4096];
    size_t n = load("mayhem/fuzz_pmem/testsuite/e820.bin", buf, sizeof buf);
    if (n < 1) return;
    boot_params_t *bp = fakephys_boot_params();
    memset(bp, 0, sizeof *bp);
    bp->e820_entries = buf[0];
    memcpy(bp->e820_map, buf + 1, n - 1);
    pmem_init();
    char line[128];
    for (int i = 0; i < pm_map_size; i++) {
        snprintf(line, sizeof line, "  pm_map[%d] = %#lx-%#lx", i, (unsigned long)pm_map[i].start, (unsigned long)pm_map[i].end);
        puts(line);
    }
    // Usable RAM 0-0x9F000, 1 MiB-2 GiB (the overlapping 2 MiB entry merged in), 4-5 GiB, in pages.
    check("pmem_map", pm_map_size == 3
                      && pm_map[0].start == 0x0 && pm_map[0].end == 0x9F
                      && pm_map[1].start == 0x100 && pm_map[1].end == 0x7FF00
                      && pm_map[2].start == 0x100000 && pm_map[2].end == 0x140000);
    check("pmem_pages", num_pm_pages == 0x9F + (0x7FF00 - 0x100) + 0x40000);
}

static void test_spd(void)
{
    static uint8_t buf[1024];
    size_t n = load("mayhem/fuzz_spd/testsuite/ddr4.bin", buf, sizeof buf);
    if (n < 1) return;
    spd_info info;
    spd_set(buf, n);
    screen_reset();
    parse_spd(&info, 0);
    check("spd_ddr4_valid", info.isValid && strcmp(info.type, "DDR4") == 0);
    check("spd_ddr4_decode", info.module_size == 8192 && info.hasECC && info.freq == 3200
                             && strcmp(info.sku, "M393A1K43BB1-CTD") == 0);
    check("spd_ddr4_jedec", streq(get_jep106_name(info.jedec_code), "Samsung"));
    print_spdi(info, 0);
    check("spd_ddr4_screen", screen_contains("Samsung") && screen_contains("M393A1K43BB1-CTD"));
    uint8_t blank[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    spd_set(blank, sizeof blank);
    parse_spd(&info, 0);
    check("spd_empty_slot", !info.isValid);
    spd_set(NULL, 0);
}

static int count_name(const uint8_t *disk, size_t len, const char *pat)
{
    int c = 0;
    size_t pl = strlen(pat);
    for (size_t i = 0; i + pl <= len; i++) if (memcmp(disk + i, pat, pl) == 0) c++;
    return c;
}

static void test_fat(const char *file, const char *tag)
{
    static uint8_t img[1 << 16];
    size_t n = load(file, img, sizeof img);
    if (n < 512) return;
    char name[128];
    static const char report[] = "Memtest86+ selftest report payload\r\n";
    disk_set(img, n - n % 512);
    usb_msd_t msd;
    memset(&msd, 0, sizeof msd);
    msd.block_size = 512;
    msd.block_count = n / 512;
    uint8_t sector[512];
    fat32_fs_t fs;
    memset(&fs, 0, sizeof fs);
    snprintf(name, sizeof name, "fat_%s_mount", tag);
    check(name, fat32_mount(&fs, &msd, sector) && fs.fat_type == 16);
    char fname[13] = { 0 };
    snprintf(name, sizeof name, "fat_%s_next_name", tag);
    check(name, fat32_next_filename(&fs, fname) && strcmp(fname, "MT86P_02TXT") == 0);
    snprintf(name, sizeof name, "fat_%s_write", tag);
    check(name, fat32_write_file(&fs, fname, report, sizeof report - 1)
                && count_name(img, n, "MT86P_02TXT") == 1 && count_name(img, n, "selftest report payload") == 1);
    // Remount: the new file is seen, so the next free name moves on.
    memset(&fs, 0, sizeof fs);
    char fname2[13] = { 0 };
    snprintf(name, sizeof name, "fat_%s_remount", tag);
    check(name, fat32_mount(&fs, &msd, sector) && fat32_next_filename(&fs, fname2)
                && strcmp(fname2, "MT86P_03TXT") == 0);
    disk_set(NULL, 0);
}

static void test_config(void)
{
    static uint8_t buf[4096];
    size_t n = load("mayhem/fuzz_config/testsuite/cmdline.txt", buf, sizeof buf);
    if (n < 1) return;
    fakephys_load(buf, n);
    boot_params_t *bp = fakephys_boot_params();
    memset(bp, 0, sizeof *bp);
    bp->cmd_line_ptr = (uint32_t)FAKEPHYS_BASE;
    bp->cmd_line_size = (uint32_t)n;
    pm_map[0].start = 0x100; pm_map[0].end = 0x80000; pm_map_size = 1;
    config_stubs_reset();
    config_init();
    char line[256];
    snprintf(line, sizeof line, "  tty=%d addr=%#lx baud=%d cpu_mode=%d err=%d kbd=%d pause=%d dark=%d max=%d usb=%d t0=%d t1=%d t3=%d",
             enable_tty, (unsigned long)tty_address, tty_baud_rate, cpu_mode, error_mode, keyboard_types,
             pause_at_start, dark_mode, log_max_passes, usb_init_options, test_list[0].enabled,
             test_list[1].enabled, test_list[3].enabled);
    puts(line);
    check("config_console", enable_tty && tty_address == 0x2F8 && tty_baud_rate == 9600);
    check("config_modes", cpu_mode == SEQ && error_mode == ERROR_MODE_BADRAM && keyboard_types == KT_USB);
    check("config_flags", !pause_at_start && dark_mode && log_max_passes == 7 && (usb_init_options & USB_IGNORE_EHCI));
    check("config_tests", !test_list[0].enabled && test_list[1].enabled && !test_list[2].enabled && test_list[3].enabled);
    fakephys_reset();
}

int main(int argc, char **argv)
{
    if (argc > 1) dir = argv[1];
    test_smbios("mayhem/fuzz_smbios/testsuite/smbios_v3.bin", "v3");
    test_smbios("mayhem/fuzz_smbios/testsuite/smbios_v2.bin", "v2");
    test_acpi("mayhem/fuzz_acpi/testsuite/acpi_xsdt.bin", "xsdt");
    test_acpi("mayhem/fuzz_acpi/testsuite/acpi_rsdt.bin", "rsdt");
    test_pmem();
    test_spd();
    test_fat("mayhem/fuzz_fat32/testsuite/fat16.img", "plain");
    test_fat("mayhem/fuzz_fat32/testsuite/fat16_mbr.img", "mbr");
    test_config();
    char line[64];
    snprintf(line, sizeof line, "SUMMARY passed=%d failed=%d", passed, failed);
    puts(line);
    return failed != 0;
}
