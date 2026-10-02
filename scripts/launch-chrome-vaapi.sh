#!/usr/bin/env bash

# Google Chrome VAAPI launcher for vaapi-cpu-bridge on Raspberry Pi 5
# This wrapper sets up the environment to use the CPU bridge driver shim
# while keeping GPU acceleration enabled for non-AV1 codecs.

set -euo pipefail

# Detect Chrome installation
CHROME_BIN=""
if command -v google-chrome &>/dev/null; then
    CHROME_BIN="google-chrome"
elif command -v google-chrome-stable &>/dev/null; then
    CHROME_BIN="google-chrome-stable"
elif command -v google-chrome-beta &>/dev/null; then
    CHROME_BIN="google-chrome-beta"
else
    printf "Error: Google Chrome not found. Install google-chrome or google-chrome-stable.\n" >&2
    exit 1
fi

# Path to the built CPU bridge shim
BRIDGE_SO="${BRIDGE_SO:-./build/libva_cpu_bridge.so}"
if [ ! -f "$BRIDGE_SO" ]; then
    printf "Error: Bridge .so not found at %s\n" "$BRIDGE_SO" >&2
    printf "Build it with: bash scripts/build-libva-shim.sh\n" >&2
    exit 1
fi

# Set up VA-API environment variables
export LIBVA_DRIVERS_PATH="$(cd "$(dirname "$BRIDGE_SO")" && pwd):${LIBVA_DRIVERS_PATH:-/usr/lib/arm-linux-gnueabihf/dri}"
export LIBVA_DRIVER_NAME="cpu_bridge"

# Point to the real Mesa VA-API driver (common on RPi 5)
export VAAPI_REAL_DRIVER="${VAAPI_REAL_DRIVER:-/usr/lib/arm-linux-gnueabihf/dri/libva_mesa.so}"

# Enable VA-API video decode in Google Chrome
export VDPAU_DRIVER="va_gl"

printf "[vaapi-cpu-bridge] Launching Google Chrome with software AV1 capability...\n"
printf "[vaapi-cpu-bridge] LIBVA_DRIVERS_PATH=%s\n" "$LIBVA_DRIVERS_PATH"
printf "[vaapi-cpu-bridge] LIBVA_DRIVER_NAME=%s\n" "$LIBVA_DRIVER_NAME"
printf "[vaapi-cpu-bridge] VAAPI_REAL_DRIVER=%s\n" "$VAAPI_REAL_DRIVER"
printf "\n"

# Launch Google Chrome with VA-API and GPU acceleration enabled
# GPU acceleration is kept on for H.264/H.265 and other hardware-supported codecs
# AV1 will be routed through the software path when the browser requests it
exec "$CHROME_BIN" \
    --use-gl=egl \
    --enable-features=VaapiVideoDecoder \
    --enable-accelerated-video-decode \
    --disable-gpu-driver-bug-workarounds \
    --enable-hardware-video-decode \
    "$@"
