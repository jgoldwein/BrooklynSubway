#!/usr/bin/env python3
"""Render the top silkscreen/outline of a fabrication Gerber ZIP as an SVG underlay.

Host-only and read-only: the Gerber ZIP and the PCB are never modified. The SVG is
written in the same coordinate space the virtual map uses for LEDs (PCB units of
0.0254 mm, y drawn downward), so it aligns with the LED circles without calibration.

    python tools/virtual_map/extract_gerber_art.py [gerber.zip]

Requires `gerbonara` (host tooling only; pip install gerbonara).
"""
from __future__ import annotations

import csv
import math
import re
import statistics
import sys
import tempfile
import zipfile
from pathlib import Path

from gerbonara import GerberFile
from gerbonara.utils import MM

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
LED_CSV = ROOT / "docs" / "mapping" / "phase3b" / "led_positions_v11.csv"
OUT = HERE / "artwork" / "board_top_silkscreen.svg"
UNIT_MM = 0.0254
# Coarse Gerber offset found by vote-matching LED coordinates to top paste pads.
COARSE = (-15.5, 202.0)


def fit_offset(paste: GerberFile, leds):
    pads = [(o.x, o.y) for o in paste.objects if hasattr(o, "x") and hasattr(o, "aperture")]
    dx, dy = [], []
    for x, y in leds:
        lx, ly = x * UNIT_MM + COARSE[0], y * UNIT_MM + COARSE[1]
        near = [p for p in pads if math.hypot(p[0] - lx, p[1] - ly) < 3.0]
        if len(near) >= 3:
            dx.append(sum(p[0] for p in near) / len(near) - x * UNIT_MM)
            dy.append(sum(p[1] for p in near) / len(near) - y * UNIT_MM)
    if len(dx) < 200:
        raise SystemExit(f"only {len(dx)} LEDs matched pads; Gerber does not match the V1.1 LED map")
    return statistics.median(dx), statistics.median(dy), len(dx), max(
        math.hypot(a - statistics.median(dx), b - statistics.median(dy)) for a, b in zip(dx, dy)
    )


def inner(svg: str) -> str:
    return svg[svg.index(">", svg.index("<svg")) + 1 : svg.rindex("</svg>")]


def main() -> None:
    zip_path = Path(sys.argv[1]) if len(sys.argv) > 1 else next(HERE.glob("Gerber_*.zip"))
    leds = [(float(r["x"]), float(r["y"])) for r in csv.DictReader(open(LED_CSV, newline=""))]
    with tempfile.TemporaryDirectory() as tmp, zipfile.ZipFile(zip_path) as z:
        z.extractall(tmp)
        find = lambda ext: next(Path(tmp).glob(f"*{ext}"))
        ox, oy, matched, worst = fit_offset(GerberFile.open(find(".GTP")), leds)
        print(f"offset mm=({ox:.3f},{oy:.3f}) matched={matched}/306 worst_dev={worst:.2f}mm")
        silk = GerberFile.open(find(".GTO"))
        outline = GerberFile.open(find(".GKO"))
        # Shared bounds so both layers share one gerbonara flip transform.
        (x0, y0), (x1, y1) = outline.bounding_box("mm")
        bounds = ((x0 - 5, y0 - 5), (x1 + 5, y1 + 5))
        silk_svg = str(silk.to_svg(margin=0, arg_unit=MM, svg_unit=MM, force_bounds=bounds, fg="#e8e8e8", bg="none"))
        out_svg = str(outline.to_svg(margin=0, arg_unit=MM, svg_unit=MM, force_bounds=bounds, fg="#f2c94c", bg="none"))
    top = float(re.search(r"viewBox=\"\S+ (\S+) \S+ (\S+)\"", silk_svg).group(1))
    height = float(re.search(r"viewBox=\"\S+ (\S+) \S+ (\S+)\"", silk_svg).group(2))
    # gerbonara svg Y = (maxY + minY) - gerber Y, where minY=top and maxY=top+height.
    cflip = 2 * top + height
    transform = f"scale({1 / UNIT_MM}) translate({-ox} {oy - cflip})"
    vx0, vx1 = (bounds[0][0] - ox) / UNIT_MM, (bounds[1][0] - ox) / UNIT_MM
    vy0, vy1 = (-(bounds[1][1] - oy)) / UNIT_MM, (-(bounds[0][1] - oy)) / UNIT_MM
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vx0:.1f} {vy0:.1f} {vx1 - vx0:.1f} {vy1 - vy0:.1f}">'
        f'<g transform="{transform}">{inner(out_svg)}{inner(silk_svg)}</g></svg>',
        encoding="utf-8",
    )
    print(f"wrote {OUT} ({OUT.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
