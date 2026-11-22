#!/usr/bin/env bash
# Package plugins from a finished build into distributable ZIPs (dist/<id>-<version>-<os>-<arch>.zip).
#   ./package.sh [plugin_dir ...]   default: every directory under plugins/
# Build dir per plugin: $BUILD_DIR if set, else build/<plugin> (./build.sh <plugin>), else build/all (./build.sh).
# Stages dist/<id>/ (binary + manifest.json) then zips its contents (at the zip root) into the ZIP.
# Files in plugins/<p>/resources/ (if present) are copied into the ZIP root too.
# Validates manifest.json (incl. min_sdk_required <= SDK_VERSION) and the compiled binary's embedded manifest.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
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
  if [[ -n "${BUILD_DIR:-}" ]]; then bdir="${BUILD_DIR}"
  elif [[ -d "${ROOT}/build/${p}/bin" ]]; then bdir="${ROOT}/build/${p}"
  else bdir="${ROOT}/build/all"; fi
  "$PY" "${ROOT}/scripts/release_tools.py" validate-manifest "${ROOT}/plugins/${p}/manifest.json"
  stage_dir="${ROOT}/dist/$("$PY" -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "${ROOT}/plugins/${p}/manifest.json")"
  rm -rf "$stage_dir"  # drop leftovers from earlier runs so they don't end up in the ZIP
  # release_tools stages dist/<id>/ and prints the zip name on stdout (it never writes the zip itself).
  zip_name="$("$PY" "${ROOT}/scripts/release_tools.py" create-distribution-package "$p" \
    --build-dir "${bdir}/bin" --output-dir "${ROOT}/dist" --os-label "$OS_LABEL" --arch "$ARCH" | tail -n 1)"
  # Optional plugins/<p>/resources/ (xml, etc.) is copied into the ZIP root as-is.
  if [[ -d "${ROOT}/plugins/${p}/resources" ]]; then
    cp -R "${ROOT}/plugins/${p}/resources/." "$stage_dir/"
  fi
  "$PY" - "$stage_dir" "${ROOT}/dist/${zip_name}" <<'PYEOF'
import sys, zipfile
from pathlib import Path
src, dst = Path(sys.argv[1]), Path(sys.argv[2])
dst.unlink(missing_ok=True)
with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as zf:
    for f in sorted(src.rglob("*")):
        if f.is_file():
            zf.write(f, f.relative_to(src).as_posix())
print(f"Created {dst}")
PYEOF
done
