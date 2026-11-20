// What system/usbhcd.c references beyond device enumeration: PCI host controller discovery and
// the per-controller (EHCI/OHCI/UHCI/xHCI) drivers, which program real controller registers.
// fuzz_usbenum drives enumeration through its own fake host controller, so none of these is
// reached; PCI reads return "no device" (all ones) and the drivers fail.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "usbhcd.h"
#include "ehci.h"
#include "ohci.h"
#include "uhci.h"
#include "xhci.h"
#include "pci.h"
#include "hwquirks.h"
#include "keyboard.h"
#include "screen.h"

quirk_t quirk;

uint8_t  pci_config_read8(int bus, int dev, int func, int reg)  { (void)bus; (void)dev; (void)func; (void)reg; return 0xFF; }
uint16_t pci_config_read16(int bus, int dev, int func, int reg) { (void)bus; (void)dev; (void)func; (void)reg; return 0xFFFF; }
uint32_t pci_config_read32(int bus, int dev, int func, int reg) { (void)bus; (void)dev; (void)func; (void)reg; return 0xFFFFFFFF; }
void pci_config_write16(int bus, int dev, int func, int reg, uint16_t v) { (void)bus; (void)dev; (void)func; (void)reg; (void)v; }
void pci_config_write32(int bus, int dev, int func, int reg, uint32_t v) { (void)bus; (void)dev; (void)func; (void)reg; (void)v; }
uintptr_t pci_alloc_mmio(int bus, int dev, int func, int bar_reg, uintptr_t size)
{
    (void)bus; (void)dev; (void)func; (void)bar_reg; (void)size;
    return 0;
}

bool ehci_reset(int bus, int dev, int func, uintptr_t base) { (void)bus; (void)dev; (void)func; (void)base; return false; }
bool ehci_probe(uintptr_t base, usb_hcd_t *hcd) { (void)base; (void)hcd; return false; }
bool ohci_reset(uintptr_t base) { (void)base; return false; }
bool ohci_probe(uintptr_t base, usb_hcd_t *hcd) { (void)base; (void)hcd; return false; }
bool uhci_reset(int bus, int dev, int func, uint16_t io) { (void)bus; (void)dev; (void)func; (void)io; return false; }
bool uhci_probe(uint16_t io, usb_hcd_t *hcd) { (void)io; (void)hcd; return false; }
bool xhci_reset(uintptr_t base) { (void)base; return false; }
bool xhci_probe(uintptr_t base, usb_hcd_t *hcd) { (void)base; (void)hcd; return false; }

char get_key(void) { return 0; }
void clear_screen(void) {}
void scroll_screen_region(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
