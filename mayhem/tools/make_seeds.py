#!/usr/bin/env python3
"""Regenerate the committed seed inputs under mayhem/<target>/testsuite/ (run from the repo root).

Each file is a well-formed firmware/device structure in the layout the matching harness in
mayhem/harness/ expects; mayhem/selftest/selftest.c asserts the values upstream parses out of them.
"""
import os
import struct

BASE = 0x10000000


def put(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def fix_sum(buf, off, length, at):
    """Set buf[at] so the bytes buf[off:off+length] sum to 0 mod 256."""
    buf[at] = 0
    buf[at] = (-sum(buf[off:off + length])) & 0xFF


def dmi_struct(stype, handle, body, strings):
    hdr = struct.pack("<BBH", stype, 4 + len(body), handle) + body
    tail = b"".join(s.encode() + b"\0" for s in strings) + b"\0"
    if not strings:
        tail = b"\0\0"
    return hdr + tail


def dmi_table():
    t = b""
    t += dmi_struct(0, 0, struct.pack("<BBHB", 1, 2, 0xE800, 3) + bytes(9),
                    ["BiosVendor", "1.23", "01/02/2024"])
    t += dmi_struct(1, 1, struct.pack("<BBBB", 1, 2, 3, 4) + bytes(range(16)) + b"\x06",
                    ["SysCo", "SysBox", "v1", "SN123"])
    t += dmi_struct(2, 2, struct.pack("<BBBB", 1, 2, 3, 4), ["ACME", "Board1", "rev", "BSN42"])
    md = struct.pack("<HHHHHBBBBBHHBBBBBIHHHH",
                     0, 0xFFFE, 72, 64, 8192, 9, 0, 3, 0, 0x1A, 0x80, 3200,
                     1, 0, 0, 2, 1, 0, 3200, 1200, 1200, 1200)
    t += dmi_struct(17, 3, md, ["Samsung", "M393A1K43BB1", "DIMM0"])
    t += dmi_struct(127, 4, b"", [])
    return t


def smbios_v3():
    table = dmi_table()
    eps = bytearray(struct.pack("<5sBBBBBBBIQ", b"_SM3_", 0, 0x18, 3, 2, 0, 1, 0, len(table), BASE + 0x20))
    fix_sum(eps, 0, 0x18, 5)
    mem = bytes(eps) + bytes(0x20 - len(eps)) + table
    return b"\x01" + mem


def smbios_v2():
    table = dmi_table()
    nstructs = 5
    eps = bytearray(struct.pack("<4sbBBBHB5s5sbHIHB", b"_SM_", 0, 0x1F, 2, 8, 0x100, 0, bytes(5),
                                b"_DMI_", 0, len(table), BASE + 0x20, nstructs, 0x28))
    fix_sum(eps, 16, 15, 21)
    fix_sum(eps, 0, 0x1F, 4)
    mem = bytes(eps) + bytes(0x20 - len(eps)) + table
    return b"\x00" + mem


def sdt(sig, length, rev, body=b""):
    t = bytearray(struct.pack("<4sIBB6s8s4s4s4s", sig, length, rev, 0, b"MAYHEM", b"FUZZTEST",
                              b"\1\0\0\0", b"MHM ", b"\1\0\0\0"))
    t += body
    t += bytes(length - len(t))
    fix_sum(t, 0, length, 9)
    return bytes(t)


def acpi_tables(rev2):
    mem = bytearray(0x400)
    fadt = bytearray(sdt(b"FACP", 244, 6))
    struct.pack_into("<I", fadt, 76, 0x408)
    fadt[131] = 0x04
    struct.pack_into("<BBBBQ", fadt, 208, 1, 32, 0, 3, 0x1008)
    fix_sum(fadt, 0, 244, 9)
    mem[0x100:0x100 + 244] = fadt
    mem[0x200:0x200 + 44] = sdt(b"APIC", 44, 4)
    mem[0x300:0x300 + 56] = sdt(b"HPET", 56, 1)
    entries = [BASE + 0x100, BASE + 0x200, BASE + 0x300]
    if rev2:
        x = sdt(b"XSDT", 36 + 8 * 3, 1, struct.pack("<3Q", *entries))
    else:
        x = sdt(b"RSDT", 36 + 4 * 3, 1, struct.pack("<3I", *entries))
    mem[0x40:0x40 + len(x)] = x
    rsdp = bytearray(struct.pack("<8sB6sBIIQB3s", b"RSD PTR ", 0, b"MAYHEM", 2 if rev2 else 0,
                                 0 if rev2 else BASE + 0x40, 36, BASE + 0x40 if rev2 else 0, 0, bytes(3)))
    fix_sum(rsdp, 0, 20, 8)
    fix_sum(rsdp, 0, 36, 32)
    mem[0:36] = rsdp
    return (b"\x00" if rev2 else b"\x01") + bytes(mem)


def acpi_numa_tables(rev2):
    """acpi_tables() plus a populated MADT (3 CPUs, local APIC inside the image) and an SRAT
    (2 proximity domains), for the system/smp.c parsers fuzz_acpi runs after acpi_init()."""
    mem = bytearray(acpi_tables(rev2)[1:]) + bytearray(0x400)
    madt = struct.pack("<II", BASE + 0x700, 1)
    madt += struct.pack("<BBBBI", 0, 8, 0, 0, 1)                # processor, APIC ID 0 (BSP)
    madt += struct.pack("<BBBBI", 0, 8, 1, 2, 1)                # processor, APIC ID 2
    madt += struct.pack("<BBHIII", 9, 16, 0, 4, 1, 2)           # x2APIC processor, APIC ID 4
    madt = sdt(b"APIC", 36 + len(madt), 4, madt)
    mem[0x400:0x400 + len(madt)] = madt
    srat = struct.pack("<IQ", 1, 0)
    srat += struct.pack("<BBBBIII", 0, 16, 0, 0, 1, 0, 0)       # APIC 0 -> domain 0
    srat += struct.pack("<BBBBIII", 0, 16, 1, 2, 1, 0, 0)       # APIC 2 -> domain 1
    srat += struct.pack("<BBHIIIII", 2, 24, 0, 1, 4, 1, 0, 0)   # x2APIC 4 -> domain 1
    srat += struct.pack("<BBIHQQIIQ", 1, 40, 0, 0, 0, 0x80000000, 0, 1, 0)
    srat += struct.pack("<BBIHQQIIQ", 1, 40, 1, 0, 0x80000000, 0x80000000, 0, 1, 0)
    srat = sdt(b"SRAT", 36 + len(srat), 3, srat)
    mem[0x500:0x500 + len(srat)] = srat
    entries = [BASE + 0x100, BASE + 0x400, BASE + 0x300, BASE + 0x500]
    if rev2:
        x = sdt(b"XSDT", 36 + 8 * 4, 1, struct.pack("<4Q", *entries))
    else:
        x = sdt(b"RSDT", 36 + 4 * 4, 1, struct.pack("<4I", *entries))
    mem[0x40:0x40 + len(x)] = x
    return (b"\x00" if rev2 else b"\x01") + bytes(mem)


def e820():
    ents = [(0, 0x9F000, 1), (0x9F000, 0x1000, 2), (0x100000, 0x7FF00000, 1),
            (0x200000, 0x100000, 1), (0x7FF00000, 0x100000, 2), (0x100000000, 0x40000000, 1)]
    return bytes([len(ents)]) + b"".join(struct.pack("<QQI", *e) for e in ents)


def spd_ddr4():
    s = bytearray(512)
    s[0], s[1], s[2], s[3] = 0x23, 0x11, 0x0C, 0x01   # 384 bytes used, rev 1.1, DDR4, RDIMM
    s[4] = 0x85        # 8 Gb, 16 banks
    s[6] = 0x00        # monolithic
    s[12] = 0x01       # x8 devices, 1 rank
    s[13] = 0x0B       # 64-bit bus + ECC
    s[17] = 0x00
    s[18] = 0x05       # tCKmin 0.625 ns -> DDR4-3200
    s[24], s[25], s[26] = 0x6E, 0x6E, 0x6E
    s[320], s[321] = 0x80, 0xCE                       # Samsung
    s[323], s[324] = 0x21, 0x14
    sku = b"M393A1K43BB1-CTD    "
    s[329:329 + len(sku)] = sku
    return bytes(s)


def fat16_image(partitioned):
    sec = 512
    reserved, nfats, spf, root_entries, total = 1, 1, 17, 16, 1 + 17 + 1 + 4100
    vbr = bytearray(sec)
    vbr[0:3] = b"\xEB\x3C\x90"
    vbr[3:11] = b"MAYHEM  "
    struct.pack_into("<HBHBHHBHHHII", vbr, 11, sec, 1, reserved, nfats, root_entries, total, 0xF8, spf,
                     32, 64, 0, 0)
    vbr[54:62] = b"FAT16   "
    vbr[510:512] = b"\x55\xAA"
    fat = bytearray(sec * spf)
    struct.pack_into("<HH", fat, 0, 0xFFF8, 0xFFFF)
    root = bytearray(sec)
    root[0:11] = b"MT86P_01TXT"   # an existing report, so the next name is MT86P_02TXT
    root[11] = 0x20
    struct.pack_into("<HI", root, 26, 2, 5)
    struct.pack_into("<H", fat, 4, 0xFFFF)
    data = bytearray(sec * 4)
    data[0:5] = b"hello"
    vol = bytes(vbr + fat + root + data)
    if not partitioned:
        return vol
    mbr = bytearray(sec)
    struct.pack_into("<B3sB3sII", mbr, 446, 0x80, b"\0\0\0", 0x06, b"\0\0\0", 1, total)
    mbr[510:512] = b"\x55\xAA"
    return bytes(mbr) + vol




def usb_msd_replies():
    """Device-to-host bulk data, in the order msd_init()/fat32_mount() ask for it."""
    csw_ok = struct.pack("<IIIB", 0x53425355, 0, 0, 0)   # tag field 0 = echo the CBW tag
    img = fat16_image(False)
    out = b""
    out += csw_ok                                         # TEST UNIT READY
    out += struct.pack(">II", len(img) // 512 - 1 + 4096, 512) + csw_ok   # READ CAPACITY(10)
    for lba in range(0, 24):                              # sector reads (mount + root dir scan)
        out += img[lba * 512:(lba + 1) * 512] + csw_ok
    return out


def usb_device(cls, product_str):
    return struct.pack("<BBHBBBBHHHBBBB", 18, 1, 0x0200, cls, 0, 0, 64, 0x1234, 0x5678, 0x0100, 0, product_str, 0, 1)


def usb_config(body):
    total = 9 + len(body)
    cfg = struct.pack("<BBHBBBBB", 9, 2, total, 1, 1, 0, 0x80, 50)
    return cfg + body


def usb_enum_keyboard():
    body = struct.pack("<BBBBBBBBB", 9, 4, 0, 0, 1, 3, 1, 1, 0)          # HID boot keyboard
    body += struct.pack("<BBHBBBH", 9, 0x21, 0x0111, 0, 1, 0x22, 0x3F)   # HID descriptor
    body += struct.pack("<BBBBHB", 7, 5, 0x81, 3, 8, 10)                 # interrupt IN
    cfg = usb_config(body)
    return b"\x03" + usb_device(0, 0) + cfg[:9] + cfg


def usb_enum_msd():
    body = struct.pack("<BBBBBBBBB", 9, 4, 0, 0, 2, 8, 6, 0x50, 0)       # mass storage, SCSI, BOT
    body += struct.pack("<BBBBHB", 7, 5, 0x81, 2, 512, 0)                # bulk IN
    body += struct.pack("<BBBBHB", 7, 5, 0x02, 2, 512, 0)                # bulk OUT
    cfg = usb_config(body)
    name = "USB Stick".encode("utf-16-le")
    return b"\x03" + usb_device(0, 2) + cfg[:9] + cfg + bytes([2 + len(name), 3]) + name


put("mayhem/fuzz_smbios/testsuite/smbios_v3.bin", smbios_v3())
put("mayhem/fuzz_smbios/testsuite/smbios_v2.bin", smbios_v2())
put("mayhem/fuzz_acpi/testsuite/acpi_xsdt.bin", acpi_tables(True))
put("mayhem/fuzz_acpi/testsuite/acpi_rsdt.bin", acpi_tables(False))
put("mayhem/fuzz_acpi/testsuite/acpi_numa_xsdt.bin", acpi_numa_tables(True))
put("mayhem/fuzz_acpi/testsuite/acpi_numa_rsdt.bin", acpi_numa_tables(False))
put("mayhem/fuzz_pmem/testsuite/e820.bin", e820())
put("mayhem/fuzz_spd/testsuite/ddr4.bin", spd_ddr4())
put("mayhem/fuzz_fat32/testsuite/fat16.img", fat16_image(False))
put("mayhem/fuzz_fat32/testsuite/fat16_mbr.img", fat16_image(True))
put("mayhem/fuzz_config/testsuite/cmdline.txt",
    b"console=ttyS1,9600 cpuseqmode=seq reportmode=badram keyboard=usb nopause dark maxpasses=7 "
    b"noehci testlist=1,3\0")
put("mayhem/fuzz_config/testsuite/cmdline_mmio.txt", b"console=mmio32,0xfe215040,115200 smp powersave=off\0")
put("mayhem/fuzz_usbmsd/testsuite/msd_fat16.bin", usb_msd_replies())
put("mayhem/fuzz_usbenum/testsuite/keyboard.bin", usb_enum_keyboard())
put("mayhem/fuzz_usbenum/testsuite/msd.bin", usb_enum_msd())
