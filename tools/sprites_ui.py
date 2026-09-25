"""User-interface artwork in the style of Geometry Dash's menus: cross-shaped
play/garage/creator buttons, round green buttons with gold icons, difficulty
faces, coins, stars, arrows and the stepped side art. Drawn from primitives
in pixel coordinates (y down) at 8x supersampling."""
import math
import numpy as np
from PIL import Image, ImageDraw
from art import SS, hexc, inset_polygon

WHITE = (1.0, 1.0, 1.0)
BLACK = (0.0, 0.0, 0.0)
G_LIGHT = hexc('b4ff3c')
G_MID = hexc('5ad200')
G_DARK = hexc('2f8c00')
CYAN_L = hexc('9dffff')
CYAN = hexc('14e1ff')
CYAN_D = hexc('0090d0')
GOLD_L = hexc('fff45a')
GOLD = hexc('ffc800')
GOLD_D = hexc('e07800')


class Px:
    """RGBA canvas addressed in pixels (y down) with an anchor pixel."""

    def __init__(self, w, h, ax=None, ay=None):
        self.w, self.h = int(w), int(h)
        self.ax = self.w // 2 if ax is None else int(ax)
        self.ay = self.h // 2 if ay is None else int(ay)
        self.rgb = np.zeros((self.h * SS, self.w * SS, 3), np.float32)
        self.a = np.zeros((self.h * SS, self.w * SS), np.float32)
        ys, xs = np.mgrid[0:self.h * SS, 0:self.w * SS].astype(np.float32)
        self.gx = (xs + 0.5) / SS
        self.gy = (ys + 0.5) / SS

    def mask(self, fn):
        im = Image.new('L', (self.w * SS, self.h * SS), 0)
        fn(ImageDraw.Draw(im))
        return np.asarray(im).astype(np.float32) / 255.0

    def poly(self, pts):
        return self.mask(lambda d: d.polygon([(x * SS, y * SS) for x, y in pts], fill=255))

    def ellipse(self, cx, cy, rx, ry=None):
        ry = rx if ry is None else ry
        return self.mask(lambda d: d.ellipse([(cx - rx) * SS, (cy - ry) * SS, (cx + rx) * SS, (cy + ry) * SS], fill=255))

    def rect(self, x0, y0, x1, y1):
        return self.poly([(x0, y0), (x1, y0), (x1, y1), (x0, y1)])

    def line(self, pts, width):
        def fn(d):
            xy = [(x * SS, y * SS) for x, y in pts]
            d.line(xy, fill=255, width=max(1, int(width * SS)))
            r = width * SS / 2
            for x, y in xy:
                d.ellipse([x - r, y - r, x + r, y + r], fill=255)
        return self.mask(fn)

    def paint(self, m, color, alpha=1.0):
        m = np.clip(m * alpha, 0, 1)
        col = np.asarray(color, np.float32)
        if col.ndim == 1:
            col = col.reshape(1, 1, 3)
        self.rgb = self.rgb * (1 - m[..., None]) + col * m[..., None]
        self.a = self.a + (1 - self.a) * m

    def vgrad(self, top, bottom, y0, y1):
        t = np.clip((self.gy - y0) / max(1e-6, y1 - y0), 0, 1)[..., None]
        return np.asarray(top, np.float32) * (1 - t) + np.asarray(bottom, np.float32) * t

    def image(self):
        h, w = self.h, self.w
        a = self.a.reshape(h, SS, w, SS).mean(axis=(1, 3))
        prem = (self.rgb * self.a[..., None]).reshape(h, SS, w, SS, 3).mean(axis=(1, 3))
        rgb = np.where(a[..., None] > 1e-6, prem / np.maximum(a[..., None], 1e-6), 0)
        return np.concatenate([np.clip(rgb, 0, 1), np.clip(a, 0, 1)[..., None]], axis=2)


def grow(m, px_):
    """Dilate a supersampled mask by px_ pixels."""
    from PIL import ImageFilter
    im = Image.fromarray((m * 255).astype(np.uint8))
    r = int(round(px_ * SS))
    k = 2 * r + 1
    while k > 1:
        step = min(k, 9)
        if step % 2 == 0:
            step -= 1
        im = im.filter(ImageFilter.MaxFilter(step))
        k -= step - 1
    return np.asarray(im).astype(np.float32) / 255.0


def outlined(c, m, fill, outline=1.6, shadow=0.0):
    """Black outline (and optional drop shadow) around a shape, then its fill."""
    o = grow(m, outline)
    if shadow:
        sh = np.roll(np.roll(o, int(shadow * SS), 0), int(shadow * SS * 0.6), 1)
        c.paint(sh, BLACK, 0.45)
    c.paint(o, BLACK)
    c.paint(m, fill)


# ------------------------------------------------------------ icon glyphs

def tri_right(cx, cy, s):
    return [(cx - s * 0.55, cy - s * 0.75), (cx + s * 0.8, cy), (cx - s * 0.55, cy + s * 0.75)]


def draw_play(c, cx, cy, s):
    m = c.poly(tri_right(cx, cy, s))
    outlined(c, m, c.vgrad(GOLD_L, GOLD_D, cy - s, cy + s), outline=max(1.2, s * 0.12))


def draw_gear(c, cx, cy, s):
    teeth = []
    for i in range(16):
        ang = i * math.pi / 8
        r = s * (1.0 if i % 2 == 0 else 0.78)
        teeth.append((cx + r * math.cos(ang + 0.2), cy + r * math.sin(ang + 0.2)))
        teeth.append((cx + r * math.cos(ang - 0.2 + math.pi / 8), cy + r * math.sin(ang - 0.2 + math.pi / 8)))
    m = np.clip(c.poly(teeth) - c.ellipse(cx, cy, s * 0.36), 0, 1)
    outlined(c, m, c.vgrad(GOLD_L, GOLD_D, cy - s, cy + s), outline=1.3)


def draw_bars(c, cx, cy, s):
    m = np.zeros_like(c.a)
    for i, hgt in enumerate((0.9, 1.5, 1.15)):
        x0 = cx - s * 0.95 + i * s * 0.68
        m = np.maximum(m, c.rect(x0, cy + s * 0.8 - s * hgt, x0 + s * 0.5, cy + s * 0.8))
    outlined(c, m, c.vgrad(GOLD_L, GOLD_D, cy - s, cy + s), outline=1.3)


def draw_question(c, cx, cy, s):
    arc = c.line([(cx - s * 0.5, cy - s * 0.35), (cx - s * 0.35, cy - s * 0.8), (cx + s * 0.2, cy - s * 0.9),
                  (cx + s * 0.55, cy - s * 0.55), (cx + s * 0.4, cy - s * 0.1), (cx, cy + s * 0.15), (cx, cy + s * 0.35)], s * 0.38)
    dot = c.ellipse(cx, cy + s * 0.8, s * 0.22)
    outlined(c, np.maximum(arc, dot), c.vgrad(GOLD_L, GOLD_D, cy - s, cy + s), outline=1.3)


def draw_arrow_left(c, cx, cy, s):
    pts = [(cx - s * 0.9, cy), (cx - s * 0.05, cy - s * 0.8), (cx - s * 0.05, cy - s * 0.32), (cx + s * 0.85, cy - s * 0.32),
           (cx + s * 0.85, cy + s * 0.32), (cx - s * 0.05, cy + s * 0.32), (cx - s * 0.05, cy + s * 0.8)]
    outlined(c, c.poly(pts), c.vgrad(GOLD_L, GOLD_D, cy - s, cy + s), outline=1.3)


def draw_replay(c, cx, cy, s):
    ring = np.clip(c.ellipse(cx, cy, s * 0.85) - c.ellipse(cx, cy, s * 0.48), 0, 1)
    ring *= 1 - c.poly([(cx, cy), (cx + s * 1.2, cy - s * 1.2), (cx + s * 1.2, cy - s * 0.1)])
    head = c.poly([(cx + s * 0.28, cy - s * 0.95), (cx + s * 1.02, cy - s * 0.52), (cx + s * 0.4, cy - s * 0.02)])
    m = np.clip(ring + head, 0, 1)
    outlined(c, m, c.vgrad(CYAN_L, CYAN_D, cy - s, cy + s), outline=1.3)


def draw_list(c, cx, cy, s):
    m = np.zeros_like(c.a)
    for i in range(3):
        y = cy - s * 0.6 + i * s * 0.6
        m = np.maximum(m, c.ellipse(cx - s * 0.62, y, s * 0.2))
        m = np.maximum(m, c.rect(cx - s * 0.28, y - s * 0.17, cx + s * 0.85, y + s * 0.17))
    outlined(c, m, c.vgrad(CYAN_L, CYAN_D, cy - s, cy + s), outline=1.3)


def draw_diamond(c, cx, cy, s, cross=False):
    pts = [(cx, cy - s), (cx + s * 0.6, cy), (cx, cy + s), (cx - s * 0.6, cy)]
    m = c.poly(pts)
    outlined(c, m, hexc('46ff5a'), outline=1.3)
    c.paint(c.poly([(cx, cy - s), (cx + s * 0.6, cy), (cx, cy)]), hexc('c8ffc8'), 0.9)
    c.paint(c.poly([(cx, cy + s), (cx - s * 0.6, cy), (cx, cy)]), hexc('00b432'), 0.9)
    if cross:
        ring = np.clip(c.ellipse(cx, cy, s * 0.95) - c.ellipse(cx, cy, s * 0.72), 0, 1)
        bar = c.poly([(cx - s * 0.62, cy - s * 0.78), (cx - s * 0.78, cy - s * 0.62), (cx + s * 0.62, cy + s * 0.78), (cx + s * 0.78, cy + s * 0.62)])
        outlined(c, np.clip(ring + bar, 0, 1), hexc('ff2828'), outline=1.0)


def draw_face_icon(c, cx, cy, s):
    m = c.rect(cx - s, cy - s, cx + s, cy + s)
    outlined(c, m, c.vgrad(GOLD_L, GOLD_D, cy - s, cy + s), outline=1.4)
    for dx in (-0.45, 0.45):
        c.paint(c.rect(cx + dx * s - s * 0.2, cy - s * 0.45, cx + dx * s + s * 0.2, cy - s * 0.05), BLACK)
    c.paint(c.rect(cx - s * 0.55, cy + s * 0.3, cx + s * 0.55, cy + s * 0.52), BLACK)


def draw_tools(c, cx, cy, s):
    ham = c.poly([(cx - s * 0.9, cy - s * 0.55), (cx - s * 0.2, cy - s * 0.95), (cx + s * 0.05, cy - s * 0.55), (cx - s * 0.55, cy - s * 0.15)])
    handle = c.line([(cx - s * 0.35, cy - s * 0.45), (cx + s * 0.75, cy + s * 0.8)], s * 0.3)
    wrench = c.line([(cx + s * 0.55, cy - s * 0.55), (cx - s * 0.6, cy + s * 0.7)], s * 0.3)
    jaw = np.clip(c.ellipse(cx + s * 0.62, cy - s * 0.62, s * 0.38) - c.poly([(cx + s * 0.62, cy - s * 0.62), (cx + s * 1.2, cy - s * 1.1), (cx + s * 1.2, cy - s * 0.3)]), 0, 1)
    m = np.clip(ham + handle + wrench + jaw, 0, 1)
    outlined(c, m, c.vgrad(GOLD_L, GOLD_D, cy - s, cy + s), outline=1.3)


# ------------------------------------------------------------ buttons

def round_button(d, icon, color='green', ring=False):
    pad = 3 if ring else 0
    c = Px(d + 3 + 2 * pad, d + 3 + 2 * pad)
    r = d / 2 - 1
    cx = cy = d / 2 + 0.5 + pad
    disc = c.ellipse(cx, cy, r)
    outer = grow(disc, 1.2 + (2.2 if ring else 0))
    c.paint(np.roll(np.roll(outer, int(1.5 * SS), 0), int(SS), 1), BLACK, 0.45)
    if ring:
        c.paint(outer, hexc('f0f0f0'))
    c.paint(grow(disc, 1.2), BLACK)
    body = c.vgrad(G_LIGHT, G_DARK, cy - r, cy + r) if color == 'green' else c.vgrad(CYAN_L, CYAN_D, cy - r, cy + r)
    c.paint(disc, body)
    rim = np.clip(c.ellipse(cx, cy, r) - c.ellipse(cx, cy, r - max(2.0, r * 0.14)), 0, 1)
    c.paint(rim, hexc('d8ff9c') if color == 'green' else CYAN_L, 0.55)
    c.paint(c.ellipse(cx - r * 0.3, cy - r * 0.45, r * 0.35, r * 0.2), WHITE, 0.35)
    if icon:
        icon(c, cx, cy, r * 0.55)
    return c


def cross_button(d, icon):
    """GD's plus-shaped main menu buttons: green arms, cyan inner corners."""
    c = Px(d + 4, d + 4)
    cx = cy = d / 2 + 1
    a = d / 2 - 1          # half size
    b = a * 0.42           # half arm width
    arms = np.clip(c.rect(cx - b, cy - a, cx + b, cy + a) + c.rect(cx - a, cy - b, cx + a, cy + b), 0, 1)
    corners = np.zeros_like(arms)
    k = a * 0.52
    for sx in (-1, 1):
        for sy in (-1, 1):
            x0, y0 = cx + sx * b, cy + sy * b
            corners = np.maximum(corners, c.rect(min(x0, x0 + sx * (k - b) * 1.9), min(y0, y0 + sy * (k - b) * 1.9),
                                                 max(x0, x0 + sx * (k - b) * 1.9), max(y0, y0 + sy * (k - b) * 1.9)))
    shape = np.clip(arms + corners, 0, 1)
    c.paint(np.roll(np.roll(grow(shape, 1.8), int(2 * SS), 0), int(1.5 * SS), 1), BLACK, 0.45)
    c.paint(grow(shape, 1.8), BLACK)
    c.paint(corners, c.vgrad(CYAN_L, CYAN_D, cy - a, cy + a))
    c.paint(grow(corners, 0.9) * (1 - grow(corners, 0.0)), BLACK, 0.8)
    c.paint(arms, c.vgrad(G_LIGHT, G_DARK, cy - a, cy + a))
    # bevel: light top-left inner edge, darker lower right
    inner = grow(arms, 0) - np.clip(np.roll(np.roll(arms, int(2 * SS), 0), int(2 * SS), 1), 0, 1)
    c.paint(np.clip(inner, 0, 1), hexc('e6ffb4'), 0.55)
    if icon:
        icon(c, cx, cy, a * 0.5)
    return c


def arrow_back(w, h):
    """GD's green triangle back arrow (points left)."""
    c = Px(w + 4, h + 4)
    pts = [(w + 1, 2), (2, h / 2 + 2), (w + 1, h + 2)]
    m = c.poly(pts)
    c.paint(np.roll(np.roll(grow(m, 1.6), int(1.5 * SS), 0), int(SS), 1), BLACK, 0.45)
    c.paint(grow(m, 1.6), BLACK)
    c.paint(m, c.vgrad(hexc('c8ff5a'), hexc('3ca000'), 2, h + 2))
    inner = c.poly(inset_polygon(pts[::-1], 2.4)[::-1])
    c.paint(np.clip(m - inner, 0, 1), hexc('e6ffaa'), 0.5)
    return c


def pause_icon(c, cx, cy, s):
    m = np.maximum(c.rect(cx - s * 0.62, cy - s * 0.7, cx - s * 0.16, cy + s * 0.7), c.rect(cx + s * 0.16, cy - s * 0.7, cx + s * 0.62, cy + s * 0.7))
    outlined(c, m, c.vgrad(CYAN_L, CYAN_D, cy - s, cy + s), outline=1.0)


def nav_arrow(w, h):
    c = Px(w + 3, h + 3)
    pts = [(1, 1), (w - 1, h / 2), (1, h - 1)]
    m = c.poly(pts)
    c.paint(np.roll(np.roll(m, int(2 * SS), 0), int(2 * SS), 1), BLACK, 0.5)
    c.paint(m, c.vgrad(WHITE, hexc('c8c8c8'), 0, h))
    c.paint(np.clip(m - c.poly(inset_polygon([(1, h - 1), (w - 1, h / 2), (1, 1)], 2.2)[::-1]), 0, 1), hexc('ffffff'), 0.6)
    return c


def diff_face(kind):
    """Difficulty faces: easy, normal, hard, harder, insane."""
    cols = {1: (hexc('7cf2ff'), hexc('00a8ff')), 2: (hexc('8cff64'), hexc('20c800')), 3: (hexc('ffd264'), hexc('ff9600')),
            4: (hexc('ff8c64'), hexc('e61e1e')), 5: (hexc('ff9cff'), hexc('dc28dc'))}[kind]
    d = 26
    c = Px(d + 3, d + 3)
    cx = cy = d / 2 + 0.5
    r = d / 2 - 1
    head = c.ellipse(cx, cy, r)
    c.paint(grow(head, 1.2), BLACK)
    c.paint(head, c.vgrad(cols[0], cols[1], cy - r, cy + r))
    c.paint(c.ellipse(cx - r * 0.35, cy - r * 0.5, r * 0.3, r * 0.16), WHITE, 0.4)
    # eyes
    for sx in (-1, 1):
        ex = cx + sx * r * 0.38
        if kind >= 4:
            brow = c.poly([(ex - r * 0.3, cy - r * 0.45 - (0.1 if sx < 0 else -0.1) * r * 3), (ex + r * 0.3, cy - r * 0.45 + (0.1 if sx < 0 else -0.1) * r * 3),
                           (ex + r * 0.3, cy - r * 0.25 + (0.1 if sx < 0 else -0.1) * r * 3), (ex - r * 0.3, cy - r * 0.25 - (0.1 if sx < 0 else -0.1) * r * 3)])
            c.paint(brow, BLACK)
        c.paint(c.ellipse(ex, cy - r * 0.1, r * 0.2, r * 0.24), WHITE)
        c.paint(c.ellipse(ex + sx * r * 0.04, cy - r * 0.08, r * 0.11), BLACK)
    # mouth
    if kind <= 2:
        smile = c.ellipse(cx, cy + r * 0.22, r * 0.45, r * 0.38) * c.rect(0, cy + r * 0.22, 999, 999)
        c.paint(smile, BLACK)
        if kind == 1:
            c.paint(c.ellipse(cx, cy + r * 0.48, r * 0.22, r * 0.1), hexc('ff5a78'))
    elif kind == 3:
        c.paint(c.rect(cx - r * 0.4, cy + r * 0.38, cx + r * 0.4, cy + r * 0.52), BLACK)
    else:
        frown = c.ellipse(cx, cy + r * 0.62, r * 0.45, r * 0.32) * c.rect(0, 0, 999, cy + r * 0.62)
        c.paint(frown, BLACK)
        if kind == 5:
            for tx in (-0.25, 0.0, 0.25):
                c.paint(c.poly([(cx + (tx - 0.09) * r, cy + r * 0.34), (cx + (tx + 0.09) * r, cy + r * 0.34), (cx + tx * r, cy + r * 0.5)]), WHITE)
    return c


def star(d, color=GOLD):
    c = Px(d + 2, d + 2)
    cx = cy = d / 2 + 1
    pts = []
    for i in range(10):
        ang = -math.pi / 2 + i * math.pi / 5
        r = d / 2 - 0.5 if i % 2 == 0 else d * 0.21
        pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
    m = c.poly(pts)
    outlined(c, m, c.vgrad(GOLD_L, GOLD_D, cy - d / 2, cy + d / 2) if color == GOLD else color, outline=1.0)
    return c


def coin_icon(d, filled=True):
    c = Px(d + 2, d + 2)
    cx = cy = d / 2 + 1
    r = d / 2 - 0.5
    disc = c.ellipse(cx, cy, r)
    c.paint(grow(disc, 0.8), hexc('3a1e00') if filled else hexc('202020'))
    if filled:
        c.paint(disc, c.vgrad(hexc('ffe25a'), hexc('d27800'), cy - r, cy + r))
    else:
        c.paint(disc, c.vgrad(hexc('9a9a9a'), hexc('505050'), cy - r, cy + r))
    pts = []
    for i in range(10):
        ang = -math.pi / 2 + i * math.pi / 5
        rr = r * 0.62 if i % 2 == 0 else r * 0.26
        pts.append((cx + rr * math.cos(ang), cy + rr * math.sin(ang) + 0.3))
    c.paint(c.poly(pts), hexc('c86400') if filled else hexc('3a3a3a'), 0.9)
    return c


def side_art(n):
    """Stepped corner decoration (lime and cyan blocks)."""
    s = 10
    c = Px(n * s + 3, n * s + 3, 0, n * s + 3)
    for col in range(n):
        for row in range(n - col):
            x0, y0 = col * s + 1, (n - 1 - row) * s + 1
            m = c.rect(x0, y0, x0 + s - 1, y0 + s - 1)
            colr = CYAN if (col + row) % 2 == 0 else G_LIGHT
            c.paint(grow(m, 0.9), BLACK)
            c.paint(m, c.vgrad(colr, tuple(v * 0.7 for v in colr), y0, y0 + s))
    return c


def checkbox(on):
    d = 18
    c = Px(d + 2, d + 2)
    m = c.rect(1.5, 1.5, d - 0.5, d - 0.5)
    c.paint(grow(m, 1.0), BLACK)
    c.paint(m, c.vgrad(hexc('c8c8c8'), hexc('787878'), 0, d))
    if on:
        tick = c.line([(4.5, 10), (8, 14), (15.5, 4.5)], 3.2)
        outlined(c, tick, c.vgrad(hexc('a0ff50'), hexc('28b400'), 0, d), outline=1.0)
    return c


def ui_sprites():
    out = []
    add = lambda n, c, f='PAL4': out.append((n, c, f))
    add('btn_play', cross_button(74, draw_play))
    add('btn_garage', cross_button(50, draw_face_icon))
    add('btn_creator', cross_button(50, draw_tools))
    add('btn_settings', round_button(34, draw_gear))
    add('btn_stats', round_button(34, draw_bars))
    add('btn_help', round_button(34, draw_question))
    add('btn_back', round_button(30, draw_arrow_left))
    add('btn_resume', round_button(62, draw_play, ring=True))
    add('btn_replay', round_button(46, draw_replay, ring=True))
    add('btn_menu', round_button(46, draw_list, ring=True))
    add('btn_practice', round_button(46, lambda c, x, y, s: draw_diamond(c, x, y, s * 1.2), ring=True))
    add('btn_normal', round_button(46, lambda c, x, y, s: draw_diamond(c, x, y, s * 1.2, True), ring=True))
    add('btn_settings_small', round_button(28, draw_gear))
    add('btn_pause', round_button(22, pause_icon))
    add('arrow_back', arrow_back(20, 26))
    add('nav_arrow', nav_arrow(22, 50))
    for k in range(1, 6):
        add('face%d' % k, diff_face(k))
    add('star_ui', star(12))
    add('star_big', star(22))
    add('coin_ui', coin_icon(14))
    add('coin_ui_empty', coin_icon(14, False))
    add('coin_big', coin_icon(26))
    add('coin_big_empty', coin_icon(26, False))
    add('side_art', side_art(4))
    add('check_on', checkbox(True))
    add('check_off', checkbox(False))
    return out
