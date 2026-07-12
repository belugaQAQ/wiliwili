#!/bin/bash
set -e

SCRIPT_DIR=$(dirname "$0")
PROJECT_ROOT=$(realpath "${SCRIPT_DIR}/../..")
ANDROID_DIR="${PROJECT_ROOT}/android"
ABI=${1:-arm64-v8a}

echo "=== wiliwili Android APK Build Script ==="
echo "Project root: ${PROJECT_ROOT}"
echo "Target ABI: ${ABI}"

# Check for Android SDK/NDK
if [ -z "$ANDROID_HOME" ]; then
    echo "ERROR: ANDROID_HOME is not set. Please install Android SDK."
    echo "  export ANDROID_HOME=/path/to/android-sdk"
    exit 1
fi

if [ -z "$ANDROID_NDK_HOME" ]; then
    # Try to find NDK within SDK
    if [ -d "$ANDROID_HOME/ndk" ]; then
        NDK_VERSION=$(ls "$ANDROID_HOME/ndk" | sort -V | tail -1)
        export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/$NDK_VERSION"
        echo "Using NDK: $ANDROID_NDK_HOME"
    else
        echo "ERROR: ANDROID_NDK_HOME is not set and no NDK found in SDK."
        echo "  Install NDK via sdkmanager: sdkmanager 'ndk;26.1.10909125'"
        exit 1
    fi
fi

# Build native dependencies first
echo ""
echo "=== Step 1: Building native dependencies ==="
if [ ! -f "${SCRIPT_DIR}/mpv/build/${ABI}/lib/libmpv.so" ]; then
    echo "Building FFmpeg and mpv for ${ABI}..."
    bash "${SCRIPT_DIR}/build_all.sh" "${ABI}"
else
    echo "Native dependencies already built, skipping."
fi

# Copy prebuilt native libraries to jniLibs
echo ""
echo "=== Step 2: Copying native libraries ==="
JNI_LIBS_DIR="${ANDROID_DIR}/app/libs/${ABI}"
mkdir -p "${JNI_LIBS_DIR}"

# Copy mpv
if [ -f "${SCRIPT_DIR}/mpv/build/${ABI}/lib/libmpv.so" ]; then
    cp "${SCRIPT_DIR}/mpv/build/${ABI}/lib/libmpv.so" "${JNI_LIBS_DIR}/"
    echo "Copied libmpv.so"
fi

# Copy FFmpeg libraries
if [ -d "${SCRIPT_DIR}/ffmpeg/build/${ABI}/lib" ]; then
    cp "${SCRIPT_DIR}/ffmpeg/build/${ABI}/lib/"*.so "${JNI_LIBS_DIR}/" 2>/dev/null || true
    echo "Copied FFmpeg libraries"
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
    # Use system gradle
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
