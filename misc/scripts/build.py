#!/usr/bin/env python3
"""RoverEngine build script: configure, build and test helpers."""

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


def run_cmd(cmd, *, cwd=None):
    ret = subprocess.run(cmd, cwd=cwd)
    if ret.returncode != 0:
        sys.exit(ret.returncode)


def configure(cmake_exe, build_type, generator, rebuild, build_tests):
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
    cmd.append(f"-DROVER_BUILD_TESTS={'ON' if build_tests else 'OFF'}")

    run_cmd(cmd)


def build(cmake_exe, build_type, generator, target, jobs):
    build_dir = ROVER_ROOT / "build" / build_type.lower()
    cmd = [cmake_exe, "--build", str(build_dir)]

    if is_multi_config(generator):
        cmd.extend(["--config", build_type])

    if target:
        cmd.extend(["--target", target])

    cmd.extend(["-j", str(jobs)])
    run_cmd(cmd)


def ctest(build_type, generator, jobs, test_regex=None, repeat_until_fail=1):
    build_dir = ROVER_ROOT / "build" / build_type.lower()

    ctest_cmd = ["ctest", "--test-dir", str(build_dir), "--output-on-failure", "-j", str(jobs)]
    if is_multi_config(generator):
        ctest_cmd.extend(["-C", build_type])
    if test_regex:
        ctest_cmd.extend(["-R", test_regex])
    if repeat_until_fail > 1:
        ctest_cmd.extend(["--repeat", f"until-fail:{repeat_until_fail}"])
    run_cmd(ctest_cmd)


def main():
    parser = argparse.ArgumentParser(description="RoverEngine build/test helper")
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
        "-j", "--jobs", type=int, default=(os.cpu_count() or 4),
        help="Build/test parallel jobs (default: cpu count)")
    parser.add_argument(
        "--tests", action="store_true",
        help="Enable tests in configure step (ROVER_BUILD_TESTS=ON)")
    parser.add_argument(
        "--run-tests", action="store_true",
        help="Run ctest after successful build")
    parser.add_argument(
        "--test-regex",
        help="Only run tests matching ctest regex (-R)")
    parser.add_argument(
        "--repeat-until-fail", type=int, default=1,
        help="Repeat tests until failure N times (ctest --repeat until-fail:N)")
    parser.add_argument(
        "target", nargs="?",
        help="CMake target to build (default: all)")
    args = parser.parse_args()

    if args.repeat_until_fail < 1:
        print("Error: --repeat-until-fail must be >= 1", file=sys.stderr)
        sys.exit(2)
    if args.jobs < 1:
        print("Error: --jobs must be >= 1", file=sys.stderr)
        sys.exit(2)

    cmake_exe = find_cmake()
    generator = args.generator or detect_generator()
    build_tests = args.tests or args.run_tests

    configure(cmake_exe, args.type, generator, args.rebuild, build_tests)
    build(cmake_exe, args.type, generator, args.target, args.jobs)

    if args.run_tests:
        ctest(
            args.type,
            generator,
            jobs=args.jobs,
            test_regex=args.test_regex,
            repeat_until_fail=args.repeat_until_fail,
        )


if __name__ == "__main__":
    main()
