#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== PhoneBridge Package Script ==="
echo ""

cd "$PROJECT_DIR"

echo "Building Debian package..."
dpkg-buildpackage -us -uc

echo ""
echo "Package built successfully!"
echo "Output: $(ls -1 ../phonebridge_*.deb 2>/dev/null || echo 'Check parent directory')"
