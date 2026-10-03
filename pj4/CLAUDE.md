# pj4 — PlotJuggler 4 drone plugins

Plugins built against `plotjuggler_sdk` (sibling checkout at `../../plotjuggler_sdk`; read its `CLAUDE.md`
and the `plotjuggler-plugin` skill before writing a plugin). Reference plugins: `../../pj-official-plugins`.

## Build and test

```bash
./build.sh [--debug]   # conan install + cmake + build (needs a compiler-ready shell)
./test.sh              # ctest
```

- Bump the SDK by editing `SDK_VERSION`; `scripts/ensure_sdk.sh` provides it in the Conan cache.
- New plugin: copy `plugins/hello_source`, keep `pj_configure_plugin()`; it is discovered automatically.
  Add its third-party deps to `conanfile.py`.
- Warnings are errors (`PJ_WARNING_FLAGS`). Style: Google clang-format, 120 columns (`pre-commit run -a`).
- Windows: use the `Git Bash (VS Dev)` profile (`../../.vscode/settings.json`) so `cl` is on PATH.

## Shared code

`../core/` (`pj_drone::core`, pure C++20, no Qt / SDK) holds what both PJ3 and PJ4 plugins can share: the ArduPilot
BIN parser (`SampleSink` interface), parameter/file export helpers and a memory-mapped file. Plugins link it; keep SDK
types out of it. Its tests (`pj_drone_core_test`) run with `./test.sh`.

## Package

`./package.sh` zips built plugins into `dist/` (after `./build.sh`). Manifests need `category` and
`min_sdk_required` (<= `SDK_VERSION`). `scripts/release_tools.py` is a copy of the pj-official-plugins one;
registry submission / release tagging scripts from there were NOT ported.
