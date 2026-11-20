// Memory module SPD EEPROM contents: parse_spd() (DDR5/DDR4/DDR3/DDR2/DDR/SDRAM/RDRAM decoders,
// XMP/EXPO profiles, CRC, SKU, JEP106 lookup) then print_spdi() for a valid module.
//
// Input: the SPD EEPROM bytes of slot 0 (get_spd(0, adr) = input[adr]; 0xFF past the end, as an
// unacknowledged SMBus read).
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "platform.h"
#include "spd.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    spd_info info;
    spd_set(data, size);
    screen_reset();
    parse_spd(&info, 0);
    if (info.isValid) print_spdi(info, 0);
    spd_set(NULL, 0);
    return 0;
}
