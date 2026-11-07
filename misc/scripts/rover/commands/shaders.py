"""``rover shaders`` -- recompile Slang/HLSL shaders to SPIR-V headers."""

from __future__ import annotations

import argparse

from rover import log
from rover.commands import build as cmd_build
from rover.config import BuildType, ProjectPaths

NAME = "shaders"
HELP = "Recompile Slang/HLSL shaders using the vendored compiler."
DESCRIPTION = (
    "The shader build is wired into CMake via misc/cmake/RoverShader.cmake; "
    "this command bumps the mtime on .slang/.hlsl files under main/builtin_assets/shaders/ "
    "to rebuild them, then triggers an incremental build."
)


def add_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--no-build", action="store_true",
        help="Touch shader sources but skip the subsequent build step.",
    )


def _touch_sources(paths: ProjectPaths) -> int:
    """Bump mtime on Slang/HLSL sources. Returns count touched."""
    if not paths.shaders_dir.is_dir():
        log.warn(f"Shader directory does not exist: {paths.shaders_dir}")
        return 0

    count = 0
    for source in paths.shaders_dir.rglob("*"):
        if not source.is_file() or source.suffix not in (".slang", ".hlsl"):
            continue
        source.touch()
        log.trace(f"touched {source.relative_to(paths.root)}")
        count += 1
    return count


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    log.step("Invalidating shader cache")
    n = _touch_sources(paths)
    log.info(f"Touched {n} Slang/HLSL file(s)")

    if args.no_build:
        return 0

    if not paths.is_configured(build_type):
        log.warn(f"Build tree not configured for {build_type.cmake_name}; "
                 "shader compilation is part of the cmake build step. Run "
                 "``rover configure`` first.")
        return 0

    build_args = argparse.Namespace(target=None, jobs=None, clean_first=False)
    return cmd_build.run(paths, build_type, build_args)
