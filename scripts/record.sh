#!/usr/bin/env bash
# Copyright (c) 2024-2026 Yoonkyu Lee
# SPDX-License-Identifier: MIT
#
# Boot demo.elf, sample the framebuffer N times via the QEMU monitor,
# and stitch the frames into both an animated GIF and an mp4.
#
# Defaults:
#   FRAMES=40   INTERVAL=0.5  -> 20 s of recording
#   SETTLE=4    seconds to wait after boot before the first frame
#   FPS=2       playback fps (matches INTERVAL so timing is real-time)
#
# Output:
#   docs/img/skyline.gif
#   docs/img/skyline.mp4
#
# Requires: qemu-system-riscv64, nc (netcat-openbsd), python3, ffmpeg.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FRAMES="${FRAMES:-40}"
INTERVAL="${INTERVAL:-0.5}"
SETTLE="${SETTLE:-4}"
FPS="${FPS:-2}"
GIF_OUT="${GIF_OUT:-$ROOT/docs/img/skyline.gif}"
MP4_OUT="${MP4_OUT:-$ROOT/docs/img/skyline.mp4}"

mkdir -p "$(dirname "$GIF_OUT")" "$(dirname "$MP4_OUT")"

cd "$ROOT/kernel"
make -s demo.elf >/dev/null

WORK="$(mktemp -d)"
SOCK="$(mktemp -u /tmp/skyline-mon.XXXX)"
trap 'rm -rf "$WORK" "$SOCK"' EXIT

echo ">> boot demo.elf, settle ${SETTLE}s before first frame"
qemu-system-riscv64 \
    -machine virt -bios none \
    -kernel demo.elf -m 128M \
    -nographic \
    -monitor unix:"$SOCK",server,nowait \
    -device bochs-display &
QPID=$!

# QEMU may take a fraction of a second to bind the unix socket.
for _ in 1 2 3 4 5; do
    [ -S "$SOCK" ] && break
    sleep 0.2
done

sleep "$SETTLE"

echo ">> sampling $FRAMES frames at ${INTERVAL}s intervals"
python3 - "$SOCK" "$WORK" "$FRAMES" "$INTERVAL" <<'PY'
import socket, sys, time
sock_path, work_dir, frames, interval = sys.argv[1:]
frames = int(frames); interval = float(interval)

s = socket.socket(socket.AF_UNIX)
s.connect(sock_path)
s.settimeout(1.0)
# Drain the QEMU monitor banner.
try:
    while True:
        if not s.recv(4096): break
except socket.timeout:
    pass

for i in range(frames):
    p = f"{work_dir}/f{i:03d}.ppm"
    s.sendall(f"screendump {p}\n".encode())
    time.sleep(interval)
s.close()
PY

echo ">> stop qemu"
kill "$QPID" 2>/dev/null || true
wait 2>/dev/null || true

echo ">> convert PPM -> PNG (pure-stdlib)"
for ppm in "$WORK"/*.ppm; do
    png="${ppm%.ppm}.png"
    python3 "$ROOT/scripts/ppm_to_png.py" "$ppm" "$png" >/dev/null
done

echo ">> ffmpeg encode GIF (palettegen for clean colors)"
ffmpeg -y -loglevel error \
    -framerate "$FPS" -i "$WORK/f%03d.png" \
    -vf "split[a][b];[a]palettegen[p];[b][p]paletteuse" \
    -loop 0 "$GIF_OUT"

echo ">> ffmpeg encode mp4"
ffmpeg -y -loglevel error \
    -framerate "$FPS" -i "$WORK/f%03d.png" \
    -c:v libx264 -pix_fmt yuv420p -movflags +faststart \
    "$MP4_OUT"

echo ">> wrote $GIF_OUT ($(du -h "$GIF_OUT" | cut -f1))"
echo ">> wrote $MP4_OUT ($(du -h "$MP4_OUT" | cut -f1))"
