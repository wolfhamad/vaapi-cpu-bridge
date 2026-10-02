# vaapi-cpu-bridge

A lightweight compatibility bridge for Raspberry Pi 5 that exposes a software AV1 decode capability through VA-API without turning off the rest of the GPU-accelerated path in Chrome.

## Goal

On Raspberry Pi 5, Chrome can often keep its normal GPU/video decode path for H.264/H.265 and other hardware-accelerated codecs, while still advertising a CPU-backed AV1 decode entry to the browser when a software AV1 path is present. The bridge in this repository is designed to help with that workflow.

This is intentionally a prototype / compatibility layer. It does not patch Chromium itself. Instead, it generates a VA-API capability profile and launch configuration that allows software AV1 decode to be used when needed while leaving GPU acceleration enabled for everything else.

## Why this matters

For Raspberry Pi 5, AV1 decode is often not fully hardware-accelerated in the default VA-API driver stack. In that case, Chrome may either:

- refuse to decode AV1 in-browser, or
- fall back to a software path only if the driver reports a compatible decode entry.

The bridge here makes the capability visible to Chrome without disabling the existing GPU-accelerated configuration for other codecs.

## Design

The bridge does three things:

1. Detects the current VA-API profile inventory (`vainfo`).
2. Detects whether a CPU-side AV1 decoder is available (`dav1d`, `ffmpeg`, or a packaged AV1 software decoder).
3. Generates a Chrome-ready config that advertises AV1 decode support while explicitly preserving GPU acceleration for legacy hardware paths.

The generated config can be used as a guide for packaging a custom browser launch or a systemd unit, while the rest of the stack remains enabled.

## Repository layout

- `src/vaapi_cpu_bridge.py` – capability probe + config generator
- `scripts/install.sh` – installs and wires a legacy launch wrapper
- `conf/chrome-vaapi-cpu-bridge.json` – example Chrome capability config
- `docs/raspberry-pi-5.md` – Pi 5-specific notes and launch recommendations

## Quick start

1. Install dependencies:

   ```bash
   sudo apt update
   sudo apt install vainfo ffmpeg mesa-va-drivers
   ```

2. Run the generator:

   ```bash
   python3 src/vaapi_cpu_bridge.py --write conf/chrome-vaapi-cpu-bridge.json
   ```

3. Inspect the generated config:

   ```bash
   cat conf/chrome-vaapi-cpu-bridge.json
   ```

4. Install the helper wrapper:

   ```bash
   sudo bash scripts/install.sh
   ```

5. Launch Chrome with the bridge wrapper:

   ```bash
   chromium-vaapi-cpu-bridge --use-gl=egl --enable-features=VaapiVideoDecoder
   ```

## Important note

This repo is a compatibility and operational aid, not a kernel-level or libva driver replacement. The effective behavior depends on the actual driver stack on your Pi 5 and the browser version you are running.

The goal is to keep hardware acceleration enabled for normal video decode while making a software AV1 decode path visible where the browser can use it on demand.

## Typical Pi 5 setup

Use a standard desktop environment with EGL acceleration enabled, while leaving the main GPU path available for H.264/H.265 in Chromium. The bridge is most useful when Chrome refuses AV1 because it cannot see a valid software AV1 decode profile in the VA API capabilities table.

## Future work

- Add support for libva capability patching via a custom vendor profile
- Add an integrated launcher that preserves GPU acceleration and only soft-routes AV1
- Add a `systemd` unit example for kiosk and desktop workloads
- Add per-browser profile detection for Chrome vs Chromium

## License

MIT
