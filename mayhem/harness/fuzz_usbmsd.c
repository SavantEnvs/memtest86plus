// A USB mass-storage device's replies: msd_init() (Bulk-Only Transport CBW/CSW handling,
// TEST UNIT READY / REQUEST SENSE / READ CAPACITY(10/16), block size validation), then what
// memtest86+ does with the stick: fat32_mount() and fat32_next_filename() reading sectors
// through the real msd_read_sectors().
//
// The host controller is a fake whose methods answer for the device: control requests and
// host-to-device bulk transfers succeed; every device-to-host bulk transfer (data or CSW) takes
// its bytes from the input, in order, and fails once the input is exhausted. A real device
// echoes the CBW tag in its CSW, so the CSW tag the driver sees is the last CBW tag XOR the
// 4 input bytes at that position (zeros = a well-behaved device, anything else = a mismatch).
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "usbhcd.h"
#include "usbmsd.h"
#include "fat32.h"

static const uint8_t *in;
static size_t in_left;
static uint32_t last_tag;

static bool fake_setup_request(usb_hcd_r hcd, const usb_ep_t *ep, const usb_setup_pkt_t *pkt)
{
    (void)hcd; (void)ep; (void)pkt;
    return true;
}

static bool fake_reset_bulk_ep(usb_hcd_r hcd, const usb_ep_t *ep, int ep_id)
{
    (void)hcd; (void)ep; (void)ep_id;
    return true;
}

static bool fake_bulk_transfer(usb_hcd_r hcd, const usb_ep_t *ep, void *buffer, size_t length, bool is_output)
{
    (void)hcd; (void)ep;
    if (is_output) {
        if (length == 31) memcpy(&last_tag, (uint8_t *)buffer + 4, 4);   // CBW tag
        return true;
    }
    if (length > in_left) return false;
    memcpy(buffer, in, length);
    in += length;
    in_left -= length;
    if (length == 13) {                                                 // CSW
        uint32_t tag;
        memcpy(&tag, (uint8_t *)buffer + 4, 4);
        tag ^= last_tag;
        memcpy((uint8_t *)buffer + 4, &tag, 4);
    }
    return true;
}

static const hcd_methods_t fake_methods = {
    .setup_request = fake_setup_request,
    .bulk_transfer = fake_bulk_transfer,
    .reset_bulk_ep = fake_reset_bulk_ep,
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    in = data;
    in_left = size;
    last_tag = 0;

    usb_hcd_t hcd;
    memset(&hcd, 0, sizeof hcd);
    hcd.methods = &fake_methods;
    usb_msd_t msd;
    memset(&msd, 0, sizeof msd);
    msd.hcd = &hcd;

    if (msd_init(&msd)) {
        uint8_t *buf = malloc(msd.block_size);
        fat32_fs_t fs;
        memset(&fs, 0, sizeof fs);
        if (fat32_mount(&fs, &msd, buf)) {
            char name[13];
            fat32_next_filename(&fs, name);
        }
        free(buf);
    }
    return 0;
}
