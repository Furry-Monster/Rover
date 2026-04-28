"""``rover status`` -- print a summary of build state and tooling."""

from __future__ import annotations

import argparse
import os
import sys
from datetime import datetime
from pathlib import Path

from rover import log, shell, vulkan
from rover.config import BuildType, ProjectPaths

NAME = "status"
HELP = "Print build configuration, executable status, and tool versions."
DESCRIPTION = (
    "Read-only health report. Useful for sanity checks at the start of a "
    "session or attached to bug reports."
)


def add_args(parser: argparse.ArgumentParser) -> None:
    pass  # no flags


def _human_mtime(p: Path) -> str:
    if not p.exists():
        return "(missing)"
    ts = datetime.fromtimestamp(p.stat().st_mtime)
    return ts.strftime("%Y-%m-%d %H:%M:%S")


def _tool_version(name: str, version_arg: str = "--version") -> str:
    p = shell.which(name)
    if p is None:
        return f"{name}: not found"
    res = shell.run([p, version_arg], check=False, capture=True)
    if res.returncode != 0:
        return f"{name}: {p} (couldn't query version)"
    first_line = (res.stdout or res.stderr or "").splitlines()[0] if (res.stdout or res.stderr) else ""
    return f"{name}: {first_line.strip()}    [{p}]"


def _print_section(title: str) -> None:
    bar = "─" * (len(title) + 2)
    if sys.stdout.isatty() and "NO_COLOR" not in os.environ:
        print(f"\n{log.BOLD}{title}{log.RESET}\n{bar}")
    else:
        print(f"\n{title}\n{bar}")


def run(paths: ProjectPaths, build_type: BuildType, args: argparse.Namespace) -> int:
    _print_section("Project")
    print(f"  root          : {paths.root}")
    print(f"  selected build: {build_type.value}")

    _print_section("Build trees")
    for t in BuildType.all():
        bd  = paths.build_dir_for(t)
        cfg = "configured" if paths.is_configured(t) else "not configured"
        marker = "✓" if paths.is_configured(t) else "·"
        print(f"  {marker} {t.value:<7} build/{t.value}/  ({cfg})")

    _print_section("Executables")
    for t in BuildType.all():
        for name in ("rover", "rover_tests"):
            exe = paths.executable(t, name)
            mtime = _human_mtime(exe)
            print(f"  {name:<11} ({t.value:<7}) {mtime}    {exe}")

    _print_section("Toolchain")
    print(f"  {_tool_version('cmake')}")
    print(f"  {_tool_version('ninja')}")
    print(f"  {_tool_version('clang-format')}")
    print(f"  {_tool_version('gdb')}")
    sdk = vulkan.find_vulkan_sdk()
    if sdk:
        print(f"  Vulkan SDK   : {sdk}")
    else:
        print(f"  Vulkan SDK   : (not detected; validation may be unavailable)")

    return 0
