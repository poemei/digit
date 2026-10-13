#!/bin/sh
# Install Digit without modifying Core discovery, logging, or module ABI.
set -eu
PREFIX=${PREFIX:-/opt/digit}
OWNER=${OWNER:-digit}
GROUP=${GROUP:-digit}
case "$PREFIX" in /*) ;; *) echo "PREFIX must be absolute" >&2; exit 2;; esac
if [ "$(id -u)" -ne 0 ]; then
 echo "Installation requires root (sudo make install)." >&2; exit 2
fi
if ! getent group "$GROUP" >/dev/null 2>&1; then
 groupadd --system "$GROUP"
fi
if ! id "$OWNER" >/dev/null 2>&1; then
 useradd --system --gid "$GROUP" --home-dir "$PREFIX" --no-create-home --shell /usr/sbin/nologin "$OWNER"
fi
install -d -m 0750 "$PREFIX" "$PREFIX/bin" "$PREFIX/modules" "$PREFIX/logs"
if [ -e "$PREFIX/bin/modules" ] && [ ! -L "$PREFIX/bin/modules" ]; then
 echo "Refusing to replace existing $PREFIX/bin/modules; resolve it before installation." >&2; exit 2
fi
ln -sfn ../modules "$PREFIX/bin/modules"
install -m 0750 bin/digit "$PREFIX/bin/digit"
install -m 0750 scripts/digit-run "$PREFIX/run"
if [ ! -e "$PREFIX/logs/digit.log" ]; then
 install -m 0640 /dev/null "$PREFIX/logs/digit.log"
fi
if [ ! -e "$PREFIX/logs/modules.log" ]; then
 install -m 0640 /dev/null "$PREFIX/logs/modules.log"
fi
chown -R "$OWNER:$GROUP" "$PREFIX"
printf 'Installed Digit to %s/bin/digit; owner=%s:%s\n' "$PREFIX" "$OWNER" "$GROUP"
printf 'Use %s/run to ensure logs are written to %s/logs/digit.log\n' "$PREFIX" "$PREFIX"
