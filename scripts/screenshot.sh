#!/usr/bin/env bash
# Copyright (c) 2024-2026 Yoonkyu Lee
# SPDX-License-Identifier: MIT
#
# Boot demo.elf, let it draw a few frames, then dump the framebuffer
# via the QEMU monitor and convert PPM -> PNG. Requires `nc` for the
# monitor socket and `python3` for the PNG writer (no Pillow / IM needed).
#
# Usage: scripts/screenshot.sh [out.png]   (default: docs/img/skyline.png)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/docs/img/skyline.png}"
SETTLE_SECS="${SETTLE_SECS:-4}"

mkdir -p "$(dirname "$OUT")"

cd "$ROOT/kernel"
make -s demo.elf >/dev/null

SOCK="$(mktemp -u /tmp/skyline-mon.XXXX)"
PPM="$(mktemp -u /tmp/skyline.XXXX.ppm)"
trap 'rm -f "$SOCK" "$PPM"' EXIT

echo ">> booting demo.elf, settling for ${SETTLE_SECS}s"
qemu-system-riscv64 \
    -machine virt -bios none \
    -kernel demo.elf -m 128M \
    -nographic \
    -monitor unix:"$SOCK",server,nowait \
    -device bochs-display &
QPID=$!

sleep "$SETTLE_SECS"
echo "screendump $PPM" | nc -q 1 -U "$SOCK"
sleep 1
kill "$QPID" 2>/dev/null || true
wait 2>/dev/null || true

python3 "$ROOT/scripts/ppm_to_png.py" "$PPM" "$OUT"
echo ">> wrote $OUT"
