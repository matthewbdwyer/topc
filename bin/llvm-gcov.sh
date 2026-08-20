#!/bin/sh

# lcov --gcov-tool wrapper.
#
# The compiler is instrumented with clang's --coverage, which emits gcov-style
# .gcda/.gcno in clang's own format. The system gcov (Apple's /usr/bin/gcov)
# cannot parse that format and fails with "cannot merge previous GCDA file:
# corrupt". clang ships a matching reader as `llvm-cov gcov`, so point lcov at
# this wrapper via `lcov --gcov-tool bin/llvm-gcov.sh`.
#
# The llvm-cov binary is discovered next to $TOPCLANG when set, else from
# $LLVM_COV, else the Homebrew llvm@22 default, else PATH.

if [ -n "${LLVM_COV}" ] && [ -x "${LLVM_COV}" ]; then
  :
elif [ -n "${TOPCLANG}" ] && [ -x "$(dirname "${TOPCLANG}")/llvm-cov" ]; then
  LLVM_COV="$(dirname "${TOPCLANG}")/llvm-cov"
elif [ -x "/opt/homebrew/opt/llvm@22/bin/llvm-cov" ]; then
  LLVM_COV="/opt/homebrew/opt/llvm@22/bin/llvm-cov"
else
  LLVM_COV="$(command -v llvm-cov)"
fi

exec "${LLVM_COV}" gcov "$@"
