#!/bin/bash
# Thin shim — delegates to the Python test runner.
# All flags are forwarded as-is.
exec python3 "$(dirname "$0")/run.py" "$@"
