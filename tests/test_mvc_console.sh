#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cd "$tmp"
printf 'unknown\n/quit\n' | "$root/bin/digit" >out.txt 2>err.txt
count=0; failed=0
expect(){
 count=$((count+1))
 if ! grep -F "$1" "$2" >/dev/null; then failed=$((failed+1)); printf 'FAIL console: %s\n' "$1"; fi
}
expect 'STN-LABZ Digit | interactive console' out.txt
expect 'Unknown command.' out.txt
expect 'ACTIVATION ACTIVATE_PASS id=router' logs/digit.log
expect 'ROUTER DISPATCH request=console-1' logs/digit.log
expect 'CONSOLE STOPPED mode=local' logs/digit.log
if "$root/bin/digit" --bogus >bad.txt 2>&1; then failed=$((failed+1)); fi
count=$((count+1))
expect 'Usage: ./bin/digit [--core]' bad.txt
printf 'Core mvc_console tests: %s executed, %s failed\n' "$count" "$failed"
[ "$failed" -eq 0 ]
