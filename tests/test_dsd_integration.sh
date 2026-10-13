#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/bin/modules/dsd/bin" "$tmp/bin/modules/expression/bin"
cp "$root/bin/digit" "$tmp/bin/digit"
cp "$root/modules/dsd/bin/dsd.so" "$tmp/bin/modules/dsd/bin/dsd.so"
cp "$root/modules/dsd/module.conf" "$tmp/bin/modules/dsd/module.conf"
cp "$root/modules/expression/bin/expression.so" "$tmp/bin/modules/expression/bin/expression.so"
cp "$root/modules/expression/module.conf" "$tmp/bin/modules/expression/module.conf"
cd "$tmp"
mkdir -p logs
printf '2026-10-13T03:51:47Z MODULE FAILED id=dsd file=modules/dsd/src/dsd.c line=42 reason=fixture_failure\n' >logs/digit.log
printf 'diagnose fixture_failure\ndiagnose no_such_record\npurrs\npuuurs\ndiagnose my last request\n/quit\n' | ./bin/digit >out.txt 2>err.txt
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
expect 'purrs: cat vocalization (evidence: fixture:expression-v1)' out.txt
expect 'ACTIVATION ACTIVATE_PASS id=expression' logs/digit.log
expect 'ACTIVATION STOP_PASS id=expression' logs/digit.log
expect 'Log: 2026-10-13T03:51:47Z MODULE FAILED' out.txt
expect 'Code: modules/dsd/src/dsd.c:42' out.txt
expect 'Finding: Recorded event indicates the requested condition; reason=fixture_failure.' out.txt
expect 'Log: no matching recorded event' out.txt
expect 'Code: not_recorded' out.txt
expect 'Module: router' out.txt
expect 'reason=no_registered_route' out.txt
expect 'route=/puuurs' out.txt
expect 'ROUTER DISPATCH request=console-4 route=/puuurs service=none outcome=2' logs/digit.log
expect 'ROUTER DISPATCH request=console-1 route=/diagnose service=command.diagnose outcome=0' logs/digit.log
expect 'ACTIVATION ACTIVATE_PASS id=dsd' logs/digit.log
expect 'ACTIVATION STOP_PASS id=dsd' logs/digit.log
expect 'CONSOLE STOPPED mode=local' logs/digit.log
printf 'Core dsd_integration tests: %s executed, %s failed\n' "$checks" "$failures"
[ "$failures" -eq 0 ]
