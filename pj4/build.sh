#!/usr/bin/env bash
# Build the PJ4 drone plugins. Run from a compiler-ready shell
# (on Windows: the "Git Bash (VS Dev)" terminal profile from .vscode/settings.json).
#   ./build.sh                       Release build, all plugins  -> build/all/
#   ./build.sh <plugin> [plugin...]  Only the named plugins (dirs under plugins/)
#                                    -> build/<plugin>[+<plugin>...]/
#   ./build.sh --debug [...]         Debug build                 -> <the above>/debug/
# Plugins land in <build dir>/bin/.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_TYPE=Release
DEBUG_SUFFIX=
PLUGINS=()
for arg in "$@"; do
  case "${arg}" in
    --debug) BUILD_TYPE=Debug; DEBUG_SUFFIX=/debug ;;
    -*) echo "Usage: ./build.sh [--debug] [plugin ...]"; exit 1 ;;
    *)
      [[ -f "${ROOT}/plugins/${arg}/CMakeLists.txt" ]] || { echo "Unknown plugin: ${arg}"; exit 1; }
      PLUGINS+=("${arg}")
      ;;
  esac
done

PJ_PLUGINS=""
SCOPE=all
if [[ ${#PLUGINS[@]} -gt 0 ]]; then
  PJ_PLUGINS="$(IFS=';'; echo "${PLUGINS[*]}")"
  SCOPE="$(IFS='+'; echo "${PLUGINS[*]}")"
fi
BUILD_DIR="${ROOT}/build/${SCOPE}"
BUILD_DIR="${BUILD_DIR}${DEBUG_SUFFIX}"
# Read by conanfile.py (fetch only the selected plugins' deps) and passed to CMake below.
export BUILD_TYPE PJ_PLUGINS

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
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DPJ_PLUGINS="${PJ_PLUGINS}" "${CCACHE_ARGS[@]}"
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j "$(nproc)"
