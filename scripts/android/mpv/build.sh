#!/bin/bash
set -e

# Build libmpv for Android NDK
# Usage: ./build.sh [arm64-v8a|armeabi-v7a|x86_64]

ANDROID_NDK_HOME=${ANDROID_NDK_HOME:-/opt/android-ndk}
ANDROID_API=21
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
INSTALL_DIR=${SCRIPT_DIR}/build
SOURCE_DIR=${SCRIPT_DIR}/mpv-source
FFMPEG_DIR=${SCRIPT_DIR}/../ffmpeg/build

ABI=${1:-arm64-v8a}

case $ABI in
    arm64-v8a)
        ARCH=aarch64
        CPU_FAMILY=aarch64
        TARGET=aarch64-linux-android
        ;;
    armeabi-v7a)
        ARCH=arm
        CPU_FAMILY=arm
        TARGET=armv7a-linux-androideabi
        ;;
    x86_64)
        ARCH=x86_64
        CPU_FAMILY=x86_64
        TARGET=x86_64-linux-android
        ;;
    *)
        echo "Unsupported ABI: $ABI"
        echo "Supported: arm64-v8a, armeabi-v7a, x86_64"
        exit 1
        ;;
esac

PREFIX=${INSTALL_DIR}/${ABI}
FFMPEG_PREFIX=${FFMPEG_DIR}/${ABI}
TOOLCHAIN=${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64

echo "Building libmpv for Android ${ABI}..."

# Add NDK toolchain to PATH so cross-compiler can be found
export PATH="${TOOLCHAIN}/bin:${PATH}"

if [ ! -d "$SOURCE_DIR" ]; then
    git clone --depth 1 https://github.com/mpv-player/mpv.git "$SOURCE_DIR"
fi

cd "$SOURCE_DIR"

# Create meson cross file for Android
cat > android-cross-${ABI}.ini << EOF
[binaries]
c = '${TOOLCHAIN}/bin/${TARGET}${ANDROID_API}-clang'
cpp = '${TOOLCHAIN}/bin/${TARGET}${ANDROID_API}-clang++'
ar = '${TOOLCHAIN}/bin/llvm-ar'
ranlib = '${TOOLCHAIN}/bin/llvm-ranlib'
strip = '${TOOLCHAIN}/bin/llvm-strip'
pkgconfig = 'pkg-config'

[host_machine]
system = 'android'
cpu_family = '${CPU_FAMILY}'
cpu = '${ARCH}'
endian = 'little'

[paths]
prefix = '${PREFIX}'

[built-in options]
c_args = ['-DANDROID', '-D__ANDROID__', '--target=${TARGET}${ANDROID_API}', '-fPIC']
cpp_args = ['-DANDROID', '-D__ANDROID__', '--target=${TARGET}${ANDROID_API}', '-fPIC', '-std=c++17']
c_link_args = ['-landroid', '-llog']
cpp_link_args = ['-landroid', '-llog']
EOF

export PKG_CONFIG_PATH=${FFMPEG_PREFIX}/lib/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=${TOOLCHAIN}/sysroot

meson setup build-${ABI} \
    --cross-file android-cross-${ABI}.ini \
    -Dlibmpv=true \
    -Dcplayer=false \
    -Dbuild-date=false \
    -Dgl=enabled \
    -Degl=enabled \
    -Dandroid=enabled \
    -Dmediacodec-fbo=enabled \
    -Dlua=disabled \
    -Djavascript=disabled \
    -Dlibarchive=disabled \
    -Diconv=disabled \
    -Dmanpage-build=disabled \
    -Dhtml-build=disabled \
    -Dpdf-build=disabled

meson compile -C build-${ABI}
meson install -C build-${ABI}

echo "libmpv built successfully for ${ABI}. Output: ${PREFIX}"
