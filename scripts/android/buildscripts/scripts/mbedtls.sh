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

# Disable PSA crypto so the TLS layer uses the legacy mbedtls_md_* APIs.
#
# Root cause of "mbedTLS: ssl_init failed" on Android:
# mbedtls 3.6.5 defaults to MBEDTLS_USE_PSA_CRYPTO=ON, which makes the TLS
# layer (ssl_tls.c) perform hash/cipher operations via the PSA Crypto API
# (psa_hash_setup, psa_aead_*, etc.). The call chain is:
#   mbedtls_ssl_setup -> ssl_handshake_init -> mbedtls_ssl_reset_checksum
#     -> psa_hash_setup  (returns PSA_ERROR_BAD_STATE if not initialized)
# PSA operations require psa_crypto_init() to have been called first.
# However, curl 8.4.0's mbedTLS backend (lib/vtls/mbedtls.c) does NOT call
# psa_crypto_init() — that was only added in curl 8.6.0 (commit 8bc5d0f).
# Since cpr fetches curl 8.4.0 via FetchContent, every HTTPS request fails
# at ssl_setup with a non-zero return, and curl logs "mbedTLS: ssl_init failed".
#
# Disabling MBEDTLS_USE_PSA_CRYPTO makes ssl_tls.c use the legacy
# mbedtls_md_* path (which needs no runtime init), fixing the issue.
# MBEDTLS_PSA_CRYPTO_C is left enabled (harmless — just unused by TLS).
./scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO

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
