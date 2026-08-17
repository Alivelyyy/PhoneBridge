#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
LINUX_DIR="$PROJECT_DIR/linux"

echo "=== PhoneBridge Test Script ==="
echo ""

# Build and run tests
echo "Building tests..."
cd "$LINUX_DIR"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
make -j$(nproc) phonebridge_tests

echo ""
echo "Running tests..."
echo ""
./phonebridge_tests

echo ""
echo "Tests complete!"
