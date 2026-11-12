#!/bin/zsh
# Release build. Always replaces the user-folder VST3 and AU when it finishes.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="${ROOT}/build"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --config Release --target MidiToolkit_InstallPlugins -j"$JOBS"
# Standalone app too (same UI as the plugins; used for visual QA).
cmake --build "$BUILD" --config Release --target MidiToolkit_Standalone -j"$JOBS"
