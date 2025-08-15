#!/bin/bash

# LoongArch Cross-Compilation Build Script
# Fixes:
# 1. Replaces deprecated _BSD_SOURCE with _DEFAULT_SOURCE
# 2. Ensures proper file paths for compilation
# 3. Handles build dependencies correctly

# Configuration
BUILD_DIR="build/loongarch"
SRC_DIR="src"
CC="loongarch64-linux-gnu-gcc"
AR="loongarch64-linux-gnu-ar"
RANLIB="loongarch64-linux-gnu-ranlib"

# Updated compiler flags (replaced _BSD_SOURCE with _DEFAULT_SOURCE)
CFLAGS="-std=gnu11 -O2 -g -W -Wextra -Werror -D_DEFAULT_SOURCE"
CFLAGS+=" -Wstrict-prototypes -Wmissing-prototypes"
CFLAGS+=" -Wpointer-arith -Wshadow -Wcast-qual -Wwrite-strings"
LDFLAGS="-static"

# Enable debug mode if requested
if [[ "$1" == "--debug" ]]; then
    echo "Enabling debug mode..."
    CFLAGS+=" -O0 -DDEBUG -fno-omit-frame-pointer"
    CFLAGS+=" -fprofile-arcs -ftest-coverage"
    shift
else
    CFLAGS+=" -DNDEBUG"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || exit 1

# Clean build artifacts
clean() {
    echo "Cleaning build files..."
    rm -f *.o *.a *.gcno *.gcov *.gcda vis.sh
    rm -f masstree_vis masstree_st_test masstree_mt_test
    echo "Build directory cleaned: $BUILD_DIR"
}

# Build static library
build_lib() {
    echo "Building Masstree static library..."
    $CC $CFLAGS -c ../../$SRC_DIR/masstree.c -o masstree.o
    $AR cr libmasstree.a masstree.o
    $RANLIB libmasstree.a
}

# Build test programs with proper dependency handling
build_tests() {
    echo "Building test programs..."
    
    # Ensure the library exists first
    if [ ! -f libmasstree.a ]; then
        echo "Error: libmasstree.a not found. Building library first..."
        build_lib
    fi
    
    # Single-threaded test
    $CC $CFLAGS -c ../../$SRC_DIR/tests_st.c -o tests_st.o
    $CC $LDFLAGS tests_st.o libmasstree.a -o masstree_st_test -lpthread
    
    # Multi-threaded test
    $CC $CFLAGS -c ../../$SRC_DIR/tests_mt.c -o tests_mt.o
    $CC $LDFLAGS tests_mt.o libmasstree.a -o masstree_mt_test -lpthread
    
    # Visualization tool
    $CC $CFLAGS -c ../../$SRC_DIR/vis.c -o vis.o
    $CC $LDFLAGS vis.o libmasstree.a -o masstree_vis
    
    # Copy visualization script
    cp ../../$SRC_DIR/vis.sh .
    chmod +x vis.sh
}

# Verify static linking
verify_static() {
    echo -e "\nVerifying static linking:"
    for binary in masstree_st_test masstree_mt_test masstree_vis; do
        if [ -f "$binary" ]; then
            echo -n "$binary: "
            if file "$binary" | grep -q "statically linked"; then
                echo "PASS (statically linked)"
            else
                echo "FAIL (not statically linked)"
                exit 1
            fi
        else
            echo "$binary: NOT FOUND (build failed)"
            exit 1
        fi
    done
}

# Main execution flow
case "$1" in
    clean)
        clean
        exit 0
        ;;
    *)
        build_lib
        build_tests
        verify_static
        echo -e "\nBuild complete. Binaries are in: $BUILD_DIR"
        echo "To run tests, use: ./build.sh test"
        ;;
esac

# Return to project root
cd ../..