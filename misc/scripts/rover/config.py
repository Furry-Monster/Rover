"""Project configuration and path discovery.

The CLI auto-detects the engine root by walking up from the script's
location until it finds a ``CMakeLists.txt`` containing
``project(RoverEngine``. All other paths are derived from there, so
the script remains correct even if the working directory differs.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional

from rover import log


# ---------------------------------------------------------------------------
# Build type enum
# ---------------------------------------------------------------------------
class BuildType(str, Enum):
    DEBUG   = "debug"
    RELEASE = "release"

    @property
    def cmake_name(self) -> str:
        """The case-sensitive name CMake expects in CMAKE_BUILD_TYPE."""
        return "Debug" if self is BuildType.DEBUG else "Release"

    @classmethod
    def all(cls) -> list["BuildType"]:
        return [cls.DEBUG, cls.RELEASE]


# ---------------------------------------------------------------------------
# Project paths
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class ProjectPaths:
    """All filesystem paths used by the CLI, anchored at the engine root."""

    root: Path

    # ---- Discovery -------------------------------------------------------
    @classmethod
    def discover(cls, start: Optional[Path] = None) -> "ProjectPaths":
        """Walk up from ``start`` (default: this script) until a CMakeLists.txt
        identifying RoverEngine is found. Aborts the process if not found."""
        here = (start or Path(__file__)).resolve()
        for candidate in [here, *here.parents]:
            if not candidate.is_dir():
                continue
            cmake_file = candidate / "CMakeLists.txt"
            if cmake_file.is_file():
                try:
                    text = cmake_file.read_text(encoding="utf-8", errors="ignore")
                except OSError:
                    continue
                if "project(RoverEngine" in text:
                    log.trace(f"Discovered project root: {candidate}")
                    return cls(root=candidate)

        log.fatal(
            "Could not locate Rover engine root. Run this CLI from inside "
            "the repository, or set ROVER_ROOT environment variable."
        )
        # Unreachable, but keeps type checkers happy.
        raise SystemExit(1)

    # ---- Common paths ----------------------------------------------------
    @property
    def build_dir(self)  -> Path: return self.root / "build"

    @property
    def bin_dir(self)    -> Path: return self.root / "bin"

    @property
    def vendor_dir(self) -> Path: return self.root / "vendor"

    @property
    def docs_dir(self)   -> Path: return self.root / "docs"

    @property
    def misc_dir(self)   -> Path: return self.root / "misc"

    @property
    def scripts_dir(self) -> Path: return self.misc_dir / "scripts"

    @property
    def shaders_dir(self) -> Path: return self.root / "main" / "shaders"

    # ---- Per-build-type ---------------------------------------------------
    def build_dir_for(self, t: BuildType) -> Path:
        return self.build_dir / t.value

    def bin_dir_for(self, t: BuildType) -> Path:
        return self.bin_dir / t.value

    def executable(self, t: BuildType, name: str = "rover") -> Path:
        return self.bin_dir_for(t) / name

    def is_configured(self, t: BuildType) -> bool:
        return (self.build_dir_for(t) / "CMakeCache.txt").is_file()

    def cmake_cache(self, t: BuildType) -> Path:
        return self.build_dir_for(t) / "CMakeCache.txt"

    # ---- Source globs (used by format/lint) ------------------------------
    def first_party_dirs(self) -> list[Path]:
        """Directories containing engine source code (excludes vendor)."""
        return [
            self.root / "core",
            self.root / "drivers",
            self.root / "platform",
            self.root / "services",
            self.root / "modules",
            self.root / "editor",
            self.root / "main",
            self.root / "tests",
        ]
