#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="${GITHUB_WORKSPACE:-$(cd "${SCRIPT_DIR}/../.." && pwd)}"
TIPC="${ROOT_DIR}/build/src/tipc"
RTLIB="${ROOT_DIR}/rtlib"
SCRATCH_DIR="$(mktemp -d)"

if [ -z "${TIPCLANG}" ]; then
  echo "error: TIPCLANG env var must be set"
  exit 1
fi

# Portable timeout: include the delay in the variable so it can be empty when unavailable.
# On macOS install GNU coreutils (brew install coreutils) to get gtimeout.
TIMEOUT=""
if command -v timeout >/dev/null 2>&1; then
  TIMEOUT="timeout 30"
elif command -v gtimeout >/dev/null 2>&1; then
  TIMEOUT="gtimeout 30"
fi

numtests=0
numfailures=0

initialize_test() {
  echo -n "."
  rm -f "${SCRATCH_DIR}"/*
  ((numtests++))
}

# Compile, link, and run a single selftest program.
# Usage: run_selftest <tipfile> [extra_tipc_flags]
run_selftest() {
  local tipfile="$1"
  local extra_flags="${2:-}"
  local base
  base="$(basename "${tipfile}" .tip)"

  initialize_test
  if ! ${TIPC} ${extra_flags} "${tipfile}"; then
    echo "Compile failure for: ${tipfile}"
    ((numfailures++))
    return
  fi
  ${TIPCLANG} -w "${tipfile}.bc" "${RTLIB}/tip_rtlib.bc" -o "${base}"

  ${TIMEOUT} "./${base}" &>/dev/null
  local exit_code=${?}
  if [ ${exit_code} -eq 124 ]; then
    echo "Timeout for: ${tipfile}"
    ((numfailures++))
  elif [ ${exit_code} -ne 0 ]; then
    echo "Test failure for: ${tipfile}"
    "./${base}"
    ((numfailures++))
  else
    rm "${base}"
  fi
  rm -f "${tipfile}.bc"
}

# Self contained test cases
for i in "${SCRIPT_DIR}"/selftests/*.tip
do
  run_selftest "$i" ""
  run_selftest "$i" "-do"
done

# IO related test cases
for i in "${SCRIPT_DIR}"/iotests/*.expected
do
  initialize_test

  expected="$(basename $i .tip)"
  executable="$(echo $expected | cut -f1 -d-)"
  input="$(echo $expected | cut -f2 -d- | cut -f1 -d.)"

  if ! ${TIPC} "${SCRIPT_DIR}/iotests/$executable.tip"; then
    echo "Compile failure for: iotests/$executable.tip"
    ((numfailures++))
    continue
  fi
  ${TIPCLANG} -w "${SCRIPT_DIR}/iotests/$executable.tip.bc" "${RTLIB}/tip_rtlib.bc" -o "$executable"

  ./${executable} $input >"${SCRIPT_DIR}/iotests/$executable.output" 2>&1

  diff "${SCRIPT_DIR}/iotests/$executable.output" "$i" > "${SCRATCH_DIR}/$executable.diff"

  if [[ -s ${SCRATCH_DIR}/$executable.diff ]]
  then
    echo "Test differences for: $i"
    cat "${SCRATCH_DIR}/$executable.diff"
    ((numfailures++))
  fi

  rm "${SCRIPT_DIR}/iotests/$executable.tip.bc"
  rm "${SCRIPT_DIR}/iotests/$executable.output"
  rm "$executable"
done

# Tests to cover driver logic for error and argument handling
for i in "${SCRIPT_DIR}"/iotests/*error.tip
do
  initialize_test

  ${TIPC} "$i" &>/dev/null
  exit_code=${?}
  if [ ${exit_code} -eq 0 ]; then
    echo "Test failure for: $i (expected error)"
    ((numfailures++))
    rm -f "${SCRIPT_DIR}"/iotests/*error.tip.bc
  fi
done

# System tests for polymorphic type inference
for i in "${SCRIPT_DIR}"/polytests/*.tip
do
  base="$(basename $i .tip)"

  # test optimized program
  initialize_test
  if ! ${TIPC} --pi "$i"; then
    echo "Compile failure for: $i"
    ((numfailures++))
    continue
  fi
  ${TIPCLANG} -w "$i.bc" "${RTLIB}/tip_rtlib.bc" -o "$base"

  ${TIMEOUT} "./${base}" &>/dev/null
  exit_code=${?}
  if [ ${exit_code} -eq 124 ]; then
    echo "Timeout for: $i"
    ((numfailures++))
  elif [ ${exit_code} -ne 0 ]; then
    echo "Test failure for: $i"
    "./${base}"
    ((numfailures++))
  else
    rm "${base}"
  fi
  rm -f "$i.bc"

  ${TIPC} --pp --pt --pi "$i" >"${SCRATCH_DIR}/$base.pppt"
  diff "$i.pppt" "${SCRATCH_DIR}/$base.pppt" >"${SCRATCH_DIR}/$base.diff"
  if [[ -s ${SCRATCH_DIR}/$base.diff ]]
  then
    echo "Test differences for: $i"
    cat "${SCRATCH_DIR}/$base.diff"
    ((numfailures++))
  fi
done

# Tests to cover argument handling
# Test pretty printing and symbol printing.
initialize_test
${TIPC} -pp -ps "${SCRIPT_DIR}/iotests/fib.tip" >"${SCRATCH_DIR}/fib.ppps"
diff "${SCRIPT_DIR}/iotests/fib.ppps" "${SCRATCH_DIR}/fib.ppps" >"${SCRATCH_DIR}/fib.diff"
if [[ -s ${SCRATCH_DIR}/fib.diff ]]
then
  echo "Test differences for: iotests/fib.tip"
  cat "${SCRATCH_DIR}/fib.diff"
  ((numfailures++))
fi

# Test default output file.
initialize_test
input="${SCRIPT_DIR}/iotests/main.tip"
expected="${SCRIPT_DIR}/iotests/main.tip.ll"
${TIPC} --asm "$input"
if [ ! -f "$expected" ]; then
  echo "Did not find expected output, $expected, for input $input"
  ((numfailures++))
fi
rm -f "$expected"

# Test human-readable assembly.
initialize_test
input="${SCRIPT_DIR}/iotests/fib.tip"
output="${SCRATCH_DIR}/fib.tip.ll"
expected="${SCRIPT_DIR}/iotests/fib.tip.ll"
diffed="${SCRATCH_DIR}/fib.diff"
${TIPC} --asm "$input" -o "$output"
diff <(sed -n '4,$p' "$output") <(sed -n '4,$p' "$expected") > "$diffed"
if [ -s "$diffed" ]; then
  echo "Test differences for: $input"
  cat "$diffed"
  ((numfailures++))
fi

# Test call graph.
initialize_test
input="${SCRIPT_DIR}/iotests/fib.tip"
output="${SCRATCH_DIR}/fib.tip.bc"
output_graph="${SCRATCH_DIR}/fib.tip.dot"
expected_graph="${SCRIPT_DIR}/iotests/fib.tip.dot"
diffed_graph="${SCRATCH_DIR}/fib.tip.dot.diff"
${TIPC} --pcg="$output_graph" "$input" -o "$output"
diff "$output_graph" "$expected_graph" > "$diffed_graph"
if [ -s "$diffed_graph" ]; then
  echo "Test differences for: $input"
  cat "$diffed_graph"
  ((numfailures++))
fi

# Test bad input.
initialize_test
nonexistent="$(mktemp -u).tip"

${TIPC} "$nonexistent" &>/dev/null
exit_code=${?}
if [ ${exit_code} -eq 0 ]; then
  echo "Test failure for non-existent input"
  ((numfailures++))
fi

# Type checking at the system level
for i in "${SCRIPT_DIR}"/selftests/*.tip
do
  initialize_test
  base="$(basename $i .tip)"

  ${TIPC} -pp -pt "$i" >"${SCRATCH_DIR}/$base.pppt"
  diff "$i.pppt" "${SCRATCH_DIR}/$base.pppt" >"${SCRATCH_DIR}/$base.diff"
  if [[ -s ${SCRATCH_DIR}/$base.diff ]]
  then
    echo "Test differences for: $i"
    cat "${SCRATCH_DIR}/$base.diff"
    ((numfailures++))
  fi
done

# Test unwritable output file for both ast and call graph printing
initialize_test
outputfile="${SCRIPT_DIR}/iotests/unwritable"
chmod a-w "$outputfile"
input="${SCRIPT_DIR}/iotests/linkedlist.tip"
${TIPC} --pa="$outputfile" "$input" 2>"${SCRATCH_DIR}/unwritable.out"
grep "failed to open" "${SCRATCH_DIR}/unwritable.out" > "${SCRATCH_DIR}/unwritable.grep"
if [[ ! -s ${SCRATCH_DIR}/unwritable.grep ]]; then
  echo "Test differences for: $outputfile"
  ((numfailures++))
fi

initialize_test
outputfile="${SCRIPT_DIR}/iotests/unwritable"
chmod a-w "$outputfile"
input="${SCRIPT_DIR}/iotests/linkedlist.tip"
${TIPC} --pcg="$outputfile" "$input" 2>"${SCRATCH_DIR}/unwritable.out"
grep "failed to open" "${SCRATCH_DIR}/unwritable.out" > "${SCRATCH_DIR}/unwritable.grep"
if [[ ! -s ${SCRATCH_DIR}/unwritable.grep ]]; then
  echo "Test differences for: $outputfile"
  ((numfailures++))
fi

# Logging test 
#   enable logging for a basic smoke test
initialize_test
${TIPC} -pt -log=/dev/null "${SCRIPT_DIR}/selftests/polyfactorial.tip" &>/dev/null

# Test AST visualizer
initialize_test
input="${SCRIPT_DIR}/iotests/linkedlist.tip"
output_graph="${SCRATCH_DIR}/linkedlist.tip.dot"
expected_output="${SCRIPT_DIR}/iotests/linkedlist.tip.dot"
diffed_graph="${SCRATCH_DIR}/linkedlist.tip.dot.diff"
${TIPC} --pa="$output_graph" "$input"
diff "$output_graph" "$expected_output" > "$diffed_graph"
if [ -s "$diffed_graph" ]; then
  echo "Test differences for: $input"
  cat "$diffed_graph"
  ((numfailures++))
fi

initialize_test
input="${SCRIPT_DIR}/selftests/ptr4.tip"
output_graph="${SCRATCH_DIR}/ptr4.tip.dot"
expected_output="${SCRIPT_DIR}/selftests/ptr4.tip.dot"
diffed_graph="${SCRATCH_DIR}/ptr4.tip.dot.diff"
${TIPC} --pa="$output_graph" "$input"
diff "$output_graph" "$expected_output" > "$diffed_graph"
if [ -s "$diffed_graph" ]; then
  echo "Test differences for: $input"
  cat "$diffed_graph"
  ((numfailures++))
fi

# Print out the test results
if [ ${numfailures} -eq "0" ]; then
  echo " all ${numtests} tests passed"
else
  echo " ${numfailures}/${numtests} tests failed"
fi

rm -r "${SCRATCH_DIR}"
[ ${numfailures} -eq 0 ]
