"""``rover configure`` -- run CMake to set up the build directory."""

from __future__ import annotations

import argparse

from rover import log, shell
from rover.config import BuildType, ProjectPaths

NAME = "configure"
HELP = "Configure CMake build directory (cmake -S . -B build/<type>)."
DESCRIPTION = (
    "Generates a Ninja build tree under build/<type>. Idempotent -- safe "
    "to run repeatedly. Use --reconfigure to start fresh after changing "
    "options or upgrading CMake."
)


def add_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--reconfigure", action="store_true",
        help="Delete CMakeCache.txt before configuring (forces re-detection).",
    )

    feat = parser.add_argument_group("feature toggles (mapped to ROVER_* options)")
    feat.add_argument("--no-editor", action="store_true",
                     help="Disable the editor target (ROVER_EDITOR=OFF).")
    feat.add_argument("--no-tests", action="store_true",
                     help="Disable the test suite (ROVER_TESTS=OFF).")
    feat.add_argument("--no-vulkan", action="store_true",
                     help="Disable the Vulkan driver (ROVER_VULKAN=OFF).")
    feat.add_argument("--werror", action="store_true",
                     help="Treat warnings as errors (ROVER_WARNINGS_AS_ERRORS=ON).")

    san = parser.add_argument_group("sanitizers (Debug builds only)")
    san.add_argument("--asan",  action="store_true", help="ROVER_ENABLE_ASAN=ON")
    san.add_argument("--ubsan", action="store_true", help="ROVER_ENABLE_UBSAN=ON")
    san.add_argument("--tsan",  action="store_true",
                    help="ROVER_ENABLE_TSAN=ON (mutually exclusive with ASan/UBSan).")

    parser.add_argument(
        "-D", dest="raw_defines", action="append", default=[],
        metavar="VAR=VAL",
        help="Pass a raw -D<VAR>=<VAL> to CMake. Repeatable.",
    )


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    if (args.asan or args.ubsan) and args.tsan:
        log.fatal("--tsan is mutually exclusive with --asan/--ubsan")

    build_dir = paths.build_dir_for(build_type)
    cache = paths.cmake_cache(build_type)
    if args.reconfigure and cache.is_file():
        log.step(f"Deleting {cache.relative_to(paths.root)}")
        cache.unlink()

    cmake = shell.require("cmake", hint="install cmake (>= 3.21)")
    shell.require("ninja", hint="install ninja-build")

    cmake_args: list[str] = [
        str(cmake),
        "-S", str(paths.root),
        "-B", str(build_dir),
        "-G", "Ninja",
        f"-DCMAKE_BUILD_TYPE={build_type.cmake_name}",
    ]
    if args.no_editor:  cmake_args.append("-DROVER_EDITOR=OFF")
    if args.no_tests:   cmake_args.append("-DROVER_TESTS=OFF")
    if args.no_vulkan:  cmake_args.append("-DROVER_VULKAN=OFF")
    if args.werror:     cmake_args.append("-DROVER_WARNINGS_AS_ERRORS=ON")
    if args.asan:       cmake_args.append("-DROVER_ENABLE_ASAN=ON")
    if args.ubsan:      cmake_args.append("-DROVER_ENABLE_UBSAN=ON")
    if args.tsan:       cmake_args.append("-DROVER_ENABLE_TSAN=ON")
    for d in args.raw_defines:
        cmake_args.append(f"-D{d}")

    log.step(f"Configuring {build_type.cmake_name} build at {build_dir.relative_to(paths.root)}")
    shell.run(cmake_args)
    log.success("CMake configuration complete")
    return 0
