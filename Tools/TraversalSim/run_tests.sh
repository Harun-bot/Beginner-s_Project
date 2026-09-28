#!/usr/bin/env bash
# Builds and runs the traversal core tests outside Unreal (Linux, macOS, WSL or Git Bash with g++/clang++).
# Usage: Tools/TraversalSim/run_tests.sh [-v]
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"
PYTHON="${PYTHON:-$(command -v python3 || command -v python)}"
mkdir -p "$BUILD"

"$PYTHON" "$ROOT/Tools/Editor/make_greybox_city.py" --export "$BUILD/greybox_layout.csv" > /dev/null

"${CXX:-g++}" -std=c++20 -O2 -Wall -Wextra -Werror \
    -I"$HERE/shim" -I"$ROOT/Source/WebOfTheCity/Public" \
    "$ROOT/Source/WebOfTheCity/Private/Traversal/TraversalSim.cpp" \
    "$HERE/tests/traversal_tests.cpp" \
    -o "$BUILD/traversal_tests"

"$BUILD/traversal_tests" "$BUILD/greybox_layout.csv" "$@"
