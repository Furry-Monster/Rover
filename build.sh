#!/bin/bash
# RoverEngine Build Script Wrapper for Linux

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v python3 &> /dev/null; then
  PYTHON_CMD="python3"
elif command -v python &> /dev/null; then
  PYTHON_CMD="python"
else
  echo "Error: Neither 'python3' nor 'python' command found in PATH." >&2
  exit 1
fi

"$PYTHON_CMD" "${SCRIPT_DIR}/misc/scripts/build.py" "$@"
