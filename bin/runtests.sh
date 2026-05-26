#!/bin/bash
set -e

# Set ROOT_DIR to GITHUB_WORKSPACE or the top-level Git directory if GITHUB_WORKSPACE is not set
ROOT_DIR=${GITHUB_WORKSPACE:-$(git rev-parse --show-toplevel)}
RTLIB_DIR="${ROOT_DIR}/rtlib"
UNIT_TEST_DIR="${ROOT_DIR}/build/test/unit"
SYSTEM_TEST_DIR="${ROOT_DIR}/test/system"

usage() {
  echo "usage: $0 [-h] [-s] [-u]" 1>&2
  echo "run the complete unit and system test suite"
  echo
  echo "-h  display help"
  echo "-s  runs system tests only"
  echo "-u  runs unit tests only"
  echo
  echo "Environment: set TIPC_KEEP_COVERAGE=1 to skip pre-test gcda cleanup"
}

clean_coverage_files() {
  if [ "${TIPC_KEEP_COVERAGE:-0}" = "1" ]; then
    return
  fi
  find "${ROOT_DIR}" -name '*gcda' -delete
}

run_unit_tests() {
  echo "running the unit test suite"
  local status=0
  while IFS= read -r binary; do
    "$binary" || status=1
  done < <(find "${UNIT_TEST_DIR}" -name '*_unit_tests' | sort)
  if [ $status -eq 0 ]; then
    echo "unit test run complete"
  else
    echo "unit tests FAILED"
  fi
  return $status
}

assert_unit_test_dir() {
  if [ ! -d "${UNIT_TEST_DIR}" ]; then
    echo "${UNIT_TEST_DIR} was not found. Please make sure you build the project before running tests."
    exit 1
  fi
}

run_system_tests() {
  # Build the runtime library through CMake so dependency tracking works.
  # Falls back to the legacy build.sh if the build directory is absent.
  if [ -d "${ROOT_DIR}/build" ]; then
    cmake --build "${ROOT_DIR}/build" --target top_rtlib -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
  else
    cd "${RTLIB_DIR}" || exit 1
    if ! ./build.sh; then
      echo "error: could not build the runtime library"
      exit 1
    fi
  fi

  echo "running the system test suite"
  if ! RTLIB="${ROOT_DIR}/build/rtlib" python3 "${SYSTEM_TEST_DIR}/run.py" ${SYSTEM_TEST_ARGS:-}; then
    echo "error while running system tests"
    exit 1
  fi
  echo "system test suite complete"
}

run_system_tests="true"
run_unit_tests="true"
while getopts ":hsu" opt; do
  case "${opt}" in
    h)
      usage
      exit 0
      ;;
    s)
      echo "Preparing to run only the system tests suite"
      run_unit_tests=""
      ;;
    u)
      echo "Preparing to run only the unit tests suite"
      run_system_tests=""
      ;;
    *)
      echo "$0 illegal option"
      usage
      exit 1
      ;;
  esac
done
shift $((OPTIND - 1))

clean_coverage_files

if [ -n "${run_unit_tests}" ]; then
  assert_unit_test_dir
  run_unit_tests
fi

if [ -n "${run_system_tests}" ]; then
  run_system_tests
fi

