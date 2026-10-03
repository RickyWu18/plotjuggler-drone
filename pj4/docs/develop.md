# Developer Guide

## Adding a New Plugin

1. Copy the template: `cp -r plugins/hello_source plugins/<new_plugin>`, rename the files and replace the
   template's source and manifest content.

2. Edit `plugins/<new_plugin>/CMakeLists.txt`:

```cmake
add_library(<new_plugin> SHARED <new_plugin>.cpp)
target_compile_options(<new_plugin> PRIVATE ${PJ_WARNING_FLAGS})
target_link_libraries(<new_plugin> PRIVATE plotjuggler_sdk::plugin_sdk)
pj_configure_plugin(<new_plugin>
  FAMILIES        data_source          # data_source | message_parser | toolbox | dialog
  MANIFEST_FILE   ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
  MANIFEST_HEADER generated/<new_plugin>_manifest.hpp
  MANIFEST_VAR    k<NewPlugin>Manifest
)
```

3. Fill in `manifest.json` (required: `id`, `name`, `version`, `category`, `min_sdk_required`).

4. Inherit the appropriate SDK base class and register it:

| Plugin type | Base class | Registration macro |
|---|---|---|
| File loading | `PJ::FileSourceBase` | `PJ_DATA_SOURCE_PLUGIN` |

   Other families: see the SDK's `docs/sdk-utilities.md` and its `plotjuggler-plugin` skill.

5. Plugins under `plugins/` are discovered automatically. Add third-party dependencies to `conanfile.py`.

6. Rebuild: `./build.sh`.

## Development Notes

- Warnings are errors (`-Wall -Wextra -Werror`, `/W4 /WX` on MSVC).
- Fallible operations return `PJ::Expected<T>` / `PJ::Status`.
- Style: Google clang-format, 120 columns (`pre-commit run -a`).
