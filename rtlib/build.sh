#!/bin/sh
if [ -z "${TOPCLANG}" ]; then
  echo error: TOPCLANG env var must be set
  exit 1
fi

${TOPCLANG} -c -emit-llvm top_rtlib.c
