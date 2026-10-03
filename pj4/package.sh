#!/usr/bin/env bash
# Package plugins from a finished build into distributable ZIPs (dist/<id>-<version>-<os>-<arch>.zip).
#   ./package.sh [plugin_dir ...]   default: every directory under plugins/
# Validates manifest.json (incl. min_sdk_required <= SDK_VERSION) and the compiled binary's embedded manifest.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build}"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) OS_LABEL=windows ;;
  Darwin) OS_LABEL=macos ;;
  *) OS_LABEL=linux ;;
esac
ARCH="${ARCH:-$(uname -m | sed 's/AMD64/x86_64/')}"
PY="$(command -v python3 || command -v python)"

PLUGINS=("$@")
if [[ ${#PLUGINS[@]} -eq 0 ]]; then
  for d in "${ROOT}"/plugins/*/; do [[ -f "${d}manifest.json" ]] && PLUGINS+=("$(basename "$d")"); done
fi

for p in "${PLUGINS[@]}"; do
  "$PY" "${ROOT}/scripts/release_tools.py" validate-manifest "${ROOT}/plugins/${p}/manifest.json"
  "$PY" "${ROOT}/scripts/release_tools.py" create-distribution-package "$p" \
    --build-dir "${BUILD_DIR}/bin" --output-dir "${ROOT}/dist" --os-label "$OS_LABEL" --arch "$ARCH"
done
