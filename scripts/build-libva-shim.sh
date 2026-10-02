#!/usr/bin/env bash
set -euo pipefail

mkdir -p build

gcc \
  -shared \
  -fPIC \
  -O2 \
  -Wall \
  -o build/libva_cpu_bridge.so \
  src/libva_cpu_bridge.c \
  -ldl \
  -lva

printf '\nBuilt: build/libva_cpu_bridge.so\n'
printf 'Use with: export LIBVA_DRIVER_NAME=cpu_bridge\n'
printf 'Set VAAPI_REAL_DRIVER to the actual Mesa driver if needed.\n'
