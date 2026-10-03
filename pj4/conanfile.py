import os

from conan import ConanFile

# Single source of truth for the plotjuggler_sdk version (see scripts/ensure_sdk.sh).
_SDK_VERSION = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "SDK_VERSION")).read().strip()


class PjDronePluginsConan(ConanFile):
    """Dependency set for the PlotJuggler 4 drone plugins (pj4/)."""

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    requires = (
        f"plotjuggler_sdk/{_SDK_VERSION}",
        "nlohmann_json/3.12.0",
        "gtest/1.17.0",
        # Add per-plugin deps here (e.g. mavlink parsing helpers) as plugins are ported.
    )
    default_options = {"*:shared": False}
