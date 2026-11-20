// Stand-ins for what app/config.c references beyond the command-line parser: the interactive
// config menus (keyboard, screen save/restore), SMP/CPU topology, USB and report state. The
// parser reached from config_init() only writes option globals and reads test_list.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cpuid.h"
#include "keyboard.h"
#include "smp.h"
#include "usbhcd.h"
#include "tests.h"
#include "test.h"
#include "display.h"
#include "read.h"
#include "reports.h"
#include "hwctrl.h"
#include "serial.h"
#include "unistd.h"

cpuid_info_t        cpuid_info;
keyboard_types_t    keyboard_types;
int                 num_available_cpus = 1;
usb_init_options_t  usb_init_options;
bool                usb_mass_storage_found;
char                usb_msd_name[64];
test_pattern_t      test_list[NUM_TEST_PATTERNS];
int                 scroll_message_row;
bool                bail;
bool                restart;

bool smp_is_smt_sibling(int cpu_num) { (void)cpu_num; return false; }
bool usb_hcd_available(void) { return false; }
void display_cpu_topology(void) {}
void toggle_scroll_lock(void) {}
char get_key(void) { return 0; }
uintptr_t read_value(int x, int y, int field_width, int shift) { (void)x; (void)y; (void)field_width; (void)shift; return 0; }
void save_results_to_usb(void) {}
void reboot(void) {}
void tty_send_region(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
void clear_screen_region(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
void save_screen_region(int a, int b, int c, int d, uint16_t buffer[]) { (void)a; (void)b; (void)c; (void)d; (void)buffer; }
void restore_screen_region(int a, int b, int c, int d, const uint16_t buffer[]) { (void)a; (void)b; (void)c; (void)d; (void)buffer; }

void config_stubs_reset(void)
{
    for (int i = 0; i < NUM_TEST_PATTERNS; i++) {
        test_list[i].enabled = true;
        test_list[i].cpu_mode = 0;
    }
    usb_init_options = 0;
    keyboard_types = 0;
}
