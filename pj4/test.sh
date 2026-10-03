#!/usr/bin/env bash
# Run ctest in every build directory produced by build.sh.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
found=0
for dir in "${ROOT}/build" "${ROOT}/build/debug"; do
  [[ -f "${dir}/CMakeCache.txt" ]] || continue
  found=1
  cfg="$(grep -E '^CMAKE_BUILD_TYPE:' "${dir}/CMakeCache.txt" | cut -d= -f2)"
  ctest --test-dir "${dir}" -C "${cfg:-Release}" --output-on-failure
done
[[ ${found} -eq 1 ]] || { echo "No build directory found; run ./build.sh first"; exit 1; }
