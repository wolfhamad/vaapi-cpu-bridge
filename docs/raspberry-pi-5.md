# Raspberry Pi 5 notes for `vaapi-cpu-bridge`

## Objective

Expose a software AV1 decode capability through the VA-API path while keeping the normal GPU-accelerated decode path enabled for Chromium on Raspberry Pi 5.

## Why software AV1 matters

The default Raspberry Pi 5 VA-API stack may expose only limited decode support. If Chromium cannot see any valid AV1 decode entry at all, AV1 playback may fail even though the GPU is otherwise working normally for H.264/H.265.

This project is meant to help with that specific capability gap without disabling GPU acceleration for other codecs.

## Recommended launch profile

```bash
/usr/bin/chromium \
  --use-gl=egl \
  --enable-features=VaapiVideoDecoder \
  --enable-accelerated-video-decode \
  --disable-gpu-driver-bug-workarounds
```

This keeps the normal GPU path online while allowing the browser to consider a software AV1 decode route.

## Use with this repo

```bash
python3 src/vaapi_cpu_bridge.py --write conf/chrome-vaapi-cpu-bridge.json
sudo bash scripts/install.sh
chromium-vaapi-cpu-bridge --user-data-dir=/tmp/chrome-vaapi-bridge
```

## Observability

Use these commands to confirm the environment is healthy:

```bash
vainfo | grep -i av1 || true
ls /dev/dri
ffmpeg -hide_banner -encoders | grep -i av1 || true
```

## Caveats

- This is a compatibility bridge, not a low-level VA-API driver patch.
- Actual AV1 decode performance depends on the browser build and the system driver support.
- Some browser builds require additional flags or feature gates depending on the exact Chromium version.

## Expected behavior

- H.264/H.265 decode remain on the GPU path.
- AV1 may be routed through a CPU-side codec path when the browser decides it is needed.
- GPU acceleration is not forcibly disabled.
