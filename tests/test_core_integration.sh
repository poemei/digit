#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
pid=
cleanup(){ if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then kill -TERM "$pid" 2>/dev/null || :; wait "$pid" 2>/dev/null || :; fi; rm -rf "$tmp"; }
trap cleanup EXIT HUP INT TERM
( cd "$tmp"; exec "$root/bin/digit" >"$tmp/stdout" 2>"$tmp/stderr" ) &
pid=$!
ready=0
i=0
while [ "$i" -lt 50 ]; do
  if [ -f "$tmp/logs/digit.log" ] && grep -Fq 'RUNTIME START status=running' "$tmp/logs/digit.log"; then ready=1; break; fi
  if ! kill -0 "$pid" 2>/dev/null; then break; fi
  sleep 0.1
  i=$((i+1))
done
if [ "$ready" -ne 1 ]; then echo "FAIL core integration: no running evidence" >&2; cat "$tmp/stderr" >&2; exit 1; fi
kill -TERM "$pid"
if ! wait "$pid"; then pid=; echo "FAIL core integration: nonzero shutdown" >&2; exit 1; fi
pid=
log="$tmp/logs/digit.log"
count=0
for event in 'CORE ROOT_VERIFIED agent=digit' 'DISCOVERY SCAN_COMPLETE examined=0 loaded=0 discovered=0 rejected=0' 'INVENTORY SNAPSHOT count=0' 'CORE RUNNING verification=complete qualification=complete activation=disabled' 'RUNTIME START status=running' 'RUNTIME STOP reason=stop_requested' 'CORE STOPPED reason=signal'; do
  if ! grep -Fq "$event" "$log"; then echo "FAIL core integration: missing $event" >&2; exit 1; fi
  count=$((count+1))
done
if grep -Eq 'ACTIVATION ACTIVATE_PASS|TRACE RECORD|ESCALATION REPORT' "$log"; then echo 'FAIL core integration: unexpected behavior' >&2; exit 1; fi
count=$((count+1))
if ! grep -Fq '0 registered modules' "$tmp/stdout"; then echo 'FAIL core integration: console mismatch' >&2; exit 1; fi
count=$((count+1))
echo "Core core integration tests: $count executed, 0 failed"
