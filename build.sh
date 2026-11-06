#!/usr/bin/env bash
# Cross-compile dinput8.dll and p99plus_core.dll (32-bit) with mingw-w64.
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v i686-w64-mingw32-g++ >/dev/null; then
  echo "i686-w64-mingw32-g++ not found. Install with: sudo apt install g++-mingw-w64-i686" >&2
  exit 1
fi

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
i686-w64-mingw32-strip build/dinput8.dll build/p99plus_core.dll
echo "Built v$(cat VERSION): $(pwd)/build/dinput8.dll $(pwd)/build/p99plus_core.dll"
