# Compile from Source

## Windows

### Setup the Environment

**1. Workspace layout.** PlotJuggler must be built and installed before building these plugins. The expected layout is:

```
plotjuggler_ws/
├── src/
│   ├── PlotJuggler/         <- PlotJuggler source
│   └── PlotJuggler-Drone/   <- this repo
├── build/
└── install/                 <- PlotJuggler install prefix (bin/, include/, lib/)
```

```bash
mkdir plotjuggler_ws
cd plotjuggler_ws
git clone -b main https://github.com/PlotJuggler/PlotJuggler.git src/PlotJuggler
git clone --recurse-submodules https://github.com/RickyWu18/plotjuggler-drone.git src/PlotJuggler-Drone
```

**2. Build PlotJuggler.** If PlotJuggler is not yet installed, follow the [PlotJuggler compile guide](https://github.com/PlotJuggler/PlotJuggler/blob/main/COMPILE.md) and install it into `plotjuggler_ws/install/`.

> [!TIP]
> If you experience a compilation error in `ElidingLabel.cpp` when building with vcpkg on Windows, you need to add the `/utf-8` compiler option.
> Update `src/PlotJuggler/CMakeLists.txt` inside the `if(MSVC)` block as shown below:
> ```
> if(WIN32)
>   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -D_USE_MATH_DEFINES")
>   set(CMAKE_WIN32_EXECUTABLE ON)
>   set(GUI_TYPE WIN32)
>   if(MSVC)
>     add_compile_options(/wd4267 /wd4996 /utf-8)
>   endif()
> else()
>   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fPIC -fno-omit-frame-pointer")
>   add_compile_options(-Wno-deprecated-declarations)
> endif()
> ```

### Build

```powershell
set VCPKG_ROOT=C:\path\to\vcpkg

cmake -G "Visual Studio 16 2019" `
      -S src\PlotJuggler-Drone\pj3 `
      -B build\PlotJuggler-Drone `
      -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"

cmake --build build\PlotJuggler-Drone --config Release --target install
```

> **Non-default install path?** Add `-DPJ_INSTALL=C:\path\to\install` if PlotJuggler is not at the default `..\..\install` relative to the source root.

The compiled `.dll` is placed in `build/PlotJuggler-Drone/bin/<config>/` and `build/PlotJuggler/bin/<config>/`. To use the plugin with an installed PlotJuggler, copy the `.dll` manually into the PlotJuggler install prefix (e.g., `install/bin/`); it will be loaded automatically on the next startup.
