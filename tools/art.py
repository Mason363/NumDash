"""Procedural drawing helpers for NumDash's pixel art.

All artwork is drawn from geometric primitives at 8x supersampling and
downsampled, so the calculator sprites are anti-aliased without shipping any
bitmap from the original game. Coordinates are in Geometry Dash units (30 per
block) relative to the sprite anchor unless noted; S converts them to pixels.
"""
import math
import numpy as np
from PIL import Image, ImageDraw

S = 2.0 / 3.0      # screen pixels per GD unit (20 px blocks)
SS = 8             # supersampling factor


class Art:
    """RGBA float canvas with an integer pixel anchor.

    w, h: size in pixels; ax, ay: anchor pixel (object origin). Drawing calls
    take unit coordinates relative to the anchor with y pointing up."""

    def __init__(self, w, h, ax=None, ay=None):
        self.w, self.h = int(w), int(h)
        self.ax = self.w // 2 if ax is None else int(ax)
        self.ay = self.h // 2 if ay is None else int(ay)
        self.rgb = np.zeros((self.h * SS, self.w * SS, 3), np.float32)
        self.a = np.zeros((self.h * SS, self.w * SS), np.float32)

    # unit -> supersampled pixel coordinates
    def X(self, u):
        return (self.ax + u * S) * SS

    def Y(self, v):
        return (self.ay - v * S) * SS

    def mask(self, fn):
        """Rasterize a PIL drawing callback into a float coverage mask."""
        im = Image.new('L', (self.w * SS, self.h * SS), 0)
        fn(ImageDraw.Draw(im))
        return np.asarray(im).astype(np.float32) / 255.0

    def poly_mask(self, pts):
        return self.mask(lambda d: d.polygon([(self.X(x), self.Y(y)) for x, y in pts], fill=255))

    def rect_mask(self, x0, y0, x1, y1):
        return self.poly_mask([(x0, y0), (x1, y0), (x1, y1), (x0, y1)])

    def ellipse_mask(self, cx, cy, rx, ry):
        return self.mask(lambda d: d.ellipse([self.X(cx - rx), self.Y(cy + ry), self.X(cx + rx), self.Y(cy - ry)], fill=255))

    def grid(self):
        """Unit coordinates (u, v) of every supersample centre."""
        ys, xs = np.mgrid[0:self.h * SS, 0:self.w * SS].astype(np.float32)
        u = ((xs + 0.5) / SS - self.ax) / S
        v = (self.ay - (ys + 0.5) / SS) / S
        return u, v

    def paint(self, mask, color, alpha=1.0):
        """Composite colour (rgb tuple 0..1 or HxWx3 array) over the canvas."""
        m = np.clip(mask * alpha, 0, 1)
        col = np.asarray(color, np.float32)
        if col.ndim == 1:
            col = col.reshape(1, 1, 3)
        self.rgb = self.rgb * (1 - m[..., None]) + col * m[..., None]
        self.a = self.a + (1 - self.a) * m

    def erase(self, mask):
        self.a = self.a * (1 - np.clip(mask, 0, 1))

    def image(self):
        """Downsample to (h, w, 4) float RGBA with premultiplied-correct colour."""
        h, w = self.h, self.w
        a = self.a.reshape(h, SS, w, SS).mean(axis=(1, 3))
        prem = (self.rgb * self.a[..., None]).reshape(h, SS, w, SS, 3).mean(axis=(1, 3))
        rgb = np.where(a[..., None] > 1e-6, prem / np.maximum(a[..., None], 1e-6), 0)
        return np.concatenate([np.clip(rgb, 0, 1), np.clip(a, 0, 1)[..., None]], axis=2)


def stroke_mask(art, pts, width, closed=True):
    """Polyline stroke of the given width (units), with round joins."""
    wpx = max(1, int(round(width * S * SS)))
    seq = pts + ([pts[0]] if closed else [])

    def fn(d):
        xy = [(art.X(x), art.Y(y)) for x, y in seq]
        d.line(xy, fill=255, width=wpx)
        r = wpx / 2
        for x, y in xy:
            d.ellipse([x - r, y - r, x + r, y + r], fill=255)
    return art.mask(fn)


def inset_polygon(pts, d):
    """Offset a convex counter-clockwise polygon inwards by d units."""
    n = len(pts)
    lines = []
    for i in range(n):
        (x0, y0), (x1, y1) = pts[i], pts[(i + 1) % n]
        ex, ey = x1 - x0, y1 - y0
        L = math.hypot(ex, ey)
        nx, ny = -ey / L, ex / L          # inward normal for CCW order
        lines.append((x0 + nx * d, y0 + ny * d, ex, ey))
    out = []
    for i in range(n):
        x0, y0, dx0, dy0 = lines[i - 1]
        x1, y1, dx1, dy1 = lines[i]
        det = dx0 * dy1 - dy0 * dx1
        t = ((x1 - x0) * dy1 - (y1 - y0) * dx1) / det
        out.append((x0 + dx0 * t, y0 + dy0 * t))
    return out


def lerp(a, b, t):
    return a + (b - a) * t


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def hexc(s):
    s = s.lstrip('#')
    return tuple(int(s[i:i + 2], 16) / 255.0 for i in (0, 2, 4))
