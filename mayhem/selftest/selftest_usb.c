// Known-answer tests for USB device enumeration (system/usbhcd.c), run by mayhem/test.sh after
// selftest. Drives the fuzz_usbenum harness entry point (built with normal flags) over committed
// device replies and checks what upstream reports on screen and in its globals.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "platform.h"
#include "usbhcd.h"

int puts(const char *s);
int strcmp(const char *a, const char *b);
typedef struct _IO_FILE FILE;
FILE *fopen(const char *path, const char *mode);
size_t fread(void *p, size_t s, size_t n, FILE *f);
int fclose(FILE *f);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int failed;
static const char *dir = ".";

static void check(const char *name, bool ok)
{
    char line[256];
    snprintf(line, sizeof line, "%s %s", ok ? "PASS" : "FAIL", name);
    puts(line);
    if (!ok) failed++;
}

static bool run(const char *rel)
{
    static uint8_t buf[1 << 16];
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, rel);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    screen_reset();
    LLVMFuzzerTestOneInput(buf, n);
    return true;
}

int main(int argc, char **argv)
{
    if (argc > 1) dir = argv[1];
    bool ok = run("mayhem/fuzz_usbenum/testsuite/keyboard.bin");
    check("usb_keyboard_found", ok && screen_contains("Keyboard 1234:5678 found on port 1 interface 0 endpoint 1")
                                && !usb_mass_storage_found);
    ok = run("mayhem/fuzz_usbenum/testsuite/msd.bin");
    check("usb_msd_found", ok && usb_mass_storage_found && strcmp(usb_msd_name, "USB Stick") == 0
                           && screen_contains("USB drive found on port 1 (USB Stick)"));
    return failed != 0;
}
