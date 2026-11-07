"""Run clang-tidy against first-party translation units in a CMake database."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from rover import log, shell
from rover.config import BuildType, ProjectPaths

NAME = "lint"
HELP = "Run clang-tidy over engine translation units (vendor excluded)."
DESCRIPTION = (
    "Uses the selected build's compile_commands.json and the repository .clang-tidy. "
    "Select .cpp files or directories with --paths; headers are checked through their translation units."
)


def add_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--build-dir", type=Path, help="CMake build directory, relative to the engine root.")
    parser.add_argument("--paths", nargs="+", type=Path, help="First-party source files or directories to check.")
    parser.add_argument("--checks", help="Override the clang-tidy check selection.")
    parser.add_argument("--warnings-as-errors", action="store_true", help="Treat enabled checks' warnings as errors.")
    parser.add_argument("--fix", action="store_true", help="Apply clang-tidy fixes to first-party files.")


def _translation_units(paths: ProjectPaths, database: Path, selected: list[Path] | None) -> list[Path]:
    records = json.loads(database.read_text(encoding="utf-8"))
    if not isinstance(records, list):
        raise ValueError("Compilation database must contain an array")
    roots = [root.resolve() for root in paths.first_party_dirs()]
    requested = [(paths.root / path).resolve() for path in selected] if selected else []
    sources: set[Path] = set()
    for record in records:
        working = Path(record["directory"])
        if not working.is_absolute():
            working = database.parent / working
        source = (working / record["file"]).resolve()
        if source.suffix not in (".c", ".cpp", ".cc", ".cxx") or not source.is_file():
            continue
        if not any(source.is_relative_to(root) for root in roots):
            continue
        if requested and not any(source == path or source.is_relative_to(path) for path in requested):
            continue
        sources.add(source)
    return sorted(sources)


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    build_dir = (paths.root / args.build_dir).resolve() if args.build_dir else paths.build_dir_for(build_type)
    database = build_dir / "compile_commands.json"
    try:
        files = _translation_units(paths, database, args.paths)
    except (OSError, ValueError, KeyError, TypeError) as error:
        log.error(f"Cannot read {database}: {error}. Configure the build first.")
        return 1
    if not files:
        log.error("No first-party translation units matched the compilation database and --paths.")
        return 1
    tidy = shell.require("clang-tidy", hint="install clang-tools-extra (Fedora) or clang-tidy (Debian/Ubuntu)")
    directories = "|".join(re.escape(root.name) for root in paths.first_party_dirs())
    headers = rf"^{re.escape(str(paths.root.resolve()))}/({directories})/"
    options = [str(tidy), "--quiet", "-p", str(build_dir), f"--header-filter={headers}"]
    if args.checks is not None:
        options.append(f"--checks={args.checks}")
    if args.warnings_as_errors:
        options.append("--warnings-as-errors=*")
    if args.fix:
        options.append("--fix")
    result = 0
    for source in files:
        log.step(f"Linting {source.relative_to(paths.root)}")
        status = shell.run([*options, str(source)], cwd=paths.root, check=False).returncode
        if status and not result:
            result = status
    return result
