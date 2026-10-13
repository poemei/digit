#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cd "$tmp"
printf '/diagnose\npurrs\npuurs\n/diagnose\n/quit\n' | "$root/bin/digit" --mvc >out.txt 2>err.txt
count=0
failed=0
expect(){
 count=$((count+1))
 if ! grep -F "$1" "$2" >/dev/null; then
  failed=$((failed+1))
  printf 'FAIL mvc_console: missing %s in %s\n' "$1" "$2"
 fi
}
expect 'Modules ACTIVE: router, expression, rendering' out.txt
expect 'purrs: cat vocalization (evidence: fixture:expression-v1)' out.txt
expect 'Expression unknown.' out.txt
expect 'No request trace recorded.' out.txt
expect 'EXPRESSION decision=UNKNOWN file=src/model_expression.c' out.txt
expect 'DSD REPORT status=observed_only' logs/digit.log
expect 'ROUTER DISPATCH request=console-3 route=/diagnose service=dsd.report outcome=0' logs/digit.log
expect 'RENDERING DIAGNOSTIC status=formatted' logs/digit.log
expect 'ACTIVATION ACTIVATE_PASS id=dsd' logs/digit.log
expect 'DSD STOP status=stopped' logs/digit.log
expect 'MVC START mode=explicit_local_development authentication=not_provided' logs/digit.log
expect 'ACTIVATION ACTIVATE_PASS id=expression' logs/digit.log
expect 'ACTIVATION ACTIVATE_PASS id=rendering' logs/digit.log
expect 'ACTIVATION ACTIVATE_PASS id=router' logs/digit.log
expect 'ROUTER DISPATCH request=console-1' logs/digit.log
expect 'ROUTER DISPATCH request=console-2' logs/digit.log
expect 'MODEL_EXPRESSION LOOKUP expression=purrs' logs/digit.log
expect 'MODEL_EXPRESSION LOOKUP expression=puurs' logs/digit.log
expect 'RENDERING PRESENT status=formatted' logs/digit.log
expect 'BOOTSTRAP STOPPED status=router_stopped' logs/digit.log
expect 'MVC STOPPED mode=explicit_local_development' logs/digit.log
if printf 'invalid\n' | "$root/bin/digit" --bogus >bad.txt 2>&1; then
 failed=$((failed+1))
fi
count=$((count+1))
expect 'Usage: ./bin/digit [--mvc]' bad.txt
printf 'Core mvc_console tests: %s executed, %s failed\n' "$count" "$failed"
[ "$failed" -eq 0 ]
