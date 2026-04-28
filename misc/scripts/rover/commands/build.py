"""``rover build`` -- compile the engine via CMake/Ninja."""

from __future__ import annotations

import argparse

from rover import log, shell
from rover.commands import configure as cmd_configure
from rover.config import BuildType, ProjectPaths

NAME = "build"
HELP = "Compile the engine (auto-configures if needed)."
DESCRIPTION = (
    "Runs ``cmake --build build/<type>``. If the build tree is missing, "
    "configure is invoked first with default options. Use ``rover configure`` "
    "explicitly to pass non-default flags."
)


def add_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "-t", "--target", default=None,
        help="Build only the named CMake target (e.g. rover, rover_tests, rover_core).",
    )
    parser.add_argument(
        "-j", "--jobs", type=int, default=None,
        help="Parallel job count (default: ninja's auto-detect).",
    )
    parser.add_argument(
        "--clean-first", action="store_true",
        help="Pass --clean-first to cmake --build (rebuild the chosen target).",
    )


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    if not paths.is_configured(build_type):
        log.info(
            f"Build tree not configured for {build_type.cmake_name}; "
            "running default configure first."
        )
        # Synthesize default configure args so we can reuse its logic.
        defaults = argparse.Namespace(
            reconfigure=False,
            no_editor=False, no_tests=False, no_vulkan=False, werror=False,
            asan=False, ubsan=False, tsan=False, raw_defines=[],
        )
        rc = cmd_configure.run(paths, build_type, defaults)
        if rc != 0:
            return rc

    cmake = shell.require("cmake")
    cmd: list[str] = [str(cmake), "--build", str(paths.build_dir_for(build_type))]
    if args.target:
        cmd += ["--target", args.target]
    if args.jobs is not None:
        cmd += ["-j", str(args.jobs)]
    if args.clean_first:
        cmd.append("--clean-first")

    label = args.target or "all targets"
    log.step(f"Building {label} ({build_type.cmake_name})")
    shell.run(cmd)
    log.success("Build complete")
    return 0
