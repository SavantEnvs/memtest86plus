// USB device enumeration: find_attached_usb_keyboards() on a root hub port — device descriptor
// fetch and validation (assign_usb_address), configuration/interface/endpoint descriptor walk,
// HID boot keyboard detection, hub descriptor and recursive hub port scan, mass-storage
// (Bulk-Only) interface detection and USB string descriptor fetch.
//
// The host controller is a fake whose methods answer for the attached devices: port resets,
// control requests without a data stage and endpoint configuration succeed; every control
// request with a data stage (get_data_request) and every device-to-host bulk transfer takes the
// requested number of bytes from the input, in order — fewer if the input runs out (a short
// transfer; the rest of the buffer keeps its previous contents) — and fails once it is empty.
// Input byte 0 (mod 5) is the speed the root port reports for the device.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "usbhcd.h"

static const uint8_t *in;
static size_t in_left;

static bool take(void *dst, size_t length)
{
    if (in_left == 0) return false;
    if (length > in_left) length = in_left;
    memcpy(dst, in, length);
    in += length;
    in_left -= length;
    return true;
}

static bool ok_port(usb_hcd_r hcd, int port) { (void)hcd; (void)port; return true; }
static bool ok_setup(usb_hcd_r hcd, const usb_ep_t *ep, const usb_setup_pkt_t *pkt)
{
    (void)hcd; (void)ep; (void)pkt;
    return true;
}
static bool data_request(usb_hcd_r hcd, const usb_ep_t *ep, const usb_setup_pkt_t *pkt,
                         const void *buffer, size_t length)
{
    (void)hcd; (void)ep; (void)pkt;
    return take((void *)buffer, length);
}
static bool ok_hub_ep(usb_hcd_r hcd, const usb_ep_t *ep, const usb_hub_t *hub)
{
    (void)hcd; (void)ep; (void)hub;
    return true;
}
static bool ok_kbd_ep(usb_hcd_r hcd, const usb_ep_t *ep, int idx) { (void)hcd; (void)ep; (void)idx; return true; }
static bool ok_bulk_ep(usb_hcd_r hcd, const usb_ep_t *ep, int id, bool out)
{
    (void)hcd; (void)ep; (void)id; (void)out;
    return true;
}
static bool bulk(usb_hcd_r hcd, const usb_ep_t *ep, void *buffer, size_t length, bool is_output)
{
    (void)hcd; (void)ep;
    return is_output ? true : take(buffer, length);
}
static bool ok_reset_bulk(usb_hcd_r hcd, const usb_ep_t *ep, int id) { (void)hcd; (void)ep; (void)id; return true; }

static const hcd_methods_t fake_methods = {
    .reset_root_hub_port = ok_port,
    .assign_address      = assign_usb_address,   // the generic upstream implementation
    .configure_hub_ep    = ok_hub_ep,
    .configure_kbd_ep    = ok_kbd_ep,
    .setup_request       = ok_setup,
    .get_data_request    = data_request,
    .configure_bulk_ep   = ok_bulk_ep,
    .bulk_transfer       = bulk,
    .reset_bulk_ep       = ok_reset_bulk,
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    usb_speed_t speed = (usb_speed_t)(data[0] % 5);
    in = data + 1;
    in_left = size - 1;

    static hcd_workspace_t ws;
    memset(&ws, 0, sizeof ws);
    usb_hcd_t hcd = { .methods = &fake_methods, .ws = &ws };
    usb_hub_t root;
    memset(&root, 0, sizeof root);
    root.num_ports = 1;

    // Enumeration state upstream keeps across scans; each input starts from a fresh scan.
    usb_forget_msd();
    usb_mass_storage_found = false;
    memset(usb_msd_name, 0, sizeof usb_msd_name);

    usb_ep_t keyboards[8];
    memset(keyboards, 0, sizeof keyboards);
    int num_devices = 0, num_keyboards = 0;
    find_attached_usb_keyboards(&hcd, &root, 1, speed, 1, &num_devices, keyboards, 8, &num_keyboards);
    return 0;
}
