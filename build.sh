#!/bin/zsh
# Release build. Replaces the user-folder VST3 when it finishes.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="${ROOT}/build"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --config Release --target MidiToolkit_InstallPlugins -j"$JOBS"
