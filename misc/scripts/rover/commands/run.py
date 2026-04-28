"""``rover run`` -- launch the rover executable."""

from __future__ import annotations

import argparse

from rover import log, shell, vulkan
from rover.commands import build as cmd_build
from rover.config import BuildType, ProjectPaths

NAME = "run"
HELP = "Launch the rover executable (builds first if stale)."
DESCRIPTION = (
    "Builds the ``rover`` target (incremental) then executes it. Extra args\n"
    "after a literal ``--`` are forwarded to the engine.\n\n"
    "Examples:\n"
    "    rover run\n"
    "    rover run --validation\n"
    "    rover run -r -- --some-engine-arg foo\n"
)


def add_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--validation", action="store_true",
        help="Set VK_INSTANCE_LAYERS / VK_LAYER_PATH to enable Khronos validation.",
    )
    parser.add_argument(
        "--no-build", action="store_true",
        help="Skip the implicit build step (assumes the binary is current).",
    )
    parser.add_argument(
        "--executable", default="rover",
        help="Override the executable name under bin/<type>/ (default: rover).",
    )
    parser.add_argument(
        "extra", nargs=argparse.REMAINDER,
        help="Args after ``--`` are forwarded verbatim to the engine.",
    )


def _strip_remainder_separator(extra: list[str]) -> list[str]:
    """argparse.REMAINDER keeps the leading ``--``; drop it for cleanliness."""
    if extra and extra[0] == "--":
        return extra[1:]
    return extra


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    if not args.no_build:
        build_args = argparse.Namespace(
            target=args.executable, jobs=None, clean_first=False,
        )
        rc = cmd_build.run(paths, build_type, build_args)
        if rc != 0:
            return rc

    exe = paths.executable(build_type, args.executable)
    if not exe.is_file():
        log.fatal(f"Executable not found: {exe}\n  Did the build succeed?")

    env = vulkan.validation_env() if args.validation else None
    if args.validation:
        log.step("Vulkan validation enabled (VK_INSTANCE_LAYERS, VK_LAYER_PATH)")

    forwarded = _strip_remainder_separator(args.extra)
    log.step(f"Running {exe.relative_to(paths.root)}"
             + (f" {' '.join(forwarded)}" if forwarded else ""))

    result = shell.run([exe, *forwarded], env=env, check=False)
    if result.returncode != 0:
        log.warn(f"Engine exited with code {result.returncode}")
    else:
        log.success("Engine exited cleanly")
    return result.returncode
