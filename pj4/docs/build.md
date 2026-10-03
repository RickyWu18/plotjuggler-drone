# Compile from Source

PJ4 plugins are built against the [`plotjuggler_sdk`](https://github.com/PlotJuggler/plotjuggler_sdk)
Conan package. PlotJuggler itself does not need to be built first.

## Windows

### Setup the Environment

**1. Prerequisites.** Visual Studio (C++ workload), CMake >= 3.22, Git, Python 3, and Conan 2
(`pip install conan`, then `conan profile detect` once).

**2. Workspace layout.** The SDK is taken from a sibling checkout when its `VERSION` matches `pj4/SDK_VERSION`;
otherwise it is built from the git tag.

```
plotjuggler4_ws/
├── plotjuggler_sdk/       <- SDK source
└── plotjuggler-drone/     <- this repo
```

```bash
mkdir plotjuggler4_ws && cd plotjuggler4_ws
git clone https://github.com/PlotJuggler/plotjuggler_sdk.git
git clone --recurse-submodules https://github.com/RickyWu18/plotjuggler-drone.git
```

**3. Open a compiler-ready shell.** Use the `Git Bash (VS Dev)` terminal profile from `.vscode/settings.json`
so `cl` is on `PATH`.

### Build

```bash
cd plotjuggler-drone/pj4
./build.sh            # Release; use --debug for Debug
./test.sh
./package.sh          # optional: zip plugins into dist/
```

`build.sh` provides `plotjuggler_sdk/<SDK_VERSION>` in the Conan cache (`FORCE_SDK=1` rebuilds it,
`SDK_LOCAL_DIR=/path` overrides the checkout), then runs `conan install`, CMake and the build.

The compiled `.dll` and its `.pjmanifest.json` are placed in `pj4/build/bin/`. Copy both into the plugin
directory scanned by the PJ4 host.
