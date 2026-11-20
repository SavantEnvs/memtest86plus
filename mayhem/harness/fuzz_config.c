// Bootloader-supplied kernel command line: config_init() -> parse_command_line() ->
// parse_option() (console=ttyS/MMIO serial parameters, tests= test list, smp/nosmp, keyboard=,
// usbdebug/usbinit=, powersave=, cpuseqmode=, efidebug, nopause, ...).
//
// Input: the command line bytes. The bootloader hands memtest86+ a pointer and a size in
// boot_params (cmd_line_ptr / cmd_line_size); here the command line sits at FAKEPHYS_BASE and
// cmd_line_size is the input size.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "fakephys.h"
#include "bootparams.h"
#include "pmem.h"
#include "config.h"

void config_stubs_reset(void);

// The option globals the parser writes, restored to their start-up values before each input
// (some options read others, e.g. "log" copies enable_tty).
#define SAVED_GLOBALS(X) \
    X(enable_tty) X(enable_tty_log) X(log_max_passes) X(tty_new_line) X(enable_ecc_polling) \
    X(dark_mode) X(enable_bench) X(enable_big_status) X(enable_mch_read) X(pause_at_start) \
    X(smp_enabled) X(enable_temp_cpu) X(enable_temp_ram) X(enable_sm) X(enable_spd_crc) \
    X(enable_numa) X(enable_trace) X(exclude_ecores) X(exclude_smtcores) X(tty_address) \
    X(tty_baud_rate) X(tty_update_period) X(tty_mmio_ref_clk) X(tty_mmio_stride) X(tty_pl011)
#define DECLARE_SAVED(g) static __typeof__(g) saved_##g;
SAVED_GLOBALS(DECLARE_SAVED)
#define SAVE(g) saved_##g = g;
#define RESTORE(g) g = saved_##g;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // cmd_line_size is a 32-bit boot_params field; 0 means "unknown, assume 255" upstream.
    if (size == 0 || size > 0xFFFF) return -1;
    if (!fakephys_load(data, size)) return -1;
    boot_params_t *bp = fakephys_boot_params();
    memset(bp, 0, sizeof *bp);
    bp->cmd_line_ptr = (uint32_t)FAKEPHYS_BASE;
    bp->cmd_line_size = (uint32_t)size;

    // One usable RAM range (what pmem_init() would leave) — config_init() reads the last entry.
    pm_map[0].start = 0x100;
    pm_map[0].end = 0x80000;
    pm_map_size = 1;
    static bool saved;
    if (!saved) { SAVED_GLOBALS(SAVE) saved = true; }
    SAVED_GLOBALS(RESTORE)
    config_stubs_reset();

    config_init();

    fakephys_reset();
    return 0;
}
