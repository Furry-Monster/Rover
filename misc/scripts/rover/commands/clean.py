"""``rover clean`` -- remove build artifacts."""

from __future__ import annotations

import argparse
import shutil

from rover import log
from rover.config import BuildType, ProjectPaths

NAME = "clean"
HELP = "Remove build artifacts (build/<type>, optionally bin/<type>)."
DESCRIPTION = (
    "Default: remove build/<type> for the selected build type. With --all, "
    "also remove bin/<type>. With --everything, remove both build/ and bin/ "
    "for every build type and the top-level compile_commands.json."
)


def add_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--all", action="store_true",
        help="Also remove the corresponding bin/<type> output directory.",
    )
    parser.add_argument(
        "--everything", action="store_true",
        help=(
            "Remove build/ AND bin/ for every build type, plus the top-level "
            "compile_commands.json. Ignores --all/-r/-d."
        ),
    )


def _rmtree_if_exists(path) -> bool:
    if not path.exists():
        log.trace(f"skip: {path} (does not exist)")
        return False
    log.step(f"Removing {path}")
    shutil.rmtree(path, ignore_errors=False)
    return True


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    removed_any = False

    if args.everything:
        if paths.build_dir.exists():
            removed_any |= _rmtree_if_exists(paths.build_dir)
        if paths.bin_dir.exists():
            removed_any |= _rmtree_if_exists(paths.bin_dir)
        ccdb = paths.root / "compile_commands.json"
        if ccdb.exists():
            log.step(f"Removing {ccdb.name}")
            ccdb.unlink()
            removed_any = True
    else:
        removed_any |= _rmtree_if_exists(paths.build_dir_for(build_type))
        if args.all:
            removed_any |= _rmtree_if_exists(paths.bin_dir_for(build_type))

    if removed_any:
        log.success("Clean complete")
    else:
        log.info("Nothing to clean")
    return 0
