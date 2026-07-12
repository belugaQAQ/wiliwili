#!/bin/bash
set -e

# Build FFmpeg for Android NDK
# Usage: ./build.sh [arm64-v8a|armeabi-v7a|x86_64]

ANDROID_NDK_HOME=${ANDROID_NDK_HOME:-/opt/android-ndk}
ANDROID_API=21
INSTALL_DIR=$(dirname "$0")/build
SOURCE_DIR=$(dirname "$0")/ffmpeg-source

# Support multiple ABIs
ABI=${1:-arm64-v8a}

case $ABI in
    arm64-v8a)
        ARCH=aarch64
        TARGET=aarch64-linux-android
        ;;
    armeabi-v7a)
        ARCH=arm
        TARGET=armv7a-linux-androideabi
        ;;
    x86_64)
        ARCH=x86_64
        TARGET=x86_64-linux-android
        ;;
    *)
        echo "Unsupported ABI: $ABI"
        echo "Supported: arm64-v8a, armeabi-v7a, x86_64"
        exit 1
        ;;
esac

CC=${TARGET}${ANDROID_API}-clang
TOOLCHAIN=${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64
PREFIX=${INSTALL_DIR}/${ABI}

echo "Building FFmpeg for Android ${ABI}..."

# Add NDK toolchain to PATH so cross-compiler can be found
export PATH="${TOOLCHAIN}/bin:${PATH}"

if [ ! -d "$SOURCE_DIR" ]; then
    git clone --depth 1 https://github.com/FFmpeg/FFmpeg.git "$SOURCE_DIR"
fi

cd "$SOURCE_DIR"

./configure \
    --prefix=${PREFIX} \
    --enable-cross-compile \
    --cross-prefix=${TARGET}- \
    --cc=${CC} \
    --target-os=android \
    --arch=${ARCH} \
    --sysroot=${TOOLCHAIN}/sysroot \
    --enable-gpl \
    --enable-version3 \
    --enable-shared \
    --disable-static \
    --disable-programs \
    --disable-doc \
    --disable-avdevice \
    --enable-swscale \
    --enable-swresample \
    --disable-encoders \
    --disable-muxers \
    --disable-demuxers \
    --enable-demuxer=matroska,mov,flv,hls,aac,mp3,wav \
    --disable-parsers \
    --enable-parser=h264,hevc,vp9,av1,aac,opus \
    --disable-decoders \
    --enable-decoder=h264,hevc,vp8,vp9,av1,aac,opus,flac,mp3 \
    --enable-network \
    --disable-protocols \
    --enable-protocol=file,http,https,hls,tcp,tls,crypto \
    --enable-mediacodec \
    --enable-jni \
    --enable-small \
    --disable-debug

make -j$(nproc)
make install

echo "FFmpeg built successfully for ${ABI}. Output: ${PREFIX}"
