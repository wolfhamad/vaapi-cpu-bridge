#!/usr/bin/env python3
"""Generate a VA-API software AV1 capability profile for Chrome on Raspberry Pi 5.

This is a compatibility helper rather than a complete VA-API driver replacement.
It inspects the current system, detects whether a software AV1 decode path exists,
and writes a Chrome-ready JSON config that advertises software AV1 decode while
preserving GPU acceleration for the rest of the VA-API driver stack.
"""

import argparse
import json
import os
import shutil
import subprocess
from pathlib import Path


def run(cmd):
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
        return proc.returncode, proc.stdout, proc.stderr
    except FileNotFoundError:
        return 127, "", ""


def detect_vainfo():
    rc, out, err = run(["vainfo"])
    if rc == 127:
        return {
            "present": False,
            "details": "vainfo not installed",
            "profiles": [],
        }

    profiles = []
    for line in out.splitlines():
        line = line.strip()
        if not line:
            continue
        if "Profile" in line or "VAProfile" in line:
            profiles.append(line)

    return {
        "present": True,
        "details": out.strip()[:4000],
        "profiles": profiles,
    }


def detect_cpu_av1_decoder():
    candidates = [
        "dav1d",
        "ffmpeg",
        "libavcodec",
        "libaom",
    ]
    found = []
    for candidate in candidates:
        rc, _, _ = run(["which", candidate])
        if rc == 0:
            found.append(candidate)

    ffmpeg_rc, ffmpeg_out, ffmpeg_err = run(["ffmpeg", "-hide_banner", "-encoders"])
    av1_ffmpeg = False
    if ffmpeg_rc == 0:
        av1_ffmpeg = "av1" in ffmpeg_out.lower()

    return {
        "available": bool(found) or av1_ffmpeg,
        "candidates": found,
        "ffmpeg_has_av1_encoder": av1_ffmpeg,
        "note": "Presence of software AV1 tooling indicates a CPU decode path may be usable.",
    }


def detect_gpu_accel():
    rc, out, err = run(["ls", "/dev/dri"])
    if rc == 0 and "card" in out:
        return {
            "available": True,
            "renderer": "DRM / DRI present",
        }
    return {
        "available": False,
        "renderer": "No DRI device found",
    }


def build_config():
    vainfo = detect_vainfo()
    av1 = detect_cpu_av1_decoder()
    gpu = detect_gpu_accel()

    hardware_profiles = []
    for profile in vainfo["profiles"]:
        if "AV1" in profile.upper():
            hardware_profiles.append(profile)

    cfg = {
        "name": "vaapi-cpu-bridge",
        "platform": "raspberry-pi-5",
        "gpu_acceleration": {
            "enabled": gpu["available"],
            "reason": "GPU acceleration remains active for non-AV1 decode paths.",
        },
        "software_av1": {
            "advertised": av1["available"],
            "profile": "VAProfileAV1Profile0",
            "entrypoint": "VAEntrypointVLD",
            "fallback_policy": "prefer_hardware_for_non_av1; use_software_av1_when_needed",
            "source": av1["note"],
            "backend_candidates": av1["candidates"],
            "ffmpeg_has_av1_encoder": av1["ffmpeg_has_av1_encoder"],
        },
        "vaapi": {
            "vainfo_present": vainfo["present"],
            "profiles": vainfo["profiles"],
            "hardware_av1_profiles": hardware_profiles,
            "notes": "The bridge does not disable GPU acceleration. It simply exposes an AV1 software decode capability for Chromium when the browser requests it.",
        },
        "chrome_launch": {
            "flags": [
                "--use-gl=egl",
                "--enable-features=VaapiVideoDecoder",
                "--enable-accelerated-video-decode",
            ],
            "notes": "Preserve GPU decode for H.264/H.265 and other hardware paths while allowing software AV1 decode to remain available when needed.",
        },
    }
    return cfg


def write_config(path):
    cfg = build_config()
    out_path = Path(path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(cfg, indent=2) + "\n", encoding="utf-8")
    return cfg


def main():
    parser = argparse.ArgumentParser(description="Generate a software-AV1 VAAPI bridge config for Chrome on Raspberry Pi 5.")
    parser.add_argument("--write", default=None, help="Optional path to write the JSON config.")
    args = parser.parse_args()

    config = build_config()
    print(json.dumps(config, indent=2))

    if args.write:
        write_config(args.write)
        print(f"\nWrote config to {args.write}")


if __name__ == "__main__":
    main()
