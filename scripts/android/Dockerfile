FROM ubuntu:22.04

MAINTAINER xfangfang <xfangfang@126.com>

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    pkg-config \
    meson \
    ninja-build \
    nasm \
    yasm \
    python3 \
    python3-pip \
    wget \
    unzip \
    && rm -rf /var/lib/apt/lists/*

# Install Android NDK
ENV ANDROID_NDK_VERSION=r26d
ENV ANDROID_NDK_HOME=/opt/android-ndk

RUN wget -q https://dl.google.com/android/repository/android-ndk-${ANDROID_NDK_VERSION}-linux.zip \
    && unzip -q android-ndk-${ANDROID_NDK_VERSION}-linux.zip -d /opt \
    && mv /opt/android-ndk-${ANDROID_NDK_VERSION} ${ANDROID_NDK_HOME} \
    && rm android-ndk-${ANDROID_NDK_VERSION}-linux.zip

ENV PATH="${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin:${PATH}"

WORKDIR /src
