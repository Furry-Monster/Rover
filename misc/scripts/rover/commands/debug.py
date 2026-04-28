"""``rover debug`` -- launch the engine under gdb."""

from __future__ import annotations

import argparse

from rover import log, shell, vulkan
from rover.commands import build as cmd_build
from rover.config import BuildType, ProjectPaths

NAME = "debug"
HELP = "Launch the engine under gdb."
DESCRIPTION = (
    "Builds (incrementally) then runs the engine inside gdb. Useful flags:\n"
    "  --break SYMBOL   set a breakpoint before run\n"
    "  --batch          run noninteractively (with backtrace on crash)\n"
    "  --validation     enable Vulkan validation layers (recommended)\n"
)


def add_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--validation", action="store_true",
        help="Enable Vulkan validation layers (Khronos).",
    )
    parser.add_argument(
        "--break", dest="break_at", action="append", default=[],
        metavar="SYMBOL",
        help="Set a breakpoint before run. Repeatable.",
    )
    parser.add_argument(
        "--batch", action="store_true",
        help=(
            "Noninteractive: run, print backtrace on signal, and exit. "
            "Useful for capturing crash traces in CI or scripts."
        ),
    )
    parser.add_argument(
        "--no-build", action="store_true",
        help="Skip the implicit build step.",
    )
    parser.add_argument(
        "--executable", default="rover",
        help="Override the executable name under bin/<type>/ (default: rover).",
    )
    parser.add_argument(
        "extra", nargs=argparse.REMAINDER,
        help="Args after ``--`` are forwarded to the engine.",
    )


def _build_gdb_argv(gdb, exe, forwarded, args) -> list[str]:
    cmd: list[str] = [str(gdb)]
    if args.batch:
        cmd += ["-batch"]
        cmd += ["-ex", "handle SIGINT pass nostop"]
    for sym in args.break_at:
        cmd += ["-ex", f"break {sym}"]
    cmd += ["-ex", "run"]
    if args.batch:
        cmd += ["-ex", "bt full"]
        cmd += ["-ex", "thread apply all bt"]
    cmd += ["--args", str(exe), *forwarded]
    return cmd


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    if not args.no_build:
        build_args = argparse.Namespace(
            target=args.executable, jobs=None, clean_first=False,
        )
        rc = cmd_build.run(paths, build_type, build_args)
        if rc != 0:
            return rc

    if build_type is BuildType.RELEASE:
        log.warn("Debugging a Release build -- symbols may be sparse. Consider -d.")

    gdb = shell.require("gdb", hint="install gdb (apt install gdb / brew install gdb)")

    exe = paths.executable(build_type, args.executable)
    if not exe.is_file():
        log.fatal(f"Executable not found: {exe}")

    forwarded = args.extra[1:] if args.extra and args.extra[0] == "--" else args.extra
    env = vulkan.validation_env() if args.validation else None
    if args.validation:
        log.step("Vulkan validation enabled")

    log.step(f"Launching gdb on {exe.relative_to(paths.root)}")
    cmd = _build_gdb_argv(gdb, exe, forwarded, args)
    result = shell.run(cmd, env=env, check=False)
    return result.returncode
