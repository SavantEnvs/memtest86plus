#!/usr/bin/env bash
#
# mayhem/test.sh — run the known-answer suites build.sh produced (mayhem/out/selftest{,_usb,_smp}). Upstream ships
# no host tests (tests/ holds the RAM test algorithms), so mayhem/selftest/selftest.c feeds
# committed SMBIOS, ACPI (incl. MADT/SRAT), e820, SPD, FAT16, command-line and USB descriptor inputs through the upstream parsers and asserts
# the values they decode (board/serial strings, table addresses, memory map, module size/speed/
# SKU, written report file, CPU list and NUMA domains).
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# The suite has this many checks; anything missing (crash, early exit) counts as failed.
EXPECTED=45
if [ ! -x mayhem/out/selftest ] || [ ! -x mayhem/out/selftest_usb ] || [ ! -x mayhem/out/selftest_smp ]; then
  echo "mayhem/out/selftest{,_usb,_smp} missing — mayhem/build.sh did not build them" >&2
  emit_ctrf "memtest-selftest" 0 "$EXPECTED"
  exit 1
fi
log=$(mayhem/out/selftest "$SRC" 2>&1); rc=$?
log_usb=$(mayhem/out/selftest_usb "$SRC" 2>&1); rc_usb=$?
log_smp=$(mayhem/out/selftest_smp "$SRC" 2>&1); rc_smp=$?
log="$log
$log_usb
$log_smp"
[ "$rc" -eq 0 ] && rc=$rc_usb
[ "$rc" -eq 0 ] && rc=$rc_smp
echo "$log"
passed=$(grep -c '^PASS ' <<<"$log")
failed=$(grep -c '^FAIL ' <<<"$log")
missing=$(( EXPECTED - passed - failed ))
[ "$missing" -gt 0 ] && failed=$(( failed + missing ))
[ "$rc" -ne 0 ] && [ "$failed" -eq 0 ] && failed=1
emit_ctrf "memtest-selftest" "$passed" "$failed"
