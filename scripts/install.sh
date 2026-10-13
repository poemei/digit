#!/bin/sh
# Install Digit without modifying Core discovery, logging, or module ABI.
set -eu
PREFIX=${PREFIX:-/opt/digit}
OWNER=${OWNER:-digit}
GROUP=${GROUP:-digit}
case "$PREFIX" in /*) ;; *) echo "PREFIX must be absolute" >&2; exit 2;; esac
if ! id "$OWNER" >/dev/null 2>&1; then
 echo "Missing user $OWNER. Create digit system account before installation." >&2; exit 2
fi
if ! getent group "$GROUP" >/dev/null 2>&1; then
 echo "Missing group $GROUP." >&2; exit 2
fi
install -d -m 0750 "$PREFIX" "$PREFIX/modules" "$PREFIX/logs"
install -m 0750 bin/digit "$PREFIX/digit"
install -m 0750 scripts/digit-run "$PREFIX/run"
if [ ! -e "$PREFIX/logs/digit.log" ]; then
 install -m 0640 /dev/null "$PREFIX/logs/digit.log"
fi
chown -R "$OWNER:$GROUP" "$PREFIX"
printf 'Installed Digit to %s/digit; owner=%s:%s\n' "$PREFIX" "$OWNER" "$GROUP"
printf 'Use %s/run to ensure logs are written to %s/logs/digit.log\n' "$PREFIX" "$PREFIX"
