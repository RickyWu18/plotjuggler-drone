# Compile from Source

PJ4 plugins are built against the [`plotjuggler_sdk`](https://github.com/PlotJuggler/plotjuggler_sdk)
Conan package. PlotJuggler itself does not need to be built first.

## Setup the Environment

**1. Prerequisites.** CMake >= 3.22, Git, Python 3, and Conan 2 (`pip install conan`, then `conan profile detect`
once), plus a C++20 compiler:

- **Windows:** Visual Studio (C++ workload). Run the scripts from Git Bash.
- **Linux:** GCC 11+ or Clang (Ninja is used automatically when installed).

**2. Clone and add the SDK remote.** The SDK is a Conan package; no sibling checkout is needed.

```bash
git clone --recurse-submodules https://github.com/RickyWu18/plotjuggler-drone.git
conan remote add plotjuggler-conan https://plotjuggler.jfrog.io/artifactory/api/conan/plotjuggler-conan
```

## Build

```bash
cd plotjuggler-drone/pj4
./build.sh            # Release; use --debug for Debug
./test.sh
./package.sh          # optional: zip plugins into dist/
```

`build.sh` runs `conan install` (which fetches `plotjuggler_sdk/<SDK_VERSION>` from the remote), CMake and the build.

To build against an unreleased local SDK checkout instead, pass its path:

```bash
./build.sh --sdk-path /path/to/plotjuggler_sdk
```

The SDK is built into the Conan cache as `plotjuggler_sdk/<its VERSION>@local/dev` (it does not replace the released
package) and is rebuilt on every run. Remove it with `conan remove "plotjuggler_sdk/*@local/dev" -c`.

On Windows you can use PowerShell instead of Git Bash: `./build.ps1` takes the same options
(`-Debug`, `-SdkPath <dir>`, plugin names).

The compiled plugin (`.dll` on Windows, `.so` on Linux) and its `.pjmanifest.json` are placed in `pj4/build/all/bin/` (or `pj4/build/<plugin>/bin/` when built with `./build.sh <plugin>`). Copy both into the plugin
directory scanned by the PJ4 host.
