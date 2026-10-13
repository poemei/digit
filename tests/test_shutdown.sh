#!/bin/sh
# Core integration check: one process must write its own complete shutdown trail.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
pid=
cleanup() {
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        kill -TERM "$pid" 2>/dev/null || :
        wait "$pid" 2>/dev/null || :
    fi
    rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM
(
    cd "$tmp"
    exec "$root/bin/digit" --core >"$tmp/stdout" 2>"$tmp/stderr"
) &
pid=$!
ready=0
i=0
while [ "$i" -lt 50 ]; do
    if [ -f "$tmp/logs/digit.log" ] &&
       grep -q 'RUNTIME START status=running' "$tmp/logs/digit.log"; then
        ready=1
        break
    fi
    if ! kill -0 "$pid" 2>/dev/null; then break; fi
    sleep 0.1
    i=$((i + 1))
done
if [ "$ready" -ne 1 ]; then
    echo "FAIL shutdown: Digit never reached logged running state" >&2
    cat "$tmp/stderr" >&2
    exit 1
fi
kill -TERM "$pid"
if ! wait "$pid"; then
    pid=
    echo "FAIL shutdown: Digit exited unsuccessfully" >&2
    cat "$tmp/stderr" >&2
    exit 1
fi
pid=
log="$tmp/logs/digit.log"
for event in 'RUNTIME STOP reason=stop_requested' \
             'CORE STOPPED reason=signal'; do
    if ! grep -Fq "$event" "$log"; then
        echo "FAIL shutdown: missing $event" >&2
        cat "$log" >&2
        exit 1
    fi
done
if ! grep -Fq 'MODULE MANAGER_SHUTDOWN id=unknown result=0' "$tmp/logs/modules.log"; then echo 'FAIL shutdown: module manager shutdown missing' >&2; exit 1; fi
if [ "$(grep -c 'CORE STOPPED reason=signal' "$log")" -ne 1 ]; then
    echo "FAIL shutdown: expected exactly one final shutdown record" >&2
    exit 1
fi
echo "Core shutdown integration tests: 5 executed, 0 failed"
