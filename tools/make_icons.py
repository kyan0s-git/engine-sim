#!/usr/bin/env python3
"""Draws the application icon and writes it in the formats the installers use.

    python3 tools/make_icons.py packaging/icons

Requires Pillow. Produces engine-sim.png (512 px), engine-sim.ico (16-256 px)
and engine-sim.icns (128-1024 px).
"""

import io
import math
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 1024
SS = 4  # supersampling


def draw_icon(size=SIZE):
    s = size * SS
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # Rounded square with a vertical gradient
    margin = int(s * 0.06)
    radius = int(s * 0.2)
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle([margin, margin, s - margin, s - margin], radius, fill=255)
    gradient = Image.new("RGBA", (s, s))
    gd = ImageDraw.Draw(gradient)
    top, bottom = (40, 44, 52), (14, 16, 19)
    for y in range(s):
        t = y / (s - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        gd.line([(0, y), (s, y)], fill=c + (255,))
    img.paste(gradient, (0, 0), mask)

    cx, cy = s / 2, s * 0.54
    r = s * 0.33
    start, end = 135.0, 405.0  # degrees, clockwise from +x (screen coordinates)

    def polar(angle_deg, radius):
        a = math.radians(angle_deg)
        return cx + math.cos(a) * radius, cy + math.sin(a) * radius

    # Dial arc and red zone
    w = int(s * 0.045)
    box = [cx - r, cy - r, cx + r, cy + r]
    d.arc(box, start, 340, fill=(230, 232, 235, 255), width=w)
    d.arc(box, 340, end, fill=(238, 68, 69, 255), width=w)

    # Ticks
    for i in range(9):
        a = start + (end - start) * i / 8
        p0 = polar(a, r * 0.80)
        p1 = polar(a, r * 0.95)
        d.line([p0, p1], fill=(230, 232, 235, 255), width=int(s * 0.022))

    # Needle at about three quarters
    a = start + (end - start) * 0.72
    tip = polar(a, r * 0.92)
    left = polar(a - 90, s * 0.03)
    right = polar(a + 90, s * 0.03)
    tail = polar(a + 180, r * 0.18)
    d.polygon([tip, left, tail, right], fill=(244, 128, 42, 255))
    hub = s * 0.065
    d.ellipse([cx - hub, cy - hub, cx + hub, cy + hub], fill=(244, 128, 42, 255))
    hub2 = s * 0.03
    d.ellipse([cx - hub2, cy - hub2, cx + hub2, cy + hub2], fill=(14, 16, 19, 255))

    # Exhaust note: three bars below the dial
    for i, h in enumerate((0.05, 0.09, 0.065)):
        x = cx + (i - 1) * s * 0.08
        y = cy + r * 0.62
        d.rounded_rectangle([x - s * 0.022, y - s * h / 2, x + s * 0.022, y + s * h / 2],
                            int(s * 0.02), fill=(119, 206, 224, 255))

    return img.resize((size, size), Image.LANCZOS)


def png_bytes(img):
    buffer = io.BytesIO()
    img.save(buffer, format="PNG")
    return buffer.getvalue()


def write_icns(path, master):
    # PNG payloads: ic07 128, ic08 256, ic09 512, ic10 1024 (512@2x)
    entries = b""
    for code, size in ((b"ic07", 128), (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024)):
        data = png_bytes(master.resize((size, size), Image.LANCZOS))
        entries += code + struct.pack(">I", len(data) + 8) + data
    path.write_bytes(b"icns" + struct.pack(">I", len(entries) + 8) + entries)


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "packaging/icons")
    out.mkdir(parents=True, exist_ok=True)

    master = draw_icon(SIZE)
    master.resize((512, 512), Image.LANCZOS).save(out / "engine-sim.png")
    master.save(out / "engine-sim.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    write_icns(out / "engine-sim.icns", master)


if __name__ == "__main__":
    main()
