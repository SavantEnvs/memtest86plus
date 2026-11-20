#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "screen.h"
#include "rtc.h"
#include "unistd.h"

char screen_text[SCREEN_ROWS][SCREEN_COLS + 1];

void screen_reset(void) { memset(screen_text, 0, sizeof screen_text); }

bool screen_contains(const char *needle)
{
    for (int r = 0; r < SCREEN_ROWS; r++) {
        char line[SCREEN_COLS + 1];
        for (int c = 0; c < SCREEN_COLS; c++) line[c] = screen_text[r][c] ? screen_text[r][c] : ' ';
        line[SCREEN_COLS] = 0;
        if (strstr(line, needle)) return true;
    }
    return false;
}

void print_char(int row, int col, char ch)
{
    if (row >= 0 && row < SCREEN_ROWS && col >= 0 && col < SCREEN_COLS) screen_text[row][col] = ch;
}
void set_foreground_colour(screen_colour_t colour) { (void)colour; }
void set_background_colour(screen_colour_t colour) { (void)colour; }
screen_palette_t palette = { BLACK, WHITE, RED, WHITE, WHITE, BLUE, BLUE };

static const uint8_t *spd_data;
static size_t spd_len;
void spd_set(const uint8_t *data, size_t len) { spd_data = data; spd_len = len; }
uint8_t get_spd(uint8_t slot_idx, uint16_t spd_adr)
{
    if (slot_idx != 0 || spd_adr >= spd_len) return 0xFF;
    return spd_data[spd_adr];
}

bool rtc_get_time(rtc_time_t *dt)
{
    // Fixed time so each run depends only on its input.
    dt->year = 2024; dt->month = 1; dt->day = 2; dt->hour = 3; dt->min = 4; dt->sec = 5;
    return true;
}

void usleep(unsigned int usec) { (void)usec; }
