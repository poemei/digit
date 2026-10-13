#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/valid/mock/bin" "$tmp/bad/invalid"
printf 'id=mock\n' > "$tmp/valid/mock/module.conf"
cp "$root/bin/loader_valid.so" "$tmp/valid/mock/bin/mock.so"
printf 'invalid-manifest\n' > "$tmp/bad/invalid/module.conf"
"$root/bin/test_discovery" "$tmp/valid" "$tmp/bad" "$tmp/absent" "$tmp/discovery.log"
