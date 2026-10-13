#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
i=0
while [ "$i" -lt 65 ]; do
    id=$(printf 'grow-%03d' "$i")
    "${CC:-cc}" -I"$root/include" -I"$root/include/stnlabz_abi" -std=c11 -fPIC -shared \
      -DTEST_EXPORT -DTEST_MODULE_ID="\"$id\"" \
      "$root/tests/fixtures/abi_loader_fixture.c" -o "$tmp/$id.so"
    i=$((i+1))
done
"$root/bin/test_loader_growth" "$tmp"
