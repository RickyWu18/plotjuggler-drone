#!/usr/bin/env bash
# Build the PJ4 drone plugins. Run from a compiler-ready shell
# (on Windows: the "Git Bash (VS Dev)" terminal profile from .vscode/settings.json).
#   ./build.sh            Release build  -> build/
#   ./build.sh --debug    Debug build    -> build/debug
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_TYPE=Release
BUILD_DIR="${ROOT}/build"
case "${1:-}" in
  "") ;;
  --debug) BUILD_TYPE=Debug; BUILD_DIR="${ROOT}/build/debug" ;;
  *) echo "Usage: ./build.sh [--debug]"; exit 1 ;;
esac
export BUILD_TYPE

"${ROOT}/scripts/ensure_sdk.sh"

conan install "${ROOT}" --output-folder="${BUILD_DIR}" --build=missing \
  -s build_type="${BUILD_TYPE}" -s compiler.cppstd=20

CCACHE_ARGS=()
if command -v ccache >/dev/null 2>&1; then
  CCACHE_ARGS=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi
GEN_ARGS=()
# Conan's toolchain targets the Visual Studio generator on Windows; Ninja elsewhere.
if [[ "${OSTYPE:-}" != msys* && "${OSTYPE:-}" != cygwin* ]] && command -v ninja >/dev/null 2>&1; then
  GEN_ARGS=(-G Ninja)
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" "${GEN_ARGS[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="${BUILD_DIR}/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" "${CCACHE_ARGS[@]}"
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j "$(nproc)"
