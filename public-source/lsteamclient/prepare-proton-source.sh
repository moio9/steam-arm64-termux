#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

here=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
# shellcheck source=source.lock
. "$here/source.lock"

patch=$here/patches/0001-termux-arm64-bridge.patch
work_root=${LSTEAM_WORK_ROOT:-$here/work}
checkout=${PROTON_CHECKOUT:-$work_root/proton}
deps_dir=${LSTEAM_DEPS_DIR:-$work_root/deps}
repository=${PROTON_REPOSITORY_OVERRIDE:-$PROTON_REPOSITORY}
stage=
header_tmp=

cleanup()
{
    if [[ -n $header_tmp && -e $header_tmp ]]; then rm -f -- "$header_tmp"; fi
    if [[ -n $stage && -d $stage ]]; then rm -rf -- "$stage"; fi
}
trap cleanup EXIT HUP INT TERM

die()
{
    printf 'prepare-proton-source: %s\n' "$*" >&2
    exit 1
}

for command in git curl sha256sum mktemp; do
    command -v "$command" >/dev/null 2>&1 || die "missing command: $command"
done

[[ -f $patch ]] || die "missing patch: $patch"
printf '%s  %s\n' "$PATCH_SHA256" "$patch" | sha256sum -c - >/dev/null ||
    die 'patch checksum mismatch'

[[ ! -e $checkout ]] ||
    die "checkout already exists (choose another PROTON_CHECKOUT): $checkout"
mkdir -p -- "${checkout%/*}" "$deps_dir/include/wine"
stage=$(mktemp -d "${checkout%/*}/.proton-lsteamclient.XXXXXX")

git -C "$stage" init -q
git -C "$stage" remote add origin "$repository"
git -C "$stage" config advice.detachedHead false
git -C "$stage" sparse-checkout init --no-cone
git -C "$stage" sparse-checkout set --no-cone \
    '/lsteamclient/*' '!/lsteamclient/steamworks_sdk_*/'

fetch_args=(fetch --quiet --depth=1)
case $repository in
    http://*|https://*)
        git -C "$stage" config remote.origin.promisor true
        git -C "$stage" config remote.origin.partialclonefilter blob:none
        fetch_args+=(--filter=blob:none)
        ;;
esac
git -C "$stage" "${fetch_args[@]}" origin "refs/tags/$PROTON_TAG"
git -C "$stage" checkout --quiet --detach FETCH_HEAD

actual_commit=$(git -C "$stage" rev-parse HEAD)
[[ $actual_commit == "$PROTON_COMMIT" ]] ||
    die "unexpected Proton commit: $actual_commit"

actual_wine_commit=$(git -C "$stage" ls-tree "$PROTON_COMMIT" wine | awk '$4 == "wine" {print $3}')
[[ $actual_wine_commit == "$PROTON_WINE_COMMIT" ]] ||
    die "unexpected Proton Wine gitlink: ${actual_wine_commit:-missing}"

if find "$stage/lsteamclient" -mindepth 1 -maxdepth 1 \
    -name 'steamworks_sdk_*' -print -quit | grep -q .; then
    die 'sparse checkout unexpectedly contains a steamworks_sdk tree'
fi

git -C "$stage" apply --check "$patch"
git -C "$stage" apply "$patch"
[[ -f $stage/lsteamclient/bridge_proxy.inc ]] ||
    die 'patch did not create bridge_proxy.inc'

list_header=$deps_dir/include/wine/list.h
if [[ -f $list_header ]]; then
    printf '%s  %s\n' "$WINE_LIST_SHA256" "$list_header" |
        sha256sum -c - >/dev/null ||
        die "existing Wine list.h has the wrong checksum: $list_header"
else
    header_tmp=$(mktemp "$deps_dir/include/wine/.list.h.XXXXXX")
    curl --fail --location --retry 3 --silent --show-error \
        "https://raw.githubusercontent.com/ValveSoftware/wine/$PROTON_WINE_COMMIT/include/wine/list.h" \
        --output "$header_tmp"
    printf '%s  %s\n' "$WINE_LIST_SHA256" "$header_tmp" |
        sha256sum -c - >/dev/null || die 'downloaded Wine list.h checksum mismatch'
    chmod 0644 "$header_tmp"
    mv -- "$header_tmp" "$list_header"
    header_tmp=
fi

mv -- "$stage" "$checkout"
stage=

printf 'Prepared Proton lsteamclient source\n'
printf '  upstream: %s (%s)\n' "$PROTON_TAG" "$PROTON_COMMIT"
printf '  checkout: %s\n' "$checkout"
printf '  Wine header: %s\n' "$list_header"
printf '  local changes:\n'
git -C "$checkout" status --short --untracked-files=all
