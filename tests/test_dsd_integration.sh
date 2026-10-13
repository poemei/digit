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
printf 'diagnose my last request\n/quit\n' | ./bin/digit >out.txt 2>err.txt
checks=0;failures=0
expect(){
 checks=$((checks+1))
 if ! grep -F "$1" "$2" >/dev/null; then
  failures=$((failures+1));printf 'FAIL dsd integration: %s\n' "$1"
 fi
}
expect 'STN-LABZ Digit | interactive console' out.txt
expect 'DSD diagnostic evidence (most recent logged events):' out.txt
expect 'ROUTER DISPATCH request=console-1 route=/diagnose service=command.diagnose outcome=0' logs/digit.log
expect 'ACTIVATION ACTIVATE_PASS id=dsd' logs/digit.log
expect 'ACTIVATION STOP_PASS id=dsd' logs/digit.log
expect 'CONSOLE STOPPED mode=local' logs/digit.log
printf 'Core dsd_integration tests: %s executed, %s failed\n' "$checks" "$failures"
[ "$failures" -eq 0 ]
