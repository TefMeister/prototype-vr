#!/bin/bash
# Builds and runs every self-test of the Prototype proxy, 32-bit like the game (2026-10-12: one place for all of
# them, since headpose.c now also needs the headset files). No game, no headset needed. Exit 0 = all passed.
set -e
cd "$(dirname "$0")/.."
if command -v i686-w64-mingw32-clang >/dev/null 2>&1; then
    CC=i686-w64-mingw32-clang
else
    CC="$(ls -d "$(cygpath -u "$LOCALAPPDATA")"/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_*/llvm-mingw-*-ucrt-x86_64/bin/i686-w64-mingw32-clang.exe 2>/dev/null | sort | tail -1)"
fi
XR="src/headpose_xr.c src/headpose_xr_math.c -Ithird_party/openxr/include -ldxguid -lversion"
mkdir -p build/tests
"$CC" -O2 -o build/tests/hp.exe test/headpose_selftest.c src/headpose_math.c
"$CC" -O2 -o build/tests/hx.exe test/headpose_xr_selftest.c src/headpose_xr_math.c src/headpose_math.c
"$CC" -O2 -DPLANT_NO_MIRROR=1 -o build/tests/hx_planted.exe test/headpose_xr_selftest.c src/headpose_xr_math.c src/headpose_math.c
"$CC" -O2 -o build/tests/hook.exe test/headpose_hook_selftest.c src/headpose.c src/headpose_math.c src/headpose_markers.c $XR
"$CC" -O2 -o build/tests/mk.exe test/headpose_markers_selftest.c src/headpose_markers.c src/headpose_math.c
./build/tests/hp.exe | tail -1
./build/tests/hx.exe
if ./build/tests/hx_planted.exe >/dev/null; then echo "FAIL: the planted mistake passed"; exit 1; else echo "planted mistake fails, as it must"; fi
./build/tests/hook.exe | tail -1
./build/tests/mk.exe | tail -1
