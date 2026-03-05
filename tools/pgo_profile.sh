#!/bin/bash
# Profile-Guided Optimization workflow

set -e

BUILD_DIR="build_pgo"
PROFILE_DATA="default.profdata"

echo "=== PGO Step 1: Generate profile ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Release -DPGO_GENERATE=ON
make -j$(nproc)

echo "=== Running with profiling instrumentation ==="
./dreadnought_main &
PID=$!
sleep 5
kill -SIGTERM $PID
wait $PID

echo "=== Merging profile data ==="
llvm-profdata merge -output=../"$PROFILE_DATA" default_*.profraw

cd ..

echo "=== PGO Step 2: Use profile ==="
BUILD_DIR_OPT="build_pgo_opt"
mkdir -p "$BUILD_DIR_OPT"
cd "$BUILD_DIR_OPT"

cmake .. -DCMAKE_BUILD_TYPE=Release -DPGO_USE=ON
make -j$(nproc)

echo "=== PGO complete. Optimized binary: $BUILD_DIR_OPT/dreadnought_main ==="