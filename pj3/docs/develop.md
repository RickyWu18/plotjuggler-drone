# Developer Guide

## Adding a New Plugin

1. Create a new subdirectory, e.g. `DataStreamDrone/`.

2. Add `DataStreamDrone/CMakeLists.txt`, modelled after `PluginTemplate/CMakeLists.txt`:

```cmake
add_library(DataStreamDrone SHARED
    datastream_drone.h
    datastream_drone.cpp)

target_include_directories(DataStreamDrone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PJ_INCLUDE_DIR})

target_compile_definitions(DataStreamDrone PRIVATE
    QT_PLUGIN
    FMT_HEADER_ONLY
    PJ_MAJOR_VERSION=${PJ_MAJOR_VERSION}
    PJ_MINOR_VERSION=${PJ_MINOR_VERSION}
    PJ_PATCH_VERSION=${PJ_PATCH_VERSION})

target_link_libraries(DataStreamDrone PRIVATE
    Qt5::Core Qt5::Widgets Qt5::Xml Qt5::Network
    ${PJ_BASE_LIB} ${PJ_QWT_LIB})

install(TARGETS DataStreamDrone
    RUNTIME DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY}/$<CONFIG>)
```

3. Inherit the appropriate base class and implement its interface:

| Plugin type    | Base class         | IID |
|----------------|--------------------|-----|
| Live streaming | `PJ::DataStreamer` | `facontidavide.PlotJuggler3.DataStreamer` |
| File loading   | `PJ::DataLoader`   | `facontidavide.PlotJuggler3.DataLoader`  |

4. Register the subdirectory in the top-level `CMakeLists.txt`:

```cmake
add_subdirectory(DataStreamDrone)
```

5. Re-configure and build:

```batch
cmake -S src\PlotJuggler-Drone\pj3 -B build\PlotJuggler-Drone
cmake --build build\PlotJuggler-Drone --config Release --target install
```

## Development Notes

- Set `isDebugPlugin()` to `true` during development; change to `false` before release.
- Always hold `mutex()` when writing to `dataMap()` from a background thread.
- Call `emit dataReceived()` after each data push to notify PlotJuggler to refresh.
