"""Subprocess helpers with consistent logging and error handling."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Mapping, Optional, Sequence, Union

from rover import log

PathLike = Union[str, Path]


def run(
    argv: Sequence[PathLike],
    *,
    cwd: Optional[Path] = None,
    env: Optional[Mapping[str, str]] = None,
    check: bool = True,
    capture: bool = False,
    inherit_env: bool = True,
) -> subprocess.CompletedProcess:
    """Run a subprocess with logging.

    Parameters
    ----------
    argv      : argv list. Strings or Paths.
    cwd       : optional working directory.
    env       : extra env vars; merged on top of os.environ when ``inherit_env``.
    check     : if True, exit the CLI with the child's exit code on failure.
    capture   : capture stdout/stderr instead of streaming to the terminal.
    inherit_env: when False, only ``env`` is passed (useful for tests).
    """
    cmd = [str(a) for a in argv]
    pretty = " ".join(cmd)
    if cwd:
        log.trace(f"$ {pretty}    (cwd={cwd})")
    else:
        log.trace(f"$ {pretty}")

    full_env: Optional[dict[str, str]]
    if env is not None:
        if inherit_env:
            full_env = dict(os.environ)
            full_env.update(env)
        else:
            full_env = dict(env)
    else:
        full_env = None  # default: child inherits parent env

    try:
        result = subprocess.run(
            cmd,
            cwd=str(cwd) if cwd else None,
            env=full_env,
            check=False,
            text=True,
            capture_output=capture,
        )
    except FileNotFoundError as e:
        log.error(f"Executable not found: {e.filename}")
        if check:
            sys.exit(127)
        # Synthesize a fake CompletedProcess for non-checked callers.
        return subprocess.CompletedProcess(cmd, 127, "", "")

    if check and result.returncode != 0:
        log.error(f"Command exited {result.returncode}: {pretty}")
        if capture:
            if result.stdout:
                sys.stdout.write(result.stdout)
            if result.stderr:
                sys.stderr.write(result.stderr)
        sys.exit(result.returncode)

    return result


def which(name: str) -> Optional[Path]:
    """Locate an executable on PATH, or return None if missing."""
    p = shutil.which(name)
    return Path(p) if p else None


def require(name: str, hint: str = "") -> Path:
    """Locate an executable or fatal-error with an actionable hint."""
    p = which(name)
    if p is None:
        msg = f"Required tool not found on PATH: {name}"
        if hint:
            msg += f"\n  Hint: {hint}"
        log.fatal(msg)
    return p  # type: ignore[return-value]
