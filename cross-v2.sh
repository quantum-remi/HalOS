#!/bin/sh

# Exit immediately if a command exits with a non-zero status.
set -e

# Dependencies check
command -v wget >/dev/null 2>&1 || { echo >&2 "wget is required but not installed. Exiting."; exit 1; }
command -v make >/dev/null 2>&1 || { echo >&2 "make is required but not installed. Exiting."; exit 1; }
command -v gcc >/dev/null 2>&1 || { echo >&2 "gcc is required but not installed. Exiting."; exit 1; }

# Versions
BINUTILS_VERSION=2.43
GCC_VERSION=14.2.0
GDB_VERSION=16.1
# Directory setup
mkdir -p cross
cd cross

export PREFIX="$(pwd)"
# export PREFIX="/home/$USER"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

# Core count for parallel processing
CORES=$(nproc || echo 1) # Defaults to 1 if `nproc` is unavailable

# Display settings
echo "PREFIX: $PREFIX"
echo "TARGET: $TARGET"
echo "PATH: $PATH"
echo "Using $CORES cores for compilation."

# Download, extract, and build Binutils
wget "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz"
tar -xf "binutils-$BINUTILS_VERSION.tar.gz"
mkdir -p build-binutils
cd build-binutils

../binutils-$BINUTILS_VERSION/configure --target="$TARGET" --prefix="$PREFIX" --disable-nls
make -j"$CORES"
make install -j"$CORES"
cd ..

# Clean up Binutils source
rm -rf "binutils-$BINUTILS_VERSION" "binutils-$BINUTILS_VERSION.tar.gz"

# Download, extract, and build GCC
wget "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz"
tar -xf "gcc-$GCC_VERSION.tar.gz"
mkdir -p build-gcc
cd build-gcc

../gcc-$GCC_VERSION/configure --target="$TARGET" --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc -j"$CORES"
make all-target-libgcc -j"$CORES"
make install-gcc -j"$CORES"
make install-target-libgcc -j"$CORES"
cd ..

# Clean up GCC source
rm -rf "gcc-$GCC_VERSION" "gcc-$GCC_VERSION.tar.gz"

# Download extract and build GDB 
wget "https://ftp.gnu.org/gnu/gdb/gdb-$GDB_VERSION.tar.gz"
tar -xf "gdb-$GDB_VERSION.tar.gz"
mkdir -p build-gdb
cd build-gdb

../gdb-$GDB_VERSION/configure --target="$TARGET" --prefix="$PREFIX"
make -j"$CORES"
make install

echo "Cross-compilation toolchain setup complete at $PREFIX."

