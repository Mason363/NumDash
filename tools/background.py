"""Background tile layout: rectangles of assorted sizes separated by bright
seams, in the style of Geometry Dash's default backdrop (tile 512 texels,
displayed at 0.9 px per texel)."""
import random

TILE = 512
GAP = 8


def split(x, y, w, h, rnd, depth, out):
    """Recursively split a region into panels; big panels stay whole."""
    big = w * h
    if depth > 3 or (w < 90 and h < 90) or (big < 42000 and rnd.random() < 0.55):
        out.append((x, y, w, h))
        return
    if w >= h:
        cut = rnd.choice([0.33, 0.4, 0.5, 0.6, 0.67])
        a = int(w * cut) // 8 * 8
        split(x, y, a, h, rnd, depth + 1, out)
        split(x + a, y, w - a, h, rnd, depth + 1, out)
    else:
        cut = rnd.choice([0.3, 0.4, 0.5, 0.6, 0.7])
        a = int(h * cut) // 8 * 8
        split(x, y, w, a, rnd, depth + 1, out)
        split(x, y + a, w, h - a, rnd, depth + 1, out)


def layout(seed=7):
    rnd = random.Random(seed)
    cols = [96, 128, 160, 128]
    out = []
    x = 0
    for cw in cols:
        y = 0
        while y < TILE:
            h = min(TILE - y, rnd.choice([112, 144, 176, 208, 96]))
            if TILE - y - h < 64:
                h = TILE - y
            split(x, y, cw, h, rnd, 0, out)
            y += h
        x += cw
    # shrink by the seam so neighbours are separated by GAP texels
    rects = []
    for (x, y, w, h) in out:
        rects.append((x + GAP // 2, y + GAP // 2, w - GAP, h - GAP))
    return rects


if __name__ == '__main__':
    from PIL import Image
    import numpy as np
    img = np.ones((TILE, TILE)) * 1.0
    for (x, y, w, h) in layout():
        img[y:y + h, x:x + w] = 0.843
        img[y:y + h, x + w - 1] = 0.71
    grad = np.clip(np.arange(TILE) / 384.0, 0, 1)[:, None]
    Image.fromarray((img * grad * 255).astype('uint8')).save('/tmp/bg_layout.png')
    print(len(layout()))
