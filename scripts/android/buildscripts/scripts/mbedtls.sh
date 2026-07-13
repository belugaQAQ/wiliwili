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

# Build mbedtls as position-independent static libraries so they can be
# linked into the final shared library (libwiliwili.so). Without -fPIC the
# ARM linker fails with "relocation R_ARM_REL32 cannot be used ... recompile
# with -fPIC" because the .a was compiled for a fixed load address.
#
# IMPORTANT: use `make CFLAGS+=` to APPEND -fPIC rather than override the
# Makefile's own CFLAGS. GNU make treats `make CFLAGS=X` as a full override
# that discards every CFLAGS assignment inside the Makefile (including
# -Iinclude, -Wall, -Werror, arch flags, ...). The environment variable
# approach (export CFLAGS=...) has the same problem — it overrides
# unconditional `CFLAGS =` in the Makefile. `CFLAGS+=` appends to whatever
# the Makefile already set, preserving its own flags.
#
# Note: mbedtls 3.6.5's getrandom path (entropy_poll.c) is guarded by
# `defined(__linux__) && defined(__GLIBC__)`, and Android's bionic does
# NOT define __GLIBC__, so mbedtls naturally falls back to /dev/urandom —
# no MBEDTLS_NO_GETRANDOM config option is needed (it doesn't even exist
# in mbedtls 3.6.5's mbedtls_config.h).
make -j$cores no_test CFLAGS+="-fPIC"
make DESTDIR="$prefix_dir" install
