#!/bin/sh
# [AI:GPT-6 | 2026-10-25 18:20:00 UTC]
set -eu
test -s bin/st1.so
test -x bin/st1_manage
if bin/st1_manage >/dev/null 2>&1; then echo 'Missing-argument rejection failed'; exit 1; fi
echo 'ST-1 build and CLI rejection: PASS'
# [End AI:GPT-6]
