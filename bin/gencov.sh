#!/bin/sh

# Set ROOT_DIR to GITHUB_WORKSPACE or the top-level Git directory if GITHUB_WORKSPACE is not set
ROOT_DIR=${GITHUB_WORKSPACE:-$(git rev-parse --show-toplevel)}
COV_OUTPUT="${ROOT_DIR}/coverage.info"
HTML_OUTPUT="${ROOT_DIR}/coverage.out"

# Flags to suppress clang/lcov version mismatch warnings on macOS.
LCOV_IGNORE="--ignore-errors inconsistent,unsupported,format,empty"

# Capture coverage data
# shellcheck disable=SC2086
lcov --capture --directory "${ROOT_DIR}" --no-external --output-file "${COV_OUTPUT}" ${LCOV_IGNORE}

# Remove unwanted coverage data
# shellcheck disable=SC2086
lcov --remove "${COV_OUTPUT}" "${ROOT_DIR}/build/*" -o "${COV_OUTPUT}" ${LCOV_IGNORE}
# shellcheck disable=SC2086
lcov --remove "${COV_OUTPUT}" "${ROOT_DIR}/externals/*" -o "${COV_OUTPUT}" ${LCOV_IGNORE}
# shellcheck disable=SC2086
lcov --remove "${COV_OUTPUT}" '*.h' -o "${COV_OUTPUT}" ${LCOV_IGNORE}
# shellcheck disable=SC2086
lcov --remove "${COV_OUTPUT}" '*.hpp' -o "${COV_OUTPUT}" ${LCOV_IGNORE}

# Generate HTML report
# shellcheck disable=SC2086
genhtml "${COV_OUTPUT}" --output-directory "${HTML_OUTPUT}" ${LCOV_IGNORE}

# Print messages to the user
echo "Coverage report has been generated as ${COV_OUTPUT}"
echo "An HTML view of this report is available in ${HTML_OUTPUT}"

