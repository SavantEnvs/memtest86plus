#!/usr/bin/env bash
#
# mayhem/build.sh — host builds of the memtest86+ firmware/device parsers.
#
# memtest86+ is a bare-metal/UEFI program (its own Makefiles in build/<arch>/ produce boot images
# with a custom linker script), so there is no host library to link. Instead each upstream parser
# unit (system/smbios.c, acpi.c, smp.c (inside harness/smp_tables.c), pmem.c, spd.c, fat32.c, usbmsd.c, app/config.c, and lib/print.c
# for screen output) is
# compiled unmodified for the host with the sanitizers, and linked with mayhem/harness/: a model of
# firmware memory (fakephys.c: map_region, boot_params, EFI system table) and of the platform
# layer (platform.c: screen, SPD SMBus, USB MSD sectors, RTC).
#
# Outputs: /mayhem/mayhem/out/fuzz_<target> (libFuzzer), /mayhem/mayhem/out/fuzz_<target>-standalone (run-once
# reproducer), /mayhem/mayhem/out/selftest{,_usb,_smp} (known-answer suites, normal flags, run by mayhem/test.sh).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
cd "$SRC"

OUT="$SRC/mayhem/out"
OBJ="$SRC/mayhem/out/obj"
mkdir -p "$OUT" "$OBJ/fuzz" "$OBJ/test"

# Upstream compiles these units as freestanding 64-bit code (build/x86_64/Makefile).
INC="-Iboot -Isystem -Isystem/imc -Isystem/x86 -Ilib -Itests -Iapp -Imayhem/harness"
UFLAGS="-std=gnu11 -DARCH_BITS=64 -ffreestanding -fno-builtin -fno-strict-aliasing"
UNITS="system/smbios.c system/acpi.c system/pmem.c system/spd.c system/fat32.c lib/print.c
       app/config.c system/usbmsd.c lib/string.c system/usbhcd.c"
PLATFORM="mayhem/harness/fakephys.c mayhem/harness/platform.c mayhem/harness/disk.c
          mayhem/harness/config_stubs.c mayhem/harness/config_globals.c
          mayhem/harness/usb_stubs.c mayhem/harness/smp_tables.c"
TARGETS="smbios acpi pmem spd fat32 config usbmsd usbenum"
# Objects every target links (names as compile() writes them), and each target's extras.
COMMON="system_smbios.c system_acpi.c system_pmem.c system_spd.c system_fat32.c lib_print.c
        mayhem_harness_fakephys.c mayhem_harness_platform.c"
extra_objs() {
  case "$1" in
    config) echo "app_config.c lib_string.c mayhem_harness_config_stubs.c mayhem_harness_disk.c" ;;
    usbenum) echo "system_usbhcd.c system_usbmsd.c mayhem_harness_usb_stubs.c mayhem_harness_config_globals.c" ;;
    usbmsd) echo "system_usbmsd.c mayhem_harness_config_globals.c" ;;   # real msd_read_sectors, not disk.c
    # system/smp.c's MADT/SRAT parsers, compiled inside smp_tables.c (they are static).
    acpi)   echo "mayhem_harness_disk.c mayhem_harness_config_globals.c mayhem_harness_smp_tables.c" ;;
    *)      echo "mayhem_harness_disk.c mayhem_harness_config_globals.c" ;;
  esac
}
objs() {  # objs <flavour> <target>
  local o
  for o in $COMMON $(extra_objs "$2"); do printf '%s ' "$OBJ/$1/$o.o"; done
}

# compile <flavour> <extra flags...>: every unit + platform file -> $OBJ/<flavour>/*.o
compile() {
  local flavour="$1"; shift
  local src
  for src in $UNITS $PLATFORM; do
    local o="$OBJ/$flavour/$(echo "$src" | tr '/' '_').o"
    local extra=""
    # ACPI places 8-byte XSDT entries at offset 36 (4-byte aligned) by specification, so every
    # XSDT walk is a misaligned load, benign on x86; only that UBSan check is relaxed, only here.
    if [ "$flavour" = fuzz ] && [ "$src" = system/acpi.c ]; then extra="-fno-sanitize=alignment"; fi
    # lib/string.c is the firmware's own libc; only hexstr2int (used by app/config.c) is needed
    # from it, so its mem*/strstr are renamed to keep them from replacing the host's.
    if [ "$src" = lib/string.c ]; then
      extra="$extra -Dmemmove=mt_memmove -Dmemcpy=mt_memcpy -Dmemset=mt_memset -Dstrstr=mt_strstr"
    fi
    # shellcheck disable=SC2086
    $CC $UFLAGS $INC "$@" $extra -c "$src" -o "$o" &
    while [ "$(jobs -rp | wc -l)" -ge "$MAYHEM_JOBS" ]; do wait -n; done
  done
  wait
}

# 1) Sanitized + coverage-instrumented project units and harness platform layer.
# shellcheck disable=SC2086
compile fuzz $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.c -o "$OBJ/fuzz/lsan_off.o"

# 2) Each harness twice: libFuzzer binary and run-once standalone reproducer.
for t in $TARGETS; do
  # shellcheck disable=SC2086
  $CC $UFLAGS $INC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link \
      -c "mayhem/harness/fuzz_$t.c" -o "$OBJ/fuzz/fuzz_$t.o"
  # shellcheck disable=SC2086
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OBJ/fuzz/fuzz_$t.o" \
      $(objs fuzz "$t") "$OBJ/fuzz/lsan_off.o" -o "$OUT/fuzz_$t"
  # shellcheck disable=SC2086
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS "$STANDALONE_FUZZ_MAIN" "$OBJ/fuzz/fuzz_$t.o" \
      $(objs fuzz "$t") "$OBJ/fuzz/lsan_off.o" -o "$OUT/fuzz_$t-standalone"
done

# 3) Known-answer suite with normal flags (no sanitizers), run by mayhem/test.sh.
# shellcheck disable=SC2086
compile test -O2 -g $COVERAGE_FLAGS
# shellcheck disable=SC2086
$CC $UFLAGS $INC -O2 -g $COVERAGE_FLAGS -c mayhem/selftest/selftest.c -o "$OBJ/test/selftest.o"
# shellcheck disable=SC2086
$CC $COVERAGE_FLAGS "$OBJ/test/selftest.o" $(objs test config) -o "$OUT/selftest"

# shellcheck disable=SC2086
$CC $UFLAGS $INC -O2 -g $COVERAGE_FLAGS -c mayhem/selftest/selftest_usb.c -o "$OBJ/test/selftest_usb.o"
# shellcheck disable=SC2086
$CC $UFLAGS $INC -O2 -g $COVERAGE_FLAGS -c mayhem/harness/fuzz_usbenum.c -o "$OBJ/test/fuzz_usbenum.o"
# shellcheck disable=SC2086
$CC $COVERAGE_FLAGS "$OBJ/test/selftest_usb.o" "$OBJ/test/fuzz_usbenum.o" $(objs test usbenum) \
    -o "$OUT/selftest_usb"

# shellcheck disable=SC2086
$CC $UFLAGS $INC -O2 -g $COVERAGE_FLAGS -c mayhem/selftest/selftest_smp.c -o "$OBJ/test/selftest_smp.o"
# shellcheck disable=SC2086
$CC $COVERAGE_FLAGS "$OBJ/test/selftest_smp.o" $(objs test acpi) -o "$OUT/selftest_smp"

echo "built: $(cd "$OUT" && ls -1 | grep -v '^obj$' | tr '\n' ' ')"
