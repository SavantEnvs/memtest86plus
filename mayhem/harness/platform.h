// Host stand-ins for the memtest86+ platform layer (screen, SMBus SPD, USB MSD sectors, RTC),
// shared by every harness. The upstream parsers are linked unmodified on top of these.
#ifndef MAYHEM_PLATFORM_H
#define MAYHEM_PLATFORM_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_ROWS 64
#define SCREEN_COLS 256
// Text the upstream code "printed" (print_char) — row-major, NUL where nothing was drawn.
extern char screen_text[SCREEN_ROWS][SCREEN_COLS + 1];
void screen_reset(void);
// True if `needle` appears on any screen row.
bool screen_contains(const char *needle);

// SPD EEPROM of slot 0: get_spd(0, adr) returns spd_data[adr] (0xFF past spd_len, like an
// SMBus read that is not acknowledged).
void spd_set(const uint8_t *data, size_t len);

// USB mass-storage disk: msd_read_sectors/msd_write_sectors work on this buffer (512-byte
// sectors); a sector range past the end fails like a device error.
void disk_set(uint8_t *data, size_t len);
#endif
