#!/bin/bash
set -e

# Build all Android dependencies (FFmpeg + libmpv)
# Usage: ./build_all.sh [arm64-v8a|armeabi-v7a|x86_64]

SCRIPT_DIR=$(dirname "$0")
ABI=${1:-arm64-v8a}

echo "=== Building FFmpeg for Android (${ABI}) ==="
bash ${SCRIPT_DIR}/ffmpeg/build.sh ${ABI}

echo "=== Building libmpv for Android (${ABI}) ==="
bash ${SCRIPT_DIR}/mpv/build.sh ${ABI}

echo "=== All Android dependencies built successfully ==="
echo "FFmpeg: ${SCRIPT_DIR}/ffmpeg/build/${ABI}"
echo "libmpv: ${SCRIPT_DIR}/mpv/build/${ABI}"
