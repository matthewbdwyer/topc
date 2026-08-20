#!/bin/bash
set -e

# Set ROOT_DIR to TOPDIR or the top-level Git directory if TOPDIR is not set
ROOT_DIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
TOPC="${ROOT_DIR}/build/src/topc"
BUILD_DIR="${ROOT_DIR}/build"
RTLIB="${BUILD_DIR}/rtlib"

# Check if TOPCLANG environment variable is set
if [ -z "${TOPCLANG}" ]; then
  echo "error: TOPCLANG env var must be set"
  exit 1
fi

# On macOS, Homebrew clang does not locate the system SDK on its own; without an
# explicit -isysroot the link step fails with "library 'System' not found".
# Resolve the active SDK once (honoring SDKROOT if set).
SYSROOT_FLAGS=""
if [ "$(uname -s)" = "Darwin" ]; then
  SDK="${SDKROOT:-$(xcrun --show-sdk-path 2>/dev/null)}"
  [ -n "${SDK}" ] && SYSROOT_FLAGS="-isysroot ${SDK}"
fi

# Check if the topc executable exists
if [ ! -f "${TOPC}" ]; then
  echo "error: topc was not found"
  exit 1
fi

# Check if the top_rtlib.bc file exists
if [ ! -f "${RTLIB}/top_rtlib.bc" ]; then
  cmake --build "${BUILD_DIR}" --target top_rtlib --parallel
fi

set -- "$@"

SOURCE_FILE=""
OUTPUT_BC=""
previous=""
for arg in "$@"; do
  if [ "${previous}" = "-o" ]; then
    OUTPUT_BC="${arg}"
    previous=""
    continue
  fi
  if [ "${arg}" = "-o" ]; then
    previous="-o"
    continue
  fi
  case "${arg}" in
    *.top)
      SOURCE_FILE="${arg}"
      ;;
  esac
done

if [ -n "${SOURCE_FILE}" ] && [ -z "${OUTPUT_BC}" ]; then
  OUTPUT_BC="${SOURCE_FILE}.bc"
fi

# Execute topc with the provided arguments
"${TOPC}" "$@"

# Link the sanitizer runtime when the IR was instrumented (--san / --asan alias),
# otherwise the instrumented bitcode leaves __asan_init unresolved at link time.
SAN_FLAGS=""
case "$*" in
  *--san*|*--asan*) SAN_FLAGS="-fsanitize=address";;
esac

# Only perform link step if bitcode has been generated
case "$*" in
  *--help*|*--asm*)
    # Do nothing if --help or --asm is present
    ;;
  *)
    if [ -z "${OUTPUT_BC}" ] || [ ! -f "${OUTPUT_BC}" ]; then
      exit 0
    fi
    exe_name="$(basename "${SOURCE_FILE}" .top)"
    "${TOPCLANG}" -w ${SAN_FLAGS} ${SYSROOT_FLAGS} "${OUTPUT_BC}" "${RTLIB}/top_rtlib.bc" -o "${exe_name}"
    ;;
esac