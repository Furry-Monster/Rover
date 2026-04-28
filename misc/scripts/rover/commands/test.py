"""``rover test`` -- run the doctest-based test suite."""

from __future__ import annotations

import argparse

from rover import log, shell
from rover.commands import build as cmd_build
from rover.config import BuildType, ProjectPaths

NAME = "test"
HELP = "Build and run the rover_tests executable."
DESCRIPTION = (
    "Compiles the rover_tests target and runs it. By default the doctest "
    "binary runs all cases; use --filter to select a subset (forwarded as "
    "``--test-case`` to doctest)."
)


def add_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--filter", default=None, metavar="PATTERN",
        help="Run only test cases matching PATTERN (doctest --test-case=).",
    )
    parser.add_argument(
        "--list", action="store_true",
        help="List test cases without running them.",
    )
    parser.add_argument(
        "--no-build", action="store_true",
        help="Skip the implicit build step.",
    )
    parser.add_argument(
        "extra", nargs=argparse.REMAINDER,
        help="Args after ``--`` are forwarded to the doctest binary.",
    )


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    if not args.no_build:
        build_args = argparse.Namespace(
            target="rover_tests", jobs=None, clean_first=False,
        )
        rc = cmd_build.run(paths, build_type, build_args)
        if rc != 0:
            return rc

    exe = paths.executable(build_type, "rover_tests")
    if not exe.is_file():
        log.fatal(f"Test executable not found: {exe}")

    forwarded = args.extra[1:] if args.extra and args.extra[0] == "--" else args.extra
    cmd: list[str] = [str(exe)]
    if args.filter:
        cmd.append(f"--test-case={args.filter}")
    if args.list:
        cmd.append("--list-test-cases")
    cmd.extend(forwarded)

    log.step(f"Running tests ({build_type.cmake_name})")
    result = shell.run(cmd, check=False)

    if result.returncode == 0:
        log.success("All tests passed")
    else:
        log.error(f"Tests failed (exit {result.returncode})")
    return result.returncode
