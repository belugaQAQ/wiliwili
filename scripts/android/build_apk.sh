#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(realpath "${SCRIPT_DIR}/../..")
ANDROID_DIR="${PROJECT_ROOT}/android"
ABI=${1:-arm64-v8a}

echo "=== wiliwili Android APK Build Script ==="
echo "Project root: ${PROJECT_ROOT}"
echo "Target ABI: ${ABI}"

# Map ABI -> buildscripts arch name (matches build_all.sh)
case $ABI in
    arm64-v8a)    ARCH=arm64   ;;
    armeabi-v7a)  ARCH=armv7l  ;;
    x86_64)       ARCH=x86_64  ;;
    *)
        echo "ERROR: Unsupported ABI: $ABI"
        exit 1
        ;;
esac

# Check for Android SDK (needed by Gradle)
if [ -z "$ANDROID_HOME" ]; then
    echo "ERROR: ANDROID_HOME is not set. Please install Android SDK."
    echo "  export ANDROID_HOME=/path/to/android-sdk"
    exit 1
fi

PREFIX="${SCRIPT_DIR}/buildscripts/prefix/${ARCH}"

# Build native dependencies first if not already built
echo ""
echo "=== Step 1: Building native dependencies ==="
if [ ! -f "${PREFIX}/lib/libmpv.so" ]; then
    echo "Building native deps for ${ABI}..."
    bash "${SCRIPT_DIR}/build_all.sh" "${ABI}"
else
    echo "Native dependencies already built at ${PREFIX}, skipping."
fi

# Copy prebuilt native libraries to jniLibs
echo ""
echo "=== Step 2: Copying native libraries ==="
JNI_LIBS_DIR="${ANDROID_DIR}/app/libs/${ABI}"
mkdir -p "${JNI_LIBS_DIR}"

# Copy libmpv + FFmpeg shared libs (deps are statically linked into libmpv.so)
if [ -d "${PREFIX}/lib" ]; then
    cp "${PREFIX}/lib/"*.so "${JNI_LIBS_DIR}/" 2>/dev/null || true
    echo "Copied shared libs from ${PREFIX}/lib"
else
    echo "ERROR: prefix lib dir not found: ${PREFIX}/lib"
    exit 1
fi

echo "Native libraries in ${JNI_LIBS_DIR}:"
ls -la "${JNI_LIBS_DIR}/"

# Build APK using Gradle
echo ""
echo "=== Step 3: Building APK with Gradle ==="
cd "${ANDROID_DIR}"
chmod +x gradlew 2>/dev/null || true

if [ -f "gradlew" ]; then
    ./gradlew assembleDebug
else
    gradle assembleDebug
fi

echo ""
echo "=== Build Complete ==="
APK_PATH=$(find "${ANDROID_DIR}/app/build/outputs/apk" -name "*.apk" | head -1)
if [ -n "$APK_PATH" ]; then
    echo "APK: ${APK_PATH}"
    echo ""
    echo "Install with: adb install ${APK_PATH}"
else
    echo "WARNING: APK not found. Check build output for errors."
fi
