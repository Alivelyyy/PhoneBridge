#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
LINUX_DIR="$PROJECT_DIR/linux"

echo "=== PhoneBridge Build Script ==="
echo ""

# Build Linux
echo "Building Linux application..."
cd "$LINUX_DIR"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
echo "Linux build complete: $LINUX_DIR/build/phonebridge"
echo ""

# Build Android (if gradlew exists)
ANDROID_DIR="$PROJECT_DIR/android"
if [ -f "$ANDROID_DIR/gradlew" ]; then
    echo "Building Android application..."
    cd "$ANDROID_DIR"
    chmod +x gradlew
    ./gradlew assembleDebug
    echo "Android build complete: $ANDROID_DIR/app/build/outputs/apk/debug/app-debug.apk"
    echo ""
fi

echo "Build complete!"
