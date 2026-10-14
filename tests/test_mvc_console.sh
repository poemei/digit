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
expect 'STN-LABZ Digit 1.0.0 | interactive console' out.txt
expect 'Unknown command.' out.txt
expect 'MODULE HOTLOAD_ACTIVE sequence=' logs/modules.log
expect 'module=router' logs/modules.log
expect 'ROUTER DISPATCH request=console-1' logs/digit.log
expect 'CONSOLE STOPPED mode=local' logs/digit.log
if "$root/bin/digit" --bogus >bad.txt 2>&1; then failed=$((failed+1)); fi
count=$((count+1))
expect 'Usage: ./bin/digit [--core]' bad.txt
# A systemd-style /dev/null stdin must not shut Digit down at EOF.
"$root/bin/digit" </dev/null >service.txt 2>service.err &
service_pid=$!
ready=0
i=0
while [ "$i" -lt 50 ]; do
 if grep -Fq 'CORE SERVICE_READY mode=unattended' logs/digit.log 2>/dev/null; then
  if kill -0 "$service_pid" 2>/dev/null; then ready=1; fi
  break
 fi
 if ! kill -0 "$service_pid" 2>/dev/null; then break; fi
 sleep 0.1
 i=$((i+1))
done
count=$((count+1))
if [ "$ready" -ne 1 ]; then failed=$((failed+1)); echo 'FAIL console: unattended service exited or never became ready'; fi
if kill -0 "$service_pid" 2>/dev/null; then kill -TERM "$service_pid" || :; fi
wait "$service_pid" || { failed=$((failed+1)); echo 'FAIL console: unattended service stop failed'; }
count=$((count+1))
expect 'RUNTIME STOP reason=stop_requested' logs/digit.log
printf 'Core mvc_console tests: %s executed, %s failed\n' "$count" "$failed"
[ "$failed" -eq 0 ]
