#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/build}"
if [[ $# -gt 0 ]]; then shift; fi
# Extra arguments are passed to CMake, e.g. -DBAT_BUILD_PLUGIN=OFF.
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$build" --parallel "${BAT_BUILD_JOBS:-4}"
ctest --test-dir "$build" -C Release --output-on-failure
