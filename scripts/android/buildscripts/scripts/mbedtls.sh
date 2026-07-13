#!/bin/bash -e

. ../../include/path.sh

if [ "$1" == "build" ]; then
	true
elif [ "$1" == "clean" ]; then
	make clean
	exit 0
else
	exit 255
fi

$0 clean # separate building not supported, always clean
if [[ "$ndk_triple" == "i686"* ]]; then
	./scripts/config.py unset MBEDTLS_AESNI_C
else
	./scripts/config.py set MBEDTLS_AESNI_C
fi

# mbedtls 3.6.x auto-detects MBEDTLS_ENTROPY_HAVE_GETRANDOM on Linux
# (Android defines __linux__) and calls the libc getrandom() wrapper.
# That wrapper is __INTRODUCED_IN(28) in bionic — on devices running
# API 21..27 the symbol does not exist at runtime, so
# mbedtls_platform_entropy_poll returns MBEDTLS_ERR_ENTROPY_SOURCE_FAILED,
# mbedtls_ctr_drbg_seed fails, mbedtls_ssl_setup fails, and every HTTPS
# request dies with "mbedTLS: ssl_init failed" (which surfaces as
# "WBI签名获取失败" because updateWbiKeys is the first HTTPS call).
# Disable the getrandom path so mbedtls falls back to /dev/urandom,
# which is available on every Android version.
./scripts/config.py set MBEDTLS_NO_GETRANDOM

# Build mbedtls as position-independent static libraries so they can be
# linked into the final shared library (libwiliwili.so). Without -fPIC the
# ARM linker fails with "relocation R_ARM_REL32 cannot be used ... recompile
# with -fPIC" because the .a was compiled for a fixed load address.
#
# IMPORTANT: pass -fPIC via the CFLAGS *environment variable*, NOT as a
# `make CFLAGS=...` command-line argument. GNU make treats a command-line
# variable assignment as a full override, which discards every CFLAGS
# assignment inside the mbedtls Makefile (including -Iinclude, -Wall,
# -Werror, arch flags, ...). With the override, mbedtls_config.h may not
# be found via the documented include path and entropy-related warnings
# are silently dropped, producing a library that compiles but fails
# ssl_init at runtime. Setting CFLAGS in the environment lets the
# Makefile's own `CFLAGS =` / `CFLAGS +=` lines still take effect.
export CFLAGS="${CFLAGS:-} -fPIC"
make -j$cores no_test
make DESTDIR="$prefix_dir" install
