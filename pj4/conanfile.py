import os

from conan import ConanFile

# Single source of truth for the plotjuggler_sdk version (see scripts/ensure_sdk.sh).
_SDK_VERSION = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "SDK_VERSION")).read().strip()


# Per-plugin third-party deps. build.sh <plugin>... exports PJ_PLUGINS so only those are fetched;
# unset/empty means every plugin. Add new plugin deps here as plugins are ported.
_PLUGIN_DEPS = {
    "data_stream_mavlink": ["asio/1.28.2"],
}


class PjDronePluginsConan(ConanFile):
    """Dependency set for the PlotJuggler 4 drone plugins (pj4/)."""

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    requires = (
        f"plotjuggler_sdk/{_SDK_VERSION}",
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
