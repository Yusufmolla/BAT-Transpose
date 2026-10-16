#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/build}"
cpack --config "$build/CPackConfig.cmake" -C Release -B "$root/dist"
