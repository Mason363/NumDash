"""Bitmap fonts in the style of Geometry Dash's outlined title lettering.

Glyphs are rasterized from two SIL Open Font License typefaces (Rammetto One
for display text, Nunito Black for small labels; see LICENSES/).
Only the 4-bit fill coverage is stored (A4 sprites); the renderer derives the
black outline and drop shadow at draw time, which keeps the fonts small.
"""
from pathlib import Path
import hashlib
import urllib.request
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent / 'fonts'
UP = 4   # supersampling for rasterization

SOURCES = {
    'RammettoOne-Regular.ttf': ('https://raw.githubusercontent.com/google/fonts/main/ofl/rammettoone/RammettoOne-Regular.ttf',
                                '0063c5480f7868be5dd95dd668575643ff953199541dce65fe5a88310bd7733f'),
    'Nunito-Black.ttf': ('https://raw.githubusercontent.com/google/fonts/main/ofl/nunito/Nunito%5Bwght%5D.ttf',
                         'bb55a5ca5c2042335b3991af27c4d0705d0ef41cac6164ac737fd8f2a1e85207'),
}

# name, file, cap height px, outline radius px, shadow dx, dy, alpha (0..15), variation
SPECS = [
    ('small', 'Nunito-Black.ttf', 7, 1, 1, 1, 8, [1000]),
    ('big', 'RammettoOne-Regular.ttf', 13, 1, 1, 2, 9, None),
    ('huge', 'RammettoOne-Regular.ttf', 19, 2, 2, 2, 9, None),
]
CHARS = " !%'()+,-./0123456789:?ABCDEFGHIJKLMNOPQRSTUVWXYZ"


def font_path(fname):
    p = HERE / fname
    if not p.exists():
        url, digest = SOURCES[fname]
        HERE.mkdir(exist_ok=True)
        data = urllib.request.urlopen(url).read()
        if hashlib.sha256(data).hexdigest() != digest:
            raise RuntimeError('unexpected font data for ' + fname)
        p.write_bytes(data)
    return p


def load_font(fname, cap, var):
    for size in range(4, 200):
        f = ImageFont.truetype(str(font_path(fname)), size * UP)
        if var:
            try:
                f.set_variation_by_axes(var)
            except Exception:
                pass
        bb = f.getbbox('H')
        if bb[3] - bb[1] >= cap * UP - UP // 2:
            return f
    raise RuntimeError('font size')


def render_glyph(font, ch):
    """Fill coverage at 1x with the pen origin on a pixel corner.
    Returns (RGBA float image, anchor x, anchor y (baseline), advance)."""
    adv = font.getlength(ch) / UP
    if not ch.strip():
        return np.zeros((1, 1, 4), np.float32), 0, 0, adv
    bb = font.getbbox(ch, anchor='ls')
    pad = 2 * UP
    x_org = pad - bb[0]
    y_org = pad - bb[1]
    # align origin to the UP grid so the baseline lands on a pixel boundary
    x_org += (-x_org) % UP
    y_org += (-y_org) % UP
    W = x_org + bb[2] + pad
    H = y_org + bb[3] + pad
    W += (-W) % UP
    H += (-H) % UP
    im = Image.new('L', (W, H), 0)
    ImageDraw.Draw(im).text((x_org, y_org), ch, font=font, fill=255, anchor='ls')
    a = np.asarray(im).astype(np.float32) / 255
    a = a.reshape(H // UP, UP, W // UP, UP).mean(axis=(1, 3))
    img = np.zeros(a.shape + (4,), np.float32)
    img[..., :3] = 1
    img[..., 3] = a
    return img, x_org // UP, y_org // UP, adv


def build_fonts(packer):
    out = []
    for name, fname, cap, outline, sdx, sdy, sa, var in SPECS:
        font = load_font(fname, cap, var)
        base = len(packer.sprites)
        adv = []
        for ch in CHARS:
            img, ax, ay, a = render_glyph(font, ch)
            packer.add('glyph_%s_%d' % (name, ord(ch)), img, ax, ay, 'A4')
            adv.append(max(1, int(round(a))))
        out.append(dict(name=name, cap=cap, outline=outline, sdx=sdx, sdy=sdy, sa=sa, base=base, advance=adv))
    return out
