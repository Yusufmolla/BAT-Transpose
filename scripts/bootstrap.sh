#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
checkout() {
    local name="$1" url="$2" commit="$3" dest="$root/external/$1"
    if [[ ! -d "$dest" ]]; then git clone "$url" "$dest"; fi
    if [[ "$(git -C "$dest" rev-parse HEAD)" != "$commit" ]]; then
        if [[ -n "$(git -C "$dest" status --porcelain)" ]]; then
            printf 'Local changes in %s; refusing to overwrite\n' "$name" >&2
            exit 1
        fi
        git -C "$dest" fetch origin "$commit"
        git -C "$dest" checkout --detach "$commit"
    fi
}
checkout vst3sdk https://github.com/steinbergmedia/vst3sdk.git 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0
git -C "$root/external/vst3sdk" submodule update --init --depth 1 base cmake pluginterfaces public.sdk vstgui4
