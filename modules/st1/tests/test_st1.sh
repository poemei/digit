#!/bin/sh
# [AI:GPT-6 | 2026-10-25 19:00:00 UTC]
# Isolated CLI end-to-end test: no production data, deployment or Core changes.
set -eu
test -s bin/st1.so
test -x bin/st1_manage
if bin/st1_manage >/dev/null 2>&1; then
  echo 'FAIL: missing arguments accepted' >&2
  exit 1
fi
tmp=$(mktemp -d "${TMPDIR:-/tmp}/digit-st1-cli.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"${CC:-cc}" -Iincludes -I../../core/stnlabz_abi \
  -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
  -DST1_CLI "-DST1_PATH=\"$tmp/research.tsv\"" src/st1.c -o "$tmp/st1_manage"
"$tmp/st1_manage" upsert ST1-TEST MISSION "Mission test" \
  "https://example.org/mission" 2026-10-25 "First report" Reported High \
  "Ezra,SD,communications" None "Demonstration released" | grep -q 'revision=1'
"$tmp/st1_manage" upsert ST1-TEST MISSION "Mission test" \
  "https://example.org/mission" 2026-10-25 "Updated report" Verified High \
  "Ezra,SD,communications" Reinforces "Results published" | grep -q 'revision=2'
"$tmp/st1_manage" list | grep -q 'r2'
"$tmp/st1_manage" watchlist | grep -q 'ST1-TEST'
"$tmp/st1_manage" show ST1-TEST | grep -q 'Updated report'
"$tmp/st1_manage" history ST1-TEST | grep -q 'r1'
"$tmp/st1_manage" search communications | grep -q 'ST1-TEST'
if "$tmp/st1_manage" show DOES-NOT-EXIST | grep -q 'ST1-TEST'; then
  echo 'FAIL: unknown record returned an unrelated record' >&2
  exit 1
fi
if "$tmp/st1_manage" upsert ST1-BAD REPORT Bad http://example.org 2026-10-25 \
  "Should be rejected" Verified High SD None "" >/dev/null 2>&1; then
  echo 'FAIL: insecure source accepted' >&2
  exit 1
fi
test "$(wc -l < "$tmp/research.tsv")" -eq 2
echo 'ST-1 isolated CLI persistence, revision, query and negative checks: PASS'
# [End AI:GPT-6]
