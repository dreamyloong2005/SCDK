#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 The Scadek OS Project contributors
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${TARGET:-x86_64-elf}"
PREFIX="${PREFIX:-$ROOT/cross}"
SOURCES="$ROOT/sources"
BUILDS="$ROOT/build"
BINUTILS_VERSION="${BINUTILS_VERSION:-2.46.0}"
GCC_VERSION="${GCC_VERSION:-15.2.0}"
JOBS="${JOBS:-$(nproc)}"

export PATH="$PREFIX/bin:$PATH"

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        return 1
    fi
}

check_deps() {
    local missing=0

    for cmd in curl tar make gcc g++; do
        need_cmd "$cmd" || missing=1
    done

    for cmd in bison flex makeinfo; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            echo "optional command not found: $cmd" >&2
        fi
    done

    if [ "$missing" -ne 0 ]; then
        cat >&2 <<'EOF'

Install the missing packages on Ubuntu/Debian with:

  sudo apt install -y build-essential curl

EOF
        exit 1
    fi
}

download() {
    local url="$1"
    local out="$2"

    if [ -f "$out" ]; then
        return
    fi

    echo "[toolchain] downloading $url"
    curl -fL --retry 3 --retry-delay 2 -o "$out" "$url"
}

extract() {
    local archive="$1"
    local dest="$2"

    if [ -d "$dest" ]; then
        return
    fi

    echo "[toolchain] extracting $(basename "$archive")"
    tar -xf "$archive" -C "$SOURCES"
}

prepare_gcc_prerequisites() {
    local src="$SOURCES/gcc-$GCC_VERSION"

    if [ -d "$src/gmp" ] && [ -d "$src/mpfr" ] && [ -d "$src/mpc" ]; then
        return
    fi

    echo "[toolchain] downloading GCC in-tree prerequisites"
    cd "$src"
    ./contrib/download_prerequisites
}

build_binutils() {
    local src="$SOURCES/binutils-$BINUTILS_VERSION"
    local build="$BUILDS/binutils-$BINUTILS_VERSION-$TARGET"

    if command -v "$TARGET-ld" >/dev/null 2>&1; then
        echo "[toolchain] $TARGET binutils already available"
        return
    fi

    mkdir -p "$build"
    cd "$build"

    echo "[toolchain] configuring binutils $BINUTILS_VERSION"
    "$src/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --with-sysroot \
        --disable-nls \
        --disable-werror

    echo "[toolchain] building binutils"
    make -j"$JOBS"
    make install
}

build_gcc() {
    local src="$SOURCES/gcc-$GCC_VERSION"
    local build="$BUILDS/gcc-$GCC_VERSION-$TARGET"

    if command -v "$TARGET-gcc" >/dev/null 2>&1; then
        echo "[toolchain] $TARGET-gcc already available"
        "$TARGET-gcc" --version | head -1
        return
    fi

    mkdir -p "$build"
    cd "$build"

    echo "[toolchain] configuring GCC $GCC_VERSION"
    "$src/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --disable-nls \
        --enable-languages=c \
        --without-headers \
        --disable-libssp \
        --disable-libquadmath \
        --disable-libgomp

    echo "[toolchain] building GCC"
    make -j"$JOBS" all-gcc
    make -j"$JOBS" all-target-libgcc
    make install-gcc
    make install-target-libgcc
}

main() {
    check_deps

    mkdir -p "$PREFIX" "$SOURCES" "$BUILDS"

    download \
        "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz" \
        "$SOURCES/binutils-$BINUTILS_VERSION.tar.xz"
    download \
        "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz" \
        "$SOURCES/gcc-$GCC_VERSION.tar.xz"

    extract "$SOURCES/binutils-$BINUTILS_VERSION.tar.xz" "$SOURCES/binutils-$BINUTILS_VERSION"
    extract "$SOURCES/gcc-$GCC_VERSION.tar.xz" "$SOURCES/gcc-$GCC_VERSION"
    prepare_gcc_prerequisites

    build_binutils
    build_gcc

    echo "[toolchain] done"
    "$TARGET-gcc" --version | head -1
    "$TARGET-ld" --version | head -1
}

main "$@"
