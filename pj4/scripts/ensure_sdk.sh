#!/usr/bin/env bash
# Make plotjuggler_sdk/<SDK_VERSION> available in the local Conan cache.
# Order: already cached -> local sibling checkout (if VERSION matches) -> git tag v<SDK_VERSION>.
# Env: SDK_LOCAL_DIR overrides the sibling checkout path; BUILD_TYPE sets the build type.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "${ROOT}/SDK_VERSION")"
REF="plotjuggler_sdk/${VERSION}"
SETTINGS=(-s build_type="${BUILD_TYPE:-Release}" -s compiler.cppstd=20)
LOCAL="${SDK_LOCAL_DIR:-${ROOT}/../../plotjuggler_sdk}"

if [[ "${FORCE_SDK:-}" != "1" ]] && conan cache path "${REF}" >/dev/null 2>&1; then
  echo "ensure_sdk: ${REF} already in the Conan cache (FORCE_SDK=1 to rebuild)"
  exit 0
fi

if [[ -f "${LOCAL}/conanfile.py" && "$(tr -d '[:space:]' < "${LOCAL}/VERSION")" == "${VERSION}" ]]; then
  echo "ensure_sdk: building ${REF} from local tree ${LOCAL}"
  conan create "${LOCAL}" "${SETTINGS[@]}" --build="plotjuggler_sdk/*" --build=missing
  exit 0
fi

echo "ensure_sdk: building ${REF} from git tag v${VERSION}"
TMP="$(mktemp -d)"
git clone --branch "v${VERSION}" --depth 1 https://github.com/PlotJuggler/plotjuggler_sdk.git "${TMP}"
conan create "${TMP}" "${SETTINGS[@]}" --build="plotjuggler_sdk/*" --build=missing
