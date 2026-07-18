#!/bin/bash
set -e

# Set ROOT_DIR to TOPDIR or the top-level Git directory if TOPDIR is not set
ROOT_DIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
TOPC="${ROOT_DIR}/build/src/topc"
RTLIB="${ROOT_DIR}/rtlib"

# Check if TOPCLANG environment variable is set
if [ -z "${TOPCLANG}" ]; then
  echo "error: TOPCLANG env var must be set"
  exit 1
fi

# Check if the tipc executable exists
if [ ! -f "${TOPC}" ]; then
  echo "error: tipc was not found"
  exit 1
fi

# Check if the top_rtlib.bc file exists
if [ ! -f "${RTLIB}/top_rtlib.bc" ]; then
  echo "error: top_rtlib.bc was not found"
  exit 1
fi

set -- "$@"

# Execute tipc with the provided arguments
${TOPC} "$@"

# Only perform link step if bitcode has been generated
case "$*" in
  *--help*|*--asm*)
    # Do nothing if --help or --asm is present
    ;;
  *)
    # Perform the linking step
    ${TOPCLANG} -w "${@:$#}.bc" "${RTLIB}/top_rtlib.bc" -o "$(basename "$(basename "${@:$#}" .tip)" .top)"
    ;;
esac