#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/build}"
cmake --build "$build" --config Release --target bat_validate --parallel "${BAT_BUILD_JOBS:-4}"
ctest --test-dir "$build" -C Release --output-on-failure
