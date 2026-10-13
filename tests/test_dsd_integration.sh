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
printf '/diagnose\npuuurrrs\ndiagnose my last request\n/quit\n' | ./bin/digit --mvc >out.txt 2>err.txt
checks=0;failures=0
expect(){
 checks=$((checks+1))
 if ! grep -F "$1" "$2" >/dev/null; then
  failures=$((failures+1))
  printf 'FAIL dsd_integration: expected %s in %s\n' "$1" "$2"
 fi
}
expect 'DSD ACTIVE: installed independent dsd.so' out.txt
expect 'No previous request available.' out.txt
expect 'Expression unknown.' out.txt
expect 'EXPRESSION decision=UNKNOWN' out.txt
expect 'file=src/model_expression.c' out.txt
expect 'ROUTER DISPATCH request=console-2 route=/diagnose service=dsd.report outcome=0' logs/digit.log
expect 'RENDERING DIAGNOSTIC status=presented' logs/digit.log
expect 'ACTIVATION ACTIVATE_PASS id=dsd' logs/digit.log
expect 'ACTIVATION STOP_PASS id=dsd' logs/digit.log
expect 'MVC STOPPED mode=explicit_local_development' logs/digit.log
printf 'Core dsd_integration tests: %s executed, %s failed\n' "$checks" "$failures"
[ "$failures" -eq 0 ]
