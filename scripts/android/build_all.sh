#!/bin/bash
set -e

# Build native deps (FFmpeg + libmpv and all transitive deps) for one Android ABI.
# Uses the vendored mpv-android official buildscripts (proven, version-pinned).
#
# Usage: ./build_all.sh [arm64-v8a|armeabi-v7a|x86_64]
#
# Output: scripts/android/buildscripts/prefix/<arch>/lib/libmpv.so + FFmpeg *.so

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
BS_DIR="${SCRIPT_DIR}/buildscripts"
ABI=${1:-arm64-v8a}

# Map Android ABI -> mpv-android buildscripts arch name
case $ABI in
    arm64-v8a)    ARCH=arm64   ;;
    armeabi-v7a)  ARCH=armv7l  ;;
    x86_64)       ARCH=x86_64  ;;
    *)
        echo "Unsupported ABI: $ABI"
        echo "Supported: arm64-v8a, armeabi-v7a, x86_64"
        exit 1
        ;;
esac

echo "=== Building native deps for Android ${ABI} (arch: ${ARCH}) ==="

cd "$BS_DIR"

# Download SDK + NDK + sources if not already present (idempotent).
# IN_CI=1 makes download-sdk.sh skip its own apt installs (workflow installs them).
if [ ! -d deps ] || [ ! -d sdk ]; then
    echo "--- Fetching SDK / NDK / sources ---"
    IN_CI=1 ./download.sh
fi

# Build all dependencies + libmpv for this arch (target: mpv, skip the
# mpv-android app build since wiliwili ships its own Android app).
echo "--- Building mpv (+ deps) for ${ARCH} ---"
./buildall.sh --arch "$ARCH" mpv

PREFIX="${BS_DIR}/prefix/${ARCH}"
echo ""
echo "=== Native build complete for ${ABI} ==="
echo "Prefix: ${PREFIX}"
echo "Shared libs:"
ls -la "${PREFIX}/lib/"*.so 2>/dev/null || echo "  (no .so found)"
