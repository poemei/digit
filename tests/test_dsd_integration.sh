#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/bin/modules/dsd/bin"
cp "$root/bin/digit" "$tmp/bin/digit"
cp "$root/modules/dsd/bin/dsd.so" "$tmp/bin/modules/dsd/bin/dsd.so"
cp "$root/modules/dsd/module.conf" "$tmp/bin/modules/dsd/module.conf"
cd "$tmp"
mkdir -p logs
printf '2026-10-13T03:51:47Z MODULE FAILED id=dsd file=modules/dsd/src/dsd.c line=42 reason=fixture_failure\\n' >logs/digit.log
printf 'diagnose fixture_failure\ndiagnose no_such_record\n/quit\n' | ./bin/digit >out.txt 2>err.txt
checks=0;failures=0
expect(){
 checks=$((checks+1))
 if ! grep -F "$1" "$2" >/dev/null; then
  failures=$((failures+1));printf 'FAIL dsd integration: %s\n' "$1"
 fi
}
expect 'STN-LABZ Digit | interactive console' out.txt
expect 'DSD DIAGNOSTIC REPORT' out.txt
expect 'Module: dsd' out.txt
expect 'Log: 2026-10-13T03:51:47Z MODULE FAILED' out.txt
expect 'Code: modules/dsd/src/dsd.c:42' out.txt
expect 'Finding: Recorded event indicates the requested condition; reason=fixture_failure.' out.txt
expect 'Log: no matching recorded event' out.txt
expect 'Code: not_recorded' out.txt
expect 'ROUTER DISPATCH request=console-1 route=/diagnose service=command.diagnose outcome=0' logs/digit.log
expect 'ACTIVATION ACTIVATE_PASS id=dsd' logs/digit.log
expect 'ACTIVATION STOP_PASS id=dsd' logs/digit.log
expect 'CONSOLE STOPPED mode=local' logs/digit.log
printf 'Core dsd_integration tests: %s executed, %s failed\n' "$checks" "$failures"
[ "$failures" -eq 0 ]
