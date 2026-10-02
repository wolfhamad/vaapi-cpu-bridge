#!/usr/bin/env bash
set -euo pipefail

# Install helper files for a Chromium launch wrapper that keeps GPU acceleration enabled
# while exposing software AV1 decode capability through a bridge profile.

PREFIX="/usr/local"
BIN_DIR="$PREFIX/bin"
ETC_DIR="/etc/chrome-vaapi-cpu-bridge"

sudo mkdir -p "$BIN_DIR" "$ETC_DIR"

cat > /tmp/chromium-vaapi-cpu-bridge <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

export LIBVA_DRIVERS_PATH="${LIBVA_DRIVERS_PATH:-/usr/lib/arm-linux-gnueabihf/dri}"
export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-0}"
export CHROME_EXTRA_ARGS="${CHROME_EXTRA_ARGS:-}"

exec /usr/bin/chromium \
  --use-gl=egl \
  --enable-features=VaapiVideoDecoder \
  --enable-accelerated-video-decode \
  --disable-gpu-driver-bug-workarounds \
  ${CHROME_EXTRA_ARGS} \
  "$@"
EOF

sudo install -m 0755 /tmp/chromium-vaapi-cpu-bridge "$BIN_DIR/chromium-vaapi-cpu-bridge"

cat > "$ETC_DIR/chrome-vaapi-cpu-bridge.json" <<'EOF'
{
  "name": "vaapi-cpu-bridge",
  "platform": "raspberry-pi-5",
  "gpu_acceleration": {
    "enabled": true,
    "reason": "Keep GPU decode active for H.264/H.265 and other codecs."
  },
  "software_av1": {
    "advertised": true,
    "profile": "VAProfileAV1Profile0",
    "entrypoint": "VAEntrypointVLD",
    "fallback_policy": "prefer_hardware_for_non_av1; use_software_av1_when_needed"
  },
  "chrome_launch": {
    "flags": [
      "--use-gl=egl",
      "--enable-features=VaapiVideoDecoder",
      "--enable-accelerated-video-decode"
    ]
  }
}
EOF

printf '\nInstalled helper and config.\n'
printf 'Use: chromium-vaapi-cpu-bridge --user-data-dir=/tmp/chrome-bridge\n\n'
