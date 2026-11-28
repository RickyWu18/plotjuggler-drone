# pj4 — PlotJuggler 4 drone plugins

Plugins built against the `plotjuggler_sdk` Conan package (from the `plotjuggler-conan` remote; no sibling
checkout needed). SDK docs: https://github.com/PlotJuggler/plotjuggler_sdk. Reference plugins:
https://github.com/PlotJuggler/pj-official-plugins.

## Build and test

```bash
./build.sh [--debug]   # conan install + cmake + build (PowerShell: ./build.ps1 [-Debug])
./test.sh              # ctest
```

- Bump the SDK by editing `SDK_VERSION`; `conan install` fetches it from the remote.
  To build against an unreleased local SDK checkout: `./build.sh --sdk-path /path/to/plotjuggler_sdk`.
- New plugin: copy `plugins/hello_source`, keep `pj_configure_plugin()`; it is discovered automatically.
  Add its third-party deps to `conanfile.py`.
- Warnings are errors (`PJ_WARNING_FLAGS`). Style: Google clang-format, 120 columns (`pre-commit run -a`).

## Shared code

`../core/` (`pj_drone::core`, pure C++20, no Qt / SDK) holds what both PJ3 and PJ4 plugins can share: the ArduPilot
BIN parser (`SampleSink` interface), parameter/file export helpers and a memory-mapped file. Plugins link it; keep SDK
types out of it. Its tests (`pj_drone_core_test`) run with `./test.sh`.

## Package

`./package.sh` zips built plugins into `dist/` (after `./build.sh`). Manifests need `category` and
`min_sdk_required` (<= `SDK_VERSION`). `scripts/release_tools.py` is a copy of the pj-official-plugins one;
registry submission / release tagging scripts from there were NOT ported.
Extra files (xml, etc.) go in `plugins/<p>/resources/`; they are copied alongside the binary and manifest.
The ZIP has a single top-level `<plugin id>/` entry containing everything.
