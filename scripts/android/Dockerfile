FROM ubuntu:22.04

MAINTAINER xfangfang <xfangfang@126.com>

ENV DEBIAN_FRONTEND=noninteractive

# Build host dependencies. The Android SDK + NDK (r29, pinned by
# buildscripts/include/depinfo.sh) and all native sources are fetched
# automatically by buildscripts/download.sh at build time.
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
    autoconf \
    automake \
    libtool \
    gperf \
    wget \
    unzip \
    openjdk-17-jdk-headless \
    && rm -rf /var/lib/apt/lists/*

ENV JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
ENV PATH="${JAVA_HOME}/bin:${PATH}"

WORKDIR /src
