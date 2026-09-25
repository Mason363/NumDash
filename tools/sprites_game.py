"""Gameplay artwork: blocks, hazards, decorations, pads, orbs, portals, coins, icons.

Each function returns (Art, format) where format is 'A4' (tinted coverage),
'LA44' (tinted luminance + alpha) or 'PAL4' (full colour). Shapes follow the
Geometry Dash 2.x look (black gradient blocks with bright outlines, glowing
decorations) and are redrawn from geometric primitives at calculator scale.
"""
import math
import random
import numpy as np
from art import Art, S, SS, stroke_mask, inset_polygon, lerp, smooth, hexc

WHITE = (1.0, 1.0, 1.0)
BLACK = (0.0, 0.0, 0.0)


def px(u):
    return int(math.ceil(u * S))


def block_art(edges='ltrb', size=30, grid=False, glowpad=0):
    """30x30 block. edges: which outline sides are drawn (l, t, r, b).
    grid: 3x3 tile pattern (IDs 2-7) instead of a vertical gradient."""
    half = size / 2
    w = px(size + 2 * glowpad)
    a = Art(w, w)
    u, v = a.grid()
    inside = (np.abs(u) < half) & (np.abs(v) < half)
    if grid:
        # three rows of cells: 80%, 60%, 40% black; thin transparent seams
        cell = size / 3
        row = np.clip(((half - v) // cell), 0, 2)
        alpha = np.choose(row.astype(int), [0.80, 0.60, 0.40])
        fu = (u + half) % cell
        fv = (half - v) % cell
        seam = (np.minimum(fu, cell - fu) < 0.6) | (np.minimum(fv, cell - fv) < 0.6)
        mask = inside & ~seam
        a.paint(mask.astype(np.float32), BLACK, 1.0)
        a.a = np.where(mask, alpha, a.a).astype(np.float32)
    else:
        t = np.clip((half - 3 - v) / (size - 6), 0, 1)       # 0 at top, 1 at bottom
        alpha = 1.0 - 0.87 * t
        m = inside.astype(np.float32)
        a.paint(m, BLACK, 1.0)
        a.a = np.where(inside, alpha, a.a).astype(np.float32)
    bw = 2.0   # outline width in units
    E = np.zeros_like(u, dtype=bool)
    if 'l' in edges: E |= inside & (u < -half + bw)
    if 'r' in edges: E |= inside & (u > half - bw)
    if 't' in edges: E |= inside & (v > half - bw)
    if 'b' in edges: E |= inside & (v < -half + bw)
    if 'c' in edges:   # inside-corner nub (ID 4)
        E |= inside & (u < -half + bw) & (v > half - bw)
    a.paint(E.astype(np.float32), WHITE, 1.0)
    return a


def glow_art(edges='ltrb', size=30, reach=6.0, peak=0.55, height=None):
    """Soft additive light along the outlined edges of a block. Each edge
    glows only across its own span, and outlined corners get a round join,
    so neighbouring blocks tile without doubled-up bright spots."""
    half = size / 2
    hh = (height if height else size) / 2
    a = Art(px(size + 2 * reach) + 1, px(2 * hh + 2 * reach) + 1)
    u, v = a.grid()
    d = np.full_like(u, 1e9)
    inl = np.abs(v) <= hh
    inb = np.abs(u) <= half
    L, T, R, B = ('l' in edges), ('t' in edges), ('r' in edges), ('b' in edges)
    if L: d = np.minimum(d, np.where(inl, np.abs(u + half), 1e9))
    if R: d = np.minimum(d, np.where(inl, np.abs(u - half), 1e9))
    if T: d = np.minimum(d, np.where(inb, np.abs(v - hh), 1e9))
    if B: d = np.minimum(d, np.where(inb, np.abs(v + hh), 1e9))
    corners = [((-half, hh), L and T), ((half, hh), R and T), ((-half, -hh), L and B), ((half, -hh), R and B)]
    for (cx, cy), on in corners:
        if on:
            outside = (np.sign(u - cx) == np.sign(cx)) & (np.sign(v - cy) == np.sign(cy))
            d = np.minimum(d, np.where(outside, np.hypot(u - cx, v - cy), 1e9))
    if 'c' in edges:
        d = np.minimum(d, np.hypot(u + half, v - hh))
    g = np.clip(1 - d / reach, 0, 1) ** 1.6 * peak
    a.rgb[:] = 1.0
    a.a = g.astype(np.float32)
    return a


def spike_art(w_u=30.0, h_u=30.0, bottom=-15.0):
    """Triangle spike, apex up, with a bright outline and gradient body."""
    top = bottom + h_u
    W = px(30)
    H = px(30)
    a = Art(W, H)
    tri = [(-w_u / 2, bottom), (w_u / 2, bottom), (0, top)]
    outer = a.poly_mask(tri)
    inner_pts = inset_polygon(tri, 1.6)
    inner = a.poly_mask(inner_pts)
    u, v = a.grid()
    t = np.clip((top - 0.45 * h_u - v) / (0.55 * h_u), 0, 1)
    body_alpha = 1.0 - 0.70 * t
    a.paint(outer, WHITE, 1.0)
    a.rgb = np.where(inner[..., None] > 0.5, 0.0, a.rgb).astype(np.float32)
    a.a = (outer * (1 - inner) + inner * body_alpha).astype(np.float32)
    return a


def spike_glow_art(w_u=30.0, h_u=30.0, bottom=-15.0, reach=6.0, peak=0.5):
    top = bottom + h_u
    a = Art(px(30 + 2 * reach) + 1, px(30 + 2 * reach) + 1)
    u, v = a.grid()
    P = [(-w_u / 2, bottom), (w_u / 2, bottom), (0, top)]

    def seg(p, q):
        (x0, y0), (x1, y1) = p, q
        dx, dy = x1 - x0, y1 - y0
        t = np.clip(((u - x0) * dx + (v - y0) * dy) / (dx * dx + dy * dy), 0, 1)
        return np.hypot(u - (x0 + t * dx), v - (y0 + t * dy))
    d = np.minimum(np.minimum(seg(P[0], P[2]), seg(P[1], P[2])), seg(P[0], P[1]) + 2.5)
    a.rgb[:] = 1.0
    a.a = (np.clip(1 - d / reach, 0, 1) ** 1.6 * peak).astype(np.float32)
    return a


def pit_art(seed):
    """Row of jagged ground spikes (ID 9), drawn black by the level."""
    rnd = random.Random(seed)
    a = Art(px(30), px(27))
    # anchor centre of the 30x27 sprite; the object sits at y=2 so its body
    # reaches ~15 units above the ground and continues below it.
    base_top = 2.0          # units above the anchor where the solid band ends
    pts = [(-15, -13.5)]
    x = -15.0
    pts.append((x, base_top + rnd.uniform(1, 4)))
    while x < 15:
        step = rnd.uniform(2.6, 4.6)
        tall = rnd.random() < 0.35
        peak = base_top + (rnd.uniform(8.0, 12.0) if tall else rnd.uniform(3.0, 6.5))
        valley = base_top + rnd.uniform(-1.0, 1.5)
        pts.append((min(15, x + step * 0.5), peak))
        x += step
        pts.append((min(15, x), valley))
    pts.append((15, -13.5))
    m = a.poly_mask(pts)
    u, v = a.grid()
    fade = np.clip((v + 13.5) / 7.0, 0, 1)      # fade out towards the bottom
    a.paint(m, WHITE, 1.0)
    a.a = (m * fade).astype(np.float32)
    return a


def plank_art():
    """Half-height slab (ID 40): outline plus black gradient fill."""
    a = Art(px(30), px(14) + 1)
    u, v = a.grid()
    inside = (np.abs(u) < 15) & (np.abs(v) < 7)
    t = np.clip((6 - v) / 12, 0, 1)
    a.paint(inside.astype(np.float32), BLACK, 1.0)
    a.a = np.where(inside, 1.0 - 0.8 * t, 0).astype(np.float32)
    edge = inside & ((np.abs(u) > 13) | (np.abs(v) > 5))
    a.paint(edge.astype(np.float32), WHITE, 1.0)
    return a


def slab_wave_art(variant):
    """Wavy 1.0-era slabs (IDs 62/65/66/68): black body, bright top line."""
    a = Art(px(30), px(16) + 2)
    u, v = a.grid()
    # bottom profile: rounded bumps whose shape depends on the variant
    shapes = {
        0: lambda x: -2 + 3.2 * np.cos((x + 15) / 30 * 2 * math.pi) ** 2,
        1: lambda x: -3 + 4.0 * np.abs(np.sin((x + 15) / 30 * math.pi * 1.5)),
        2: lambda x: -1.5 + 3.5 * np.cos((x + 5) / 30 * 2 * math.pi) ** 2,
        3: lambda x: -4 + 5.5 * np.abs(np.cos((x + 15) / 30 * math.pi)),
    }
    prof = shapes[variant](u)
    body = (np.abs(u) < 15) & (v < 8) & (v > prof - 4)
    a.paint(body.astype(np.float32), BLACK, 1.0)
    lip = (np.abs(u) < 15) & (v > prof - 4) & (v < prof - 1.5)
    a.a = np.where(lip, 0.55, a.a).astype(np.float32)
    top = (np.abs(u) < 15) & (v >= 6) & (v < 8)
    a.paint(top.astype(np.float32), WHITE, 1.0)
    return a


def rod_art(length):
    """Pulse rod pole, drawn black by the level (IDs 15-17)."""
    w_u = 6.5
    a = Art(px(w_u) + 2, px(length) + 1)
    u, v = a.grid()
    pole = (np.abs(u) < 1.6) & (np.abs(v) < length / 2 - 2)
    # segmented pole (small gaps every 5 units), fading upwards
    seg = ((v + length / 2) % 5.5) > 1.0
    alpha = np.clip(0.35 + 0.65 * (length / 2 - v) / length, 0, 1)
    a.paint((pole & seg).astype(np.float32), WHITE, 1.0)
    a.a = np.where(pole & seg, alpha, a.a).astype(np.float32)
    foot = (np.abs(u) < 3.2) & (v < -length / 2 + 2.2) & (v > -length / 2)
    a.paint(foot.astype(np.float32), WHITE, 1.0)
    return a


def ball_art(r=15.0):
    a = Art(px(2 * r) + 1, px(2 * r) + 1)
    m = a.ellipse_mask(0, 0, r - 0.5, r - 0.5)
    a.paint(m, WHITE, 1.0)
    return a


def mountains_art(w_u, h_u, peaks, seed):
    """Decorative spiky silhouettes (IDs 18-21), white with vertical fade."""
    rnd = random.Random(seed)
    a = Art(px(w_u) + 1, px(h_u) + 1)
    pts = [(-w_u / 2, -h_u / 2)]
    xs = sorted(rnd.uniform(-w_u / 2 + 3, w_u / 2 - 3) for _ in range(peaks))
    prev = -w_u / 2
    for x in xs:
        mid = (prev + x) / 2
        pts.append((mid, -h_u / 2 + rnd.uniform(0, h_u * 0.18)))
        pts.append((x, -h_u / 2 + rnd.uniform(0.35, 1.0) * h_u))
        prev = x
    pts.append((w_u / 2, -h_u / 2))
    m = a.poly_mask(pts)
    u, v = a.grid()
    fade = np.clip((v + h_u / 2) / (h_u * 0.9), 0, 1) ** 0.8
    a.paint(m, WHITE, 1.0)
    a.a = (m * (0.25 + 0.75 * fade)).astype(np.float32)
    return a


def chain_art(length, links):
    a = Art(px(20) + 1, px(length) + 1)
    top = length / 2
    link_h = (length - 8) / links
    for i in range(links):
        cy = top - link_h * (i + 0.5)
        horizontal = False
        rx, ry = (3.6, link_h * 0.62)
        outer = a.ellipse_mask(0, cy, rx, ry)
        inner = a.ellipse_mask(0, cy, rx - 1.7, ry - 1.9)
        ring = np.clip(outer - inner, 0, 1)
        a.paint(ring, WHITE, 0.85)
    base = a.rect_mask(-7, -top, 7, -top + 3)
    hook = a.ellipse_mask(0, -top + 4.5, 3.2, 3.2) - a.ellipse_mask(0, -top + 4.5, 1.6, 1.6)
    a.paint(np.clip(base + np.clip(hook, 0, 1), 0, 1), WHITE, 1.0)
    return a


def star_art(r_out=14.0, r_in=6.0):
    a = Art(px(30) + 1, px(30) + 1)
    pts = []
    for i in range(10):
        ang = math.pi / 2 + i * math.pi / 5
        r = r_out if i % 2 == 0 else r_in
        pts.append((r * math.cos(ang), r * math.sin(ang) - 1))
    a.paint(a.poly_mask(pts), WHITE, 1.0)
    return a


def diag_block_art():
    """ID 73 decorative block: four shaded triangles."""
    a = Art(px(30), px(30))
    u, v = a.grid()
    inside = (np.abs(u) < 15) & (np.abs(v) < 15)
    left = (u < v) & (u < -v)
    top = (v > u) & (v > -u)
    right = (u > v) & (u > -v)
    alpha = np.where(left, 0.85, np.where(top, 0.55, np.where(right, 0.35, 0.2)))
    a.paint(inside.astype(np.float32), BLACK, 1.0)
    a.a = np.where(inside, alpha, 0).astype(np.float32)
    return a


# ---------------------------------------------------------------- pads / orbs

PAD_COLORS = {
    'yellow': (hexc('ffe600'), hexc('ffb000')),
    'blue': (hexc('4cf7ff'), hexc('00a8ff')),
    'pink': (hexc('ff7cff'), hexc('e03cff')),
}


def pad_art(kind):
    """Jump pad dome (IDs 35, 67, 140). Anchor = object centre."""
    top_c, bot_c = PAD_COLORS[kind]
    h_u = 6.0 if kind == 'blue' else 5.0
    a = Art(px(30) + 1, px(12) + 1)
    u, v = a.grid()
    base = -h_u / 2
    dome = ((u / 12.5) ** 2 + ((v - base) / h_u) ** 2 < 1) & (v >= base)
    t = np.clip((v - base) / h_u, 0, 1)
    col = np.stack([lerp(bot_c[i], top_c[i], t) for i in range(3)], -1)
    a.paint(dome.astype(np.float32), col, 1.0)
    rim = dome & ((u / 12.5) ** 2 + ((v - base) / h_u) ** 2 > 0.55)
    a.paint(rim.astype(np.float32), tuple(min(1, c * 0.5 + 0.5) for c in top_c), 0.7)
    return a


def pad_glow_art(kind):
    top_c, _ = PAD_COLORS[kind]
    a = Art(px(36) + 1, px(18) + 1)
    u, v = a.grid()
    d = np.sqrt((u / 15.0) ** 2 + ((v + 3) / 8.0) ** 2)
    g = np.clip(1 - d, 0, 1) ** 1.4 * 0.75 * (v > -3.5)
    a.rgb[:] = top_c
    a.a = g.astype(np.float32)
    return a


def orb_art(kind='yellow'):
    """Jump orb (ID 36): filled core inside a thin bright ring."""
    core = {'yellow': (hexc('fff34a'), hexc('ffc800')), 'pink': (hexc('ff9cff'), hexc('f040ff')),
            'blue': (hexc('8ff9ff'), hexc('14c8ff'))}[kind]
    a = Art(px(30) + 1, px(30) + 1)
    u, v = a.grid()
    r = np.hypot(u, v)
    ring = (r > 12.3) & (r < 14.4)
    body = r < 9.6
    t = np.clip((v + 9) / 18, 0, 1)
    col = np.stack([lerp(core[1][i], core[0][i], t) for i in range(3)], -1)
    a.paint(ring.astype(np.float32), WHITE, 1.0)
    a.paint(body.astype(np.float32), col, 1.0)
    rim = (r > 8.2) & (r < 9.6)
    a.paint(rim.astype(np.float32), tuple(min(1, c * 0.6 + 0.4) for c in core[0]), 0.9)
    return a


def orb_glow_art(kind='yellow'):
    c = {'yellow': hexc('ffff00'), 'pink': hexc('ff00ff'), 'blue': hexc('00ffff')}[kind]
    a = Art(px(40) + 1, px(40) + 1)
    u, v = a.grid()
    r = np.hypot(u, v)
    g = np.clip(1 - np.abs(r - 13.5) / 6.5, 0, 1) ** 1.5 * 0.6
    a.rgb[:] = c
    a.a = g.astype(np.float32)
    return a


# ---------------------------------------------------------------- portals

PORTAL_STYLE = {
    # light, mid, dark rim colours
    'grav_blue': (hexc('b8ffff'), hexc('3ad4ff'), hexc('0078ff')),
    'grav_yellow': (hexc('fffbb0'), hexc('ffdc30'), hexc('ff9c00')),
    'cube': (hexc('c8ffc0'), hexc('3cff3c'), hexc('00b400')),
    'ship': (hexc('ffc8ff'), hexc('ff5cff'), hexc('d000d0')),
}


def portal_arc(a, cx, rx, ry, thick, colors, alpha=1.0):
    """Bright crescent: outer ellipse minus a shifted inner ellipse."""
    light, mid, dark = colors
    u, v = a.grid()
    e_out = ((u - cx) / rx) ** 2 + (v / ry) ** 2
    e_in = ((u - cx - thick) / (rx - thick * 0.4)) ** 2 + (v / (ry - 2.5)) ** 2
    cres = (e_out < 1) & (e_in > 1) & (u < cx + rx * 0.2)
    # colour: bright core line near the outer edge, darker towards the tips
    t = np.clip(np.abs(v) / ry, 0, 1)
    col = np.stack([lerp(lerp(light[i], mid[i], 0.35), dark[i], t ** 1.6) for i in range(3)], -1)
    a.paint(cres.astype(np.float32), col, alpha)
    edge = cres & (e_out > 0.80)
    a.paint(edge.astype(np.float32), WHITE, 0.85 * alpha)


def gravity_portal_art(style, part):
    """Gravity portals (IDs 10/11): crescent with black teeth on the right."""
    colors = PORTAL_STYLE[style]
    a = Art(px(34) + 1, px(80) + 1)
    u, v = a.grid()
    if part == 'back':
        portal_arc(a, -2.0, 10.0, 37.0, 6.0, colors, 0.95)
        return a
    # front: crescent plus the stepped black shell with teeth
    for i in range(7):
        y0 = -30 + i * 9
        tooth = a.rect_mask(2.5, y0, 6.5 + 2.5 * math.cos((y0 + 4) / 38 * math.pi / 2) , y0 + 6.5)
        a.paint(tooth, BLACK, 0.95)
    shell = ((u - 1.5) / 5.5) ** 2 + (v / 34.0) ** 2 < 1
    a.paint(shell.astype(np.float32), BLACK, 0.9)
    portal_arc(a, -4.0, 9.5, 37.0, 6.0, colors, 1.0)
    return a


def mode_portal_art(style, part):
    """Cube / ship portals (IDs 12/13): crescent, black cage with windows, dots."""
    colors = PORTAL_STYLE[style]
    light, mid, dark = colors
    a = Art(px(40) + 1, px(92) + 1)
    u, v = a.grid()
    if part == 'back':
        portal_arc(a, -4.0, 11.0, 38.0, 6.5, colors, 0.95)
        cage = ((u - 1.0) / 8.0) ** 2 + (v / 34.0) ** 2 < 1
        a.paint(cage.astype(np.float32), BLACK, 0.85)
        return a
    cage = ((u - 3.5) / 10.5) ** 2 + (v / 38.0) ** 2 < 1
    a.paint(cage.astype(np.float32), BLACK, 0.95)
    # window slots
    for row in range(-3, 4):
        for col in range(2):
            x0 = 1.5 + col * 5.0
            y0 = row * 9.5 - 1.8
            slot = a.rect_mask(x0, y0, x0 + 3.2, y0 + 3.6)
            a.paint(slot, mid, 0.55)
    portal_arc(a, -5.5, 10.5, 39.0, 6.5, colors, 1.0)
    for (dx, dy) in ((1.5, 43.0), (1.5, -43.0), (9.5, 14.0), (9.5, -14.0)):
        dot = a.ellipse_mask(dx, dy, 2.6, 2.6)
        a.paint(dot, mid, 1.0)
        a.paint(a.ellipse_mask(dx - 0.6, dy + 0.6, 1.0, 1.0), light, 0.9)
    return a


# ---------------------------------------------------------------- coins

GOLD = (hexc('ffe25a'), hexc('ffb000'), hexc('c86400'), hexc('3a1e00'))


def coin_art(frame):
    """Secret coin, four spin frames (0 face, 1 3/4, 2 edge, 3 back 3/4)."""
    light, mid, dark, rim = GOLD
    a = Art(px(40) + 1, px(40) + 1)
    u, v = a.grid()
    sx = [1.0, 0.75, 0.2, 0.75][frame]
    r = 19.0
    e = (u / (r * sx)) ** 2 + (v / r) ** 2
    disc = e < 1
    a.paint(disc.astype(np.float32), rim, 1.0)
    inner = (u / (r * sx - 2.2 * max(sx, 0.4))) ** 2 + (v / (r - 2.2)) ** 2 < 1
    t = np.clip((v + r) / (2 * r), 0, 1)
    col = np.stack([lerp(dark[i], light[i], t) for i in range(3)], -1)
    a.paint(inner.astype(np.float32), col, 1.0)
    if frame != 2:
        face = (u / (r * sx - 5.5 * sx)) ** 2 + (v / (r - 5.5)) ** 2 < 1
        a.paint(face.astype(np.float32), mid, 0.9)
        pts = []
        for i in range(10):
            ang = math.pi / 2 + i * math.pi / 5
            rr = 10.0 if i % 2 == 0 else 4.2
            pts.append((rr * math.cos(ang) * sx, rr * math.sin(ang) - 0.8))
        star = a.poly_mask(pts)
        a.paint(star, dark if frame == 3 else hexc('ffd23c'), 1.0)
        a.paint(np.clip(star - a.poly_mask([(x * 0.7, y * 0.7 + 0.8) for x, y in pts]), 0, 1), dark, 0.6)
    else:
        a.paint(a.rect_mask(-2.0, -r + 2, 2.0, r - 2), light, 0.8)
    shine = a.ellipse_mask(-6 * sx, 9, 3.5 * sx + 0.5, 2.2)
    a.paint(shine, WHITE, 0.55)
    return a


# ---------------------------------------------------------------- player icons

def cube_layers():
    """Default cube: primary (green) frame and secondary (cyan) centre.
    Returns (primary LA44 art, secondary LA44 art) drawn at 1x."""
    p = Art(px(30), px(30))
    u, v = p.grid()
    au, av = np.abs(u), np.abs(v)
    m = np.maximum(au, av)
    body = m < 15
    p.paint(body.astype(np.float32), WHITE, 1.0)
    # outer black outline and a slightly darker bevel just inside it
    p.paint((body & (m > 13.6)).astype(np.float32), BLACK, 1.0)
    p.paint((body & (m > 12.4) & (m <= 13.6)).astype(np.float32), (0.72, 0.72, 0.72), 1.0)
    # inner square: black border, transparent ring, black border around centre
    p.paint(((m > 5.4) & (m < 8.0)).astype(np.float32), BLACK, 1.0)
    p.erase(((m <= 5.4) & (m > 4.2)).astype(np.float32))
    p.paint(((m <= 4.2) & (m > 2.9)).astype(np.float32), BLACK, 1.0)
    p.erase((m <= 2.9).astype(np.float32))
    s = Art(px(30), px(30))
    s.paint((m <= 4.0).astype(np.float32), WHITE, 1.0)
    return p, s


def ship_layers():
    """Default ship: primary hull and secondary canopy. Anchor = ship centre."""
    p = Art(px(40) + 1, px(26) + 1)
    s = Art(px(40) + 1, px(26) + 1)
    hull = [(-18, -2), (-12, -8), (10, -8), (18, -3), (18, 2), (8, 5), (-12, 5), (-18, 1)]
    nose_cut = [(18, -3), (18, 2), (15, 0)]
    fin = [(-17, 2), (-13, 10), (-9, 10), (-10, 3)]
    p.paint(p.poly_mask(hull), BLACK, 1.0)
    inner = inset_polygon(hull[::-1], 1.6)[::-1]
    p.paint(p.poly_mask(inner), WHITE, 1.0)
    # panel lines
    for x in (-6.0, 4.0):
        p.paint(p.rect_mask(x, -6.5, x + 1.2, 3.5), BLACK, 0.9)
    p.paint(p.poly_mask(fin), BLACK, 1.0)
    p.paint(p.poly_mask(inset_polygon(fin[::-1], 1.2)[::-1]), WHITE, 1.0)
    p.paint(p.rect_mask(-16, -6, -12, -1), (0.6, 0.6, 0.6), 1.0)
    # secondary: canopy stripe along the upper hull
    s.paint(s.poly_mask([(-9, 1), (7, 1), (12, -1.5), (-9, -1.5)]), WHITE, 1.0)
    return p, s


# ---------------------------------------------------------------- registry

def game_sprites():
    """Ordered list of (name, art, fmt)."""
    out = []
    add = lambda n, a, f: out.append((n, a, f))
    add('block', block_art('ltrb'), 'LA44')
    add('grid_t', block_art('t', grid=True), 'LA44')
    add('grid_tl', block_art('tl', grid=True), 'LA44')
    add('grid_c', block_art('c', grid=True), 'LA44')
    add('grid_none', block_art('', grid=True), 'LA44')
    add('grid_ltr', block_art('ltr', grid=True), 'LA44')
    add('grid_lr', block_art('lr', grid=True), 'LA44')
    add('glow_all', glow_art('ltrb'), 'A4')
    add('glow_t', glow_art('t'), 'A4')
    add('glow_tl', glow_art('tl'), 'A4')
    add('glow_c', glow_art('c', reach=5.0), 'A4')
    add('glow_ltr', glow_art('ltr'), 'A4')
    add('glow_lr', glow_art('lr'), 'A4')
    add('spike', spike_art(), 'LA44')
    add('spike_small', spike_art(30.0, 14.0, -7.0), 'LA44')
    add('spike_med', spike_art(20.0, 19.0, -9.5), 'LA44')
    add('glow_spike', spike_glow_art(), 'A4')
    add('glow_spike_small', spike_glow_art(30.0, 14.0, -7.0), 'A4')
    add('glow_spike_med', spike_glow_art(20.0, 19.0, -9.5), 'A4')
    for i in range(3):
        add('pit%d' % i, pit_art(11 + i * 7), 'A4')
    add('plank', plank_art(), 'LA44')
    add('glow_plank', glow_art('ltrb', size=30, reach=5.0, height=14), 'A4')
    for i in range(4):
        add('slab%d' % i, slab_wave_art(i), 'LA44')
    add('rod1', rod_art(42.0), 'A4')
    add('rod2', rod_art(27.0), 'A4')
    add('rod3', rod_art(13.0), 'A4')
    add('rod_ball', ball_art(15.0), 'A4')
    add('dspikes1', mountains_art(128, 42, 9, 1), 'A4')
    add('dspikes2', mountains_art(104, 37, 7, 2), 'A4')
    add('dspikes3', mountains_art(73, 29, 6, 3), 'A4')
    add('dspikes4', mountains_art(42, 13, 4, 4), 'A4')
    add('chain', chain_art(70.0, 5), 'A4')
    add('chain_short', chain_art(34.0, 2), 'A4')
    add('star', star_art(), 'A4')
    add('diag_block', diag_block_art(), 'LA44')
    for k in ('yellow', 'blue', 'pink'):
        add('pad_' + k, pad_art(k), 'PAL4')
        add('glow_pad_' + k, pad_glow_art(k), 'A4')
    for k in ('yellow', 'blue', 'pink'):
        add('orb_' + k, orb_art(k), 'PAL4')
        add('glow_orb_' + k, orb_glow_art(k), 'A4')
    for st in ('grav_blue', 'grav_yellow'):
        add('portal_%s_back' % st, gravity_portal_art(st, 'back'), 'PAL4')
        add('portal_%s_front' % st, gravity_portal_art(st, 'front'), 'PAL4')
    for st in ('cube', 'ship'):
        add('portal_%s_back' % st, mode_portal_art(st, 'back'), 'PAL4')
        add('portal_%s_front' % st, mode_portal_art(st, 'front'), 'PAL4')
    for f in range(4):
        add('coin%d' % f, coin_art(f), 'PAL4')
    cp, cs = cube_layers()
    add('cube1_p', cp, 'LA44')
    add('cube1_s', cs, 'LA44')
    sp, ssec = ship_layers()
    add('ship1_p', sp, 'LA44')
    add('ship1_s', ssec, 'LA44')
    return out
