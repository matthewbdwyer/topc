#!/bin/bash
set -e

# Set ROOT_DIR to GITHUB_WORKSPACE or the top-level Git directory if GITHUB_WORKSPACE is not set
ROOT_DIR=${GITHUB_WORKSPACE:-$(git rev-parse --show-toplevel)}
BUILD_DIR="${ROOT_DIR}/build"
SYSTEM_TEST_DIR="${ROOT_DIR}/test/system"

usage() {
  echo "usage: $0 [-h] [-s] [-u] [-- <system-test-args>]" 1>&2
  echo "run the complete unit and system test suite"
  echo
  echo "-h  display help"
  echo "-s  runs system tests only"
  echo "-u  runs CTest/unit tests only"
  echo
  echo "Environment: set TOPC_KEEP_COVERAGE=1 to skip pre-test gcda cleanup"
  echo "Environment: set CTEST_ARGS to override default CTest arguments"
  echo "Environment: set SYSTEM_TEST_ARGS to pass arguments to test/system/run.py"
}

clean_coverage_files() {
  if [ "${TOPC_KEEP_COVERAGE:-0}" = "1" ]; then
    return
  fi
  find "${ROOT_DIR}" -name '*gcda' -delete
}

assert_build_dir() {
  if [ ! -d "${BUILD_DIR}" ]; then
    echo "${BUILD_DIR} was not found. Please configure and build the project before running tests."
    exit 1
  fi
}

run_ctest() {
  echo "running CTest suite"
  # shellcheck disable=SC2086 # CTEST_ARGS intentionally supports multiple args.
  ctest --test-dir "${BUILD_DIR}" ${CTEST_ARGS:---output-on-failure --progress}
  echo "CTest suite complete"
}

run_system_tests() {
  echo "building runtime library"
  cmake --build "${BUILD_DIR}" --target top_rtlib --parallel
  echo "running the system test suite"
  # shellcheck disable=SC2086 # SYSTEM_TEST_ARGS intentionally supports multiple args.
  RTLIB="${BUILD_DIR}/rtlib" python3 "${SYSTEM_TEST_DIR}/run.py" ${SYSTEM_TEST_ARGS:-} "$@"
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

if [ "${1:-}" = "--" ]; then
  shift
fi

assert_build_dir
clean_coverage_files

if [ -n "${run_unit_tests}" ]; then
  run_ctest
fi

if [ -n "${run_system_tests}" ]; then
  run_system_tests "$@"
fi

