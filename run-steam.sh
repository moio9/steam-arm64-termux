#!/data/data/com.termux/files/usr/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
while :; do
    set +e
    "$HERE/run-steam-tgcompat.sh" "$@"
    status=$?
    set -e
    [ "$status" -eq 42 ] || exit "$status"
    printf '%s\n' 'Steam update complete; relaunching client.'
done
