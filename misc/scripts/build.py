#!/usr/bin/env python3
"""RoverEngine build script: cmake configure + build."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROVER_ROOT = Path(__file__).resolve().parent.parent.parent

MULTI_CONFIG_GENERATORS = {"Visual Studio", "Xcode", "Ninja Multi-Config"}


def find_cmake():
    cmake = shutil.which("cmake")
    if not cmake:
        print("Error: cmake not found in PATH.", file=sys.stderr)
        sys.exit(1)
    return cmake


def detect_generator():
    if sys.platform == "win32":
        if shutil.which("ninja"):
            return "Ninja"
        return "Visual Studio 17 2022"
    if shutil.which("ninja"):
        return "Ninja"
    return "Unix Makefiles"


def is_multi_config(generator):
    return any(generator.startswith(prefix) for prefix in MULTI_CONFIG_GENERATORS)


def configure(cmake_exe, build_type, generator, rebuild):
    build_dir = ROVER_ROOT / "build" / build_type.lower()
    if rebuild and build_dir.exists():
        shutil.rmtree(build_dir)
    build_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        cmake_exe,
        "-G", generator,
        "-B", str(build_dir),
        "-S", str(ROVER_ROOT),
    ]
    if not is_multi_config(generator):
        cmd.append(f"-DCMAKE_BUILD_TYPE={build_type}")

    ret = subprocess.run(cmd)
    if ret.returncode != 0:
        sys.exit(ret.returncode)


def build(cmake_exe, build_type, generator, target):
    build_dir = ROVER_ROOT / "build" / build_type.lower()
    cmd = [cmake_exe, "--build", str(build_dir)]

    if is_multi_config(generator):
        cmd.extend(["--config", build_type])

    if target:
        cmd.extend(["--target", target])

    jobs = os.cpu_count() or 4
    cmd.extend(["-j", str(jobs)])

    ret = subprocess.run(cmd)
    if ret.returncode != 0:
        sys.exit(ret.returncode)


def main():
    parser = argparse.ArgumentParser(description="RoverEngine build")
    parser.add_argument(
        "-t", "--type", default="Debug",
        choices=["Debug", "Release"],
        help="Build type (default: Debug)")
    parser.add_argument(
        "-r", "--rebuild", action="store_true",
        help="Remove build dir and reconfigure")
    parser.add_argument(
        "-G", "--generator",
        help="CMake generator override (default: auto-detect)")
    parser.add_argument(
        "target", nargs="?",
        help="CMake target to build (default: all)")
    args = parser.parse_args()

    cmake_exe = find_cmake()
    generator = args.generator or detect_generator()
    configure(cmake_exe, args.type, generator, args.rebuild)
    build(cmake_exe, args.type, generator, args.target)


if __name__ == "__main__":
    main()
