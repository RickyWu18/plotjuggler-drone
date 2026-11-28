#!/usr/bin/env bash
# Build the PJ4 drone plugins on Windows (Git Bash), Linux and macOS.
#   ./build.sh                       Release build, all plugins  -> build/all/
#   ./build.sh <plugin> [plugin...]  Only the named plugins (dirs under plugins/)
#                                    -> build/<plugin>[+<plugin>...]/
#   ./build.sh --debug [...]         Debug build                 -> <the above>/debug/
#   ./build.sh --sdk-path DIR [...]  Build against the unreleased plotjuggler_sdk checked out at DIR
#                                    (conan create as plotjuggler_sdk/<DIR/VERSION>@local/dev)
# Plugins land in <build dir>/bin/.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
USAGE="Usage: ./build.sh [--debug] [--sdk-path DIR] [plugin ...]"

BUILD_TYPE=Release
DEBUG_SUFFIX=
SDK_PATH=
PLUGINS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug) BUILD_TYPE=Debug; DEBUG_SUFFIX=/debug ;;
    --sdk-path)
      [[ $# -ge 2 ]] || { echo "${USAGE}"; exit 1; }
      SDK_PATH="$2"; shift
      [[ -f "${SDK_PATH}/conanfile.py" && -f "${SDK_PATH}/VERSION" ]] \
        || { echo "Not a plotjuggler_sdk checkout (conanfile.py/VERSION missing): ${SDK_PATH}"; exit 1; }
      SDK_PATH="$(cd "${SDK_PATH}" && pwd)"
      ;;
    -*) echo "${USAGE}"; exit 1 ;;
    *)
      [[ -f "${ROOT}/plugins/$1/CMakeLists.txt" ]] || { echo "Unknown plugin: $1"; exit 1; }
      PLUGINS+=("$1")
      ;;
  esac
  shift
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

if [[ -n "${SDK_PATH}" ]]; then
  # Local SDK: build it into the cache under @local/dev so it never shadows a released package.
  SDK_VER="$(tr -d '[:space:]' < "${SDK_PATH}/VERSION")"
  export PJ_SDK_REF="plotjuggler_sdk/${SDK_VER}@local/dev"
  echo "build.sh: building ${PJ_SDK_REF} from ${SDK_PATH}"
  conan create "${SDK_PATH}" --user=local --channel=dev -s build_type="${BUILD_TYPE}" -s compiler.cppstd=20 \
    --build="plotjuggler_sdk/*" --build=missing
fi

# Otherwise plotjuggler_sdk/<SDK_VERSION> is resolved from the plotjuggler-conan remote by `conan install`.
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

cmake -S "${ROOT}" -B "${BUILD_DIR}" ${GEN_ARGS[@]+"${GEN_ARGS[@]}"} \
  -DCMAKE_TOOLCHAIN_FILE="${BUILD_DIR}/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DPJ_PLUGINS="${PJ_PLUGINS}" ${CCACHE_ARGS[@]+"${CCACHE_ARGS[@]}"}
# nproc is GNU coreutils (absent on macOS); fall back to sysctl, then to 4.
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j "${JOBS}"
