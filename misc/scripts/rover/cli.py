"""Argparse-based CLI dispatcher."""

from __future__ import annotations

import argparse
import sys
from typing import Optional

from rover import __version__, log
from rover.commands import ALL_COMMANDS
from rover.config import BuildType, ProjectPaths


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="rover",
        description=(
            "Rover engine developer CLI. Wraps CMake / Ninja / clang-format /\n"
            "gdb with consistent ergonomics. Subcommands work from any cwd\n"
            "inside the repository."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-V", "--version",
        action="version",
        version=f"rover-cli {__version__}",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Print trace-level diagnostics (every subprocess invocation).",
    )

    # ---- Build-type selection (global, applies to every subcommand) ----
    build_grp = parser.add_mutually_exclusive_group()
    build_grp.add_argument(
        "-d", "--debug",
        action="store_const", dest="build_type", const=BuildType.DEBUG,
        help="Operate on the Debug build (the default).",
    )
    build_grp.add_argument(
        "-r", "--release",
        action="store_const", dest="build_type", const=BuildType.RELEASE,
        help="Operate on the Release build.",
    )
    parser.set_defaults(build_type=BuildType.DEBUG)

    sub = parser.add_subparsers(
        dest="command", required=True, metavar="<command>",
    )

    for mod in ALL_COMMANDS:
        sp = sub.add_parser(
            mod.NAME,
            help=mod.HELP,
            description=getattr(mod, "DESCRIPTION", mod.HELP),
            formatter_class=argparse.RawDescriptionHelpFormatter,
        )
        mod.add_args(sp)
        sp.set_defaults(_handler=mod.run)

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    log.set_verbose(args.verbose)

    paths = ProjectPaths.discover()
    log.trace(f"Project root: {paths.root}")
    log.trace(f"Build type:   {args.build_type.value}")

    handler = args._handler
    try:
        return int(handler(paths, args.build_type, args))
    except KeyboardInterrupt:
        log.warn("Interrupted by user")
        return 130


if __name__ == "__main__":
    sys.exit(main())
