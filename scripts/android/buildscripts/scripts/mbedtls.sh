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
# Pass CFLAGS on the make command line so it takes precedence over any
# CFLAGS baked into the mbedtls Makefile.
make -j$cores no_test CFLAGS="${CFLAGS:-} -fPIC"
make DESTDIR="$prefix_dir" install
