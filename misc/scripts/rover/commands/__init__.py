"""Subcommand modules.

Each subcommand exposes a uniform interface so ``cli.py`` can register
them generically:

    NAME : str                       # subcommand name (e.g. "build")
    HELP : str                       # one-line help shown by argparse
    DESCRIPTION : str | None         # optional longer help (defaults to HELP)
    add_args(parser: ArgumentParser) # populate command-specific args
    run(paths, build_type, args) -> int   # do the work, return exit code

Adding a new subcommand:
  1. Create rover/commands/<name>.py with the four interface members.
  2. Append it to ``ALL_COMMANDS`` in this file.
  3. Document it in ``docs/MISC.md``.
"""

from __future__ import annotations

from rover.commands import (
    build,
    clean,
    configure,
    debug,
    format,
    run,
    shaders,
    status,
    test,
)

# Order here is the order shown in ``rover --help``.
ALL_COMMANDS = [
    configure,
    build,
    clean,
    run,
    debug,
    test,
    format,
    shaders,
    status,
]

__all__ = ["ALL_COMMANDS"]
