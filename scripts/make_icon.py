"""Generates res/openFences.ico (two overlapping translucent fences) without third-party packages.

Usage: python scripts/make_icon.py
"""
import os
import struct
import zlib

SIZES = [16, 20, 24, 32, 40, 48, 64, 256]
SS = 4  # supersampling per axis

# (x0, y0, x1, y1, radius, body RGBA, title RGBA, title height) in unit coordinates.
PANELS = [
    (0.06, 0.12, 0.62, 0.86, 0.09, (52, 116, 226, 240), (24, 66, 160, 255), 0.15),
    (0.44, 0.32, 0.94, 0.90, 0.09, (40, 186, 164, 235), (18, 120, 108, 255), 0.13),
]
# Little "icons" inside the first panel.
DOTS = [(0.15, 0.36), (0.33, 0.36), (0.15, 0.58), (0.33, 0.58)]
DOT_SIZE = 0.13


def inside_round_rect(x, y, x0, y0, x1, y1, r):
    if x < x0 or x > x1 or y < y0 or y > y1:
        return False
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def over(dst, src):
    sa = src[3] / 255.0
    da = dst[3] / 255.0
    oa = sa + da * (1 - sa)
    if oa <= 0:
        return (0, 0, 0, 0)
    out = [(src[i] * sa + dst[i] * da * (1 - sa)) / oa for i in range(3)]
    return (out[0], out[1], out[2], oa * 255)


def sample(x, y):
    color = (0, 0, 0, 0)
    for i, (x0, y0, x1, y1, r, body, title, th) in enumerate(PANELS):
        if inside_round_rect(x, y, x0, y0, x1, y1, r):
            color = over(color, title if y < y0 + th else body)
            if i == 0:
                for dx, dy in DOTS:
                    if dx <= x <= dx + DOT_SIZE and dy <= y <= dy + DOT_SIZE:
                        color = over(color, (255, 255, 255, 215))
    return color


def render(size):
    px = []
    for row in range(size):
        line = []
        for col in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(SS):
                for sx in range(SS):
                    c = sample((col + (sx + 0.5) / SS) / size, (row + (sy + 0.5) / SS) / size)
                    a = c[3]
                    acc[0] += c[0] * a
                    acc[1] += c[1] * a
                    acc[2] += c[2] * a
                    acc[3] += a
            n = SS * SS
            a = acc[3] / n
            if a > 0:
                rgb = [acc[i] / acc[3] for i in range(3)]
            else:
                rgb = [0, 0, 0]
            line.append((int(rgb[0]), int(rgb[1]), int(rgb[2]), int(round(a))))
        px.append(line)
    return px


def png_bytes(px):
    size = len(px)
    raw = b"".join(b"\x00" + bytes(v for p in row for v in p) for row in px)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def bmp_bytes(px):
    size = len(px)
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    pixels = b"".join(bytes((p[2], p[1], p[0], p[3])) for row in reversed(px) for p in row)
    mask_row = ((size + 31) // 32) * 4
    mask = b"\x00" * (mask_row * size)
    return header + pixels + mask


def main():
    images = []
    for s in SIZES:
        px = render(s)
        images.append((s, png_bytes(px) if s >= 256 else bmp_bytes(px)))
    out = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    for s, data in images:
        dim = 0 if s >= 256 else s
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    blob = out + entries + b"".join(d for _, d in images)
    target = os.path.join(os.path.dirname(__file__), "..", "res", "openFences.ico")
    with open(target, "wb") as f:
        f.write(blob)
    print("wrote", os.path.normpath(target), len(blob), "bytes")


if __name__ == "__main__":
    main()
