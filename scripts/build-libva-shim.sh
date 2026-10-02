#!/usr/bin/env bash
set -euo pipefail

# Build script for the libva CPU bridge shim on Raspberry Pi 5
# This compiles the bridge .so that injects AV1 capability while
# keeping the real GPU VA-API driver active.

printf "=== Building vaapi-cpu-bridge .so ===\n\n"

# Check for required tools
for tool in gcc pkg-config; do
    if ! command -v "$tool" &>/dev/null; then
        printf "Error: %s not found. Install build-essential and pkg-config.\n" "$tool" >&2
        exit 1
    fi
done

# Check for required development libraries
printf "Checking for required libraries...\n"

if ! pkg-config --exists libva; then
    printf "Error: libva development files not found.\n" >&2
    printf "Install with: sudo apt install libva-dev\n" >&2
    exit 1
fi

if ! pkg-config --exists libva-drm; then
    printf "Warning: libva-drm development files not found (optional)\n"
fi

# Get the actual compiler flags from pkg-config
CFLAGS=$(pkg-config --cflags libva)
LIBS=$(pkg-config --libs libva) -ldl

printf "CFLAGS: %s\n" "$CFLAGS"
printf "LIBS: %s\n\n" "$LIBS"

# Create build directory
mkdir -p build

# Compile the bridge driver
printf "Compiling libva_cpu_bridge.c...\n"
gcc \
    -shared \
    -fPIC \
    -O2 \
    -Wall \
    -Wextra \
    $CFLAGS \
    -o build/libva_cpu_bridge.so \
    src/libva_cpu_bridge.c \
    $LIBS

if [ -f build/libva_cpu_bridge.so ]; then
    SIZE=$(stat -f%z build/libva_cpu_bridge.so 2>/dev/null || stat -c%s build/libva_cpu_bridge.so 2>/dev/null)
    printf "\n✓ Successfully built: build/libva_cpu_bridge.so (%s bytes)\n\n" "$SIZE"
else
    printf "Error: Build failed\n" >&2
    exit 1
fi

# Print usage instructions
printf "=== Next steps ===\n\n"
printf "1. Set up the environment variables:\n\n"
printf "   export LIBVA_DRIVERS_PATH=\$(pwd)/build\n"
printf "   export LIBVA_DRIVER_NAME=cpu_bridge\n"
printf "   export VAAPI_REAL_DRIVER=/usr/lib/arm-linux-gnueabihf/dri/libva_mesa.so\n\n"

printf "2. Launch Google Chrome with VAAPI support:\n\n"
printf "   bash scripts/launch-chrome-vaapi.sh\n\n"

printf "3. Or manually launch with:\n\n"
printf "   google-chrome \\\\\n"
printf "     --use-gl=egl \\\\\n"
printf "     --enable-features=VaapiVideoDecoder \\\\\n"
printf "     --enable-accelerated-video-decode \\\\\n"
printf "     --disable-gpu-driver-bug-workarounds\n\n"

printf "=== Verification ===\n\n"
printf "Check if the bridge driver loads correctly:\n\n"
printf "   export LIBVA_DRIVERS_PATH=\$(pwd)/build\n"
printf "   export LIBVA_DRIVER_NAME=cpu_bridge\n"
printf "   vainfo 2>&1 | grep -i av1\n\n"
