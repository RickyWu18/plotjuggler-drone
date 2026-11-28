import os

from conan import ConanFile

# Single source of truth for the plotjuggler_sdk version (also read by scripts/release_tools.py).
# Conan fetches it from the plotjuggler-conan remote (conan remote add plotjuggler-conan
# https://plotjuggler.jfrog.io/artifactory/api/conan/plotjuggler-conan).
_SDK_VERSION = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "SDK_VERSION")).read().strip()
# build.sh --sdk-path / build.ps1 -SdkPath export PJ_SDK_REF (plotjuggler_sdk/<v>@local/dev) for a local SDK build.
_SDK_REF = os.environ.get("PJ_SDK_REF") or f"plotjuggler_sdk/{_SDK_VERSION}"


# Per-plugin third-party deps. build.sh <plugin>... exports PJ_PLUGINS so only those are fetched;
# unset/empty means every plugin. Add new plugin deps here as plugins are ported.
_PLUGIN_DEPS = {
    "data_load_ardupilot": ["pugixml/1.14"],
    "data_stream_mavlink": ["asio/1.28.2"],
}


class PjDronePluginsConan(ConanFile):
    """Dependency set for the PlotJuggler 4 drone plugins (pj4/)."""

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    requires = (
        _SDK_REF,
        "nlohmann_json/3.12.0",
        "gtest/1.17.0",
    )
    default_options = {"*:shared": False}

    def requirements(self):
        selected = os.environ.get("PJ_PLUGINS", "").replace(";", " ").split()
        for plugin, deps in _PLUGIN_DEPS.items():
            if not selected or plugin in selected:
                for dep in deps:
                    self.requires(dep)
