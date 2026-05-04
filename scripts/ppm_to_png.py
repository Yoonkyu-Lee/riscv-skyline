#!/usr/bin/env python3
# Copyright (c) 2024-2026 Yoonkyu Lee
# SPDX-License-Identifier: MIT
"""ppm_to_png - convert binary PPM (P6) to PNG using only Python stdlib.

Used by scripts/screenshot.sh to make the hero PNG without depending on
ImageMagick / Pillow. PPM is what `screendump` writes from the QEMU
monitor; PNG is what GitHub embeds in the README.
"""
import struct
import sys
import zlib


def parse_ppm(data: bytes):
    pos = 0

    def skip_ws():
        nonlocal pos
        while pos < len(data) and data[pos:pos + 1] in b" \t\r\n":
            pos += 1
        while pos < len(data) and data[pos:pos + 1] == b"#":
            while pos < len(data) and data[pos:pos + 1] != b"\n":
                pos += 1
            while pos < len(data) and data[pos:pos + 1] in b" \t\r\n":
                pos += 1

    def token():
        nonlocal pos
        skip_ws()
        start = pos
        while pos < len(data) and data[pos:pos + 1] not in b" \t\r\n":
            pos += 1
        return data[start:pos]

    magic = token()
    if magic != b"P6":
        raise ValueError(f"unsupported magic {magic!r}")
    w = int(token())
    h = int(token())
    maxval = int(token())
    if maxval != 255:
        raise ValueError(f"unsupported maxval {maxval}")
    if data[pos:pos + 1] in b" \t\r\n":
        pos += 1
    pixels = data[pos:]
    if len(pixels) < w * h * 3:
        raise ValueError(f"truncated pixels: have {len(pixels)}, need {w*h*3}")
    return w, h, pixels[:w * h * 3]


def png_chunk(tag: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + tag
        + payload
        + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
    )


def make_png(w: int, h: int, rgb: bytes) -> bytes:
    raw = bytearray()
    stride = w * 3
    for row in range(h):
        raw.append(0)  # filter type 0 (None)
        raw += rgb[row * stride:(row + 1) * stride]
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)  # 8-bit RGB
    return (
        sig
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + png_chunk(b"IEND", b"")
    )


def main():
    if len(sys.argv) != 3:
        sys.exit(f"usage: {sys.argv[0]} input.ppm output.png")
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    w, h, rgb = parse_ppm(data)
    with open(sys.argv[2], "wb") as f:
        f.write(make_png(w, h, rgb))
    print(f"wrote {sys.argv[2]} ({w}x{h})")


if __name__ == "__main__":
    main()
