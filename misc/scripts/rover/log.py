"""Lightweight terminal logger with ANSI colors.

Honors ``NO_COLOR`` environment variable and downgrades to plain text
when stderr is not a TTY (e.g. piped to a file or CI logs).
"""

from __future__ import annotations

import os
import sys
from typing import TextIO

# ---------------------------------------------------------------------------
# ANSI escape codes
# ---------------------------------------------------------------------------
RESET   = "\033[0m"
BOLD    = "\033[1m"
DIM     = "\033[2m"
RED     = "\033[31m"
GREEN   = "\033[32m"
YELLOW  = "\033[33m"
BLUE    = "\033[34m"
MAGENTA = "\033[35m"
CYAN    = "\033[36m"
GRAY    = "\033[90m"

# ---------------------------------------------------------------------------
# Module state
# ---------------------------------------------------------------------------
_use_color: bool = sys.stderr.isatty() and os.environ.get("NO_COLOR") is None
_verbose:   bool = False


def set_verbose(v: bool) -> None:
    """Enable or disable trace-level logging."""
    global _verbose
    _verbose = v


def is_verbose() -> bool:
    return _verbose


def _emit(prefix: str, color: str, msg: str, stream: TextIO = sys.stderr) -> None:
    if _use_color:
        stream.write(f"{color}{prefix}{RESET} {msg}\n")
    else:
        stream.write(f"{prefix} {msg}\n")
    stream.flush()


# ---------------------------------------------------------------------------
# Public API -- one function per severity
# ---------------------------------------------------------------------------
def trace(msg: str) -> None:
    if _verbose:
        _emit("[trace]", GRAY, msg)


def info(msg: str) -> None:
    _emit("[info ]", BLUE, msg)


def step(msg: str) -> None:
    """Mark a major workflow step in bold cyan."""
    _emit("==>", BOLD + CYAN, msg)


def warn(msg: str) -> None:
    _emit("[warn ]", YELLOW, msg)


def error(msg: str) -> None:
    _emit("[error]", RED, msg)


def success(msg: str) -> None:
    _emit("[ok   ]", GREEN, msg)


def fatal(msg: str, code: int = 1) -> None:
    """Print a fatal error and terminate the process."""
    _emit("[FATAL]", BOLD + RED, msg)
    sys.exit(code)
