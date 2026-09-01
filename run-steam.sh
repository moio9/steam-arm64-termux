#!/data/data/com.termux/files/usr/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$HERE/run-steam-tgcompat.sh" "$@"
