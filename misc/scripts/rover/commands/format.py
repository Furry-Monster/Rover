"""``rover format`` -- run clang-format over engine sources."""

from __future__ import annotations

import argparse
from pathlib import Path

from rover import log, shell
from rover.config import BuildType, ProjectPaths

NAME = "format"
HELP = "Run clang-format over engine sources (vendor/ excluded)."
DESCRIPTION = (
    "Walks the first-party source directories (core, drivers, platform, "
    "services, modules, editor, main, tests) and applies clang-format. "
    "Use --check to fail without modifying files (suitable for CI)."
)

# Globs to format. Headers and source; vendor/ is never traversed
# because we only descend into first_party_dirs().
_EXTENSIONS = {".h", ".hpp", ".c", ".cpp", ".cc", ".cxx", ".inl"}


def add_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--check", action="store_true",
        help="Verify formatting without modifying files (exits non-zero on diff).",
    )
    parser.add_argument(
        "--paths", nargs="+", default=None, metavar="PATH",
        help=(
            "Only format the given paths (files or directories, relative to "
            "the engine root). Default: all first-party dirs."
        ),
    )


def _collect_sources(paths: ProjectPaths, restrict: list[Path] | None) -> list[Path]:
    if restrict is None:
        roots = paths.first_party_dirs()
    else:
        roots = restrict
    files: list[Path] = []
    for root in roots:
        if not root.exists():
            continue
        if root.is_file():
            if root.suffix in _EXTENSIONS:
                files.append(root)
            continue
        for p in root.rglob("*"):
            if not p.is_file():
                continue
            if p.suffix not in _EXTENSIONS:
                continue
            files.append(p)
    return sorted(set(files))


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    fmt = shell.require("clang-format", hint="apt install clang-format")

    restrict: list[Path] | None = None
    if args.paths is not None:
        restrict = [(paths.root / p).resolve() for p in args.paths]

    files = _collect_sources(paths, restrict)
    if not files:
        log.warn("No source files matched")
        return 0

    log.step(f"Formatting {len(files)} files ({'check-only' if args.check else 'in-place'})")
    cmd: list[str] = [str(fmt)]
    if args.check:
        cmd += ["--dry-run", "--Werror"]
    else:
        cmd += ["-i"]
    cmd += [str(f) for f in files]

    result = shell.run(cmd, check=False)
    if result.returncode == 0:
        log.success("Formatting clean" if args.check else "Format complete")
    else:
        if args.check:
            log.error("Formatting drift detected -- run ``rover format`` to fix")
        else:
            log.error(f"clang-format exited {result.returncode}")
    return result.returncode
