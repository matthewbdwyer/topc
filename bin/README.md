# bin
The bin directory contains useful scripts for working with topc.

## bootstrap.sh
Bootstraps a topc build by installing all of the required dependencies. Further, it sets a number of required environment variables in your `.bashrc`.

This bootstrap script supports the macOS and Ubuntu platforms. It will detect the OS for you.  

_example usage:_
```bash
./bootstrap.sh
```

## runtests.sh
Runs the full suite of tests.

By default the script runs CTest with `--output-on-failure --progress`, then runs the Python system harness. This behavior can be changed at the command line by providing the `-s` switch to run only the system tests, or by providing the `-u` switch to run only CTest.

By default the script also removes stale `*.gcda` files before running tests to avoid gcov merge/corruption warnings. Set `TOPC_KEEP_COVERAGE=1` to skip this cleanup when you intentionally want to preserve coverage artifacts. Set `CTEST_ARGS` or `SYSTEM_TEST_ARGS` to pass extra arguments through to the underlying test tools.

_example usage:_
```bash
# Run only the unit tests.
./runtests.sh -u

# Run system tests with two workers.
TOPCLANG=/path/to/clang ./runtests.sh -s -- -j 2
```

## gencov.sh
Uses [LCOV][1] to generate a code coverage report.  

After a successful test run this script gathers the coverage data. You can view an HTML version of the report at `<topc project root>/coverage.out/index.html`.

_example usage:_
```bash
./gencov.sh
```

## cleancov.sh
Cleans out the old coverage recording files to avoid corruption.

If you rebuild the compiler and only run a subset of the tests, this can cause corruption of the coverage recording files.  To avoid this run this script.  Note that the runtests.sh script cleans the coverage recording files prior to executing the tests, so there is no need to use this script when rerunning the entire test suite.

_example usage:_
```bash
./cleancov.sh
```


## gendocs.sh
Uses [doxygen][2] to generate documentation from the project source code.

After a successful run, you can view the docs at `<topc project root>/docs/html/index.html`.

Additionally, if you have the Python module [coverxygen][3] installed, `gendocs.sh` will output a document coverage report for you. The report strictly checks for documentation coverage of classes.

_example usage:_
```bash
./gendocs.sh
```


## build.sh
Compiles and links a single TOP program.

The script accepts topc command line arguments for a single `.top` source file. The script can be run as is from within the git repository or, if you set the shell variable TOPDIR, you can run it from any directory.

_example usage:_
```bash
./build.sh program.top
```

[1]: http://ltp.sourceforge.net/coverage/lcov.php
[2]: https://www.doxygen.nl/manual/commands.html
[3]: https://github.com/psycofdj/coverxygen
