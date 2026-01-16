#!/bin/bash
ROOT_DIR=$(pwd)
ALEPHONE_DIR="$ROOT_DIR"
# Directory setup
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
WIN_LIB_DIR="$SCRIPT_DIR/../lib"

echo "[*] Working in: $ROOT_DIR"
mkdir -p "$WIN_LIB_DIR"

cd "$ALEPHONE_DIR"

echo "[*] Building Standalone Core..."

make clean
make -j8 -f Makefile.libretro 2>&1 | tee build_internal.log

if [ -f "durandal_libretro.so" ]; then
    echo "[*] Copying to Windows lib folder..."
    cp durandal_libretro.so "$WIN_LIB_DIR/durandal_libretro.dll"
    echo "[!] PIVOT_COMPLETE"
elif [ -f "durandal_libretro.dll" ]; then
    echo "[*] Copying to Windows lib folder..."
    cp durandal_libretro.dll "$WIN_LIB_DIR/durandal_libretro.dll"
    echo "[!] BUILD_COMPLETE"
else
    echo "[!] ERROR: Build failed to produce output."
    exit 1
fi