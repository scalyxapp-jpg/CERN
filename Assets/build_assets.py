"""Baut die finalen UI-Assets aus den generate-image-Rohbildern.

Aufruf: python3 Assets/build_assets.py
Erzeugt:
  Assets/logo-emblem.png   (transparent, 512x512)
  Assets/knob-cap.png      (freigestellte Metallkappe, 512x512, transparent)
  Assets/metal-brushed.png (512x512 Kachel)
  Assets/fader-track.png   (128x512 Kachel)
  Assets/grain.png         (128x128 subtiles Rauschen, transparent)
"""

import random
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent
RAW = ROOT / "raw"


def black_to_alpha(image: Image.Image, floor: int = 10) -> Image.Image:
    """Macht einen schwarzen Hintergrund transparent (Screen-Matte).

    alpha = max(r,g,b); Farbe wird entpremultipliziert, damit helle Kanten
    und das Rot sauber erhalten bleiben.
    """
    rgb = image.convert("RGB")
    out = Image.new("RGBA", rgb.size)
    src = rgb.load()
    dst = out.load()
    for y in range(rgb.height):
        for x in range(rgb.width):
            r, g, b = src[x, y]
            a = max(r, g, b)
            if a <= floor:
                dst[x, y] = (0, 0, 0, 0)
                continue
            scale = 255.0 / a
            dst[x, y] = (
                min(255, int(r * scale)),
                min(255, int(g * scale)),
                min(255, int(b * scale)),
                a,
            )
    return out


def trim_to_content(image: Image.Image, margin_ratio: float = 0.06) -> Image.Image:
    box = image.getchannel("A").getbbox()
    if box is None:
        return image
    margin = int(max(box[2] - box[0], box[3] - box[1]) * margin_ratio)
    box = (
        max(0, box[0] - margin),
        max(0, box[1] - margin),
        min(image.width, box[2] + margin),
        min(image.height, box[3] + margin),
    )
    return image.crop(box)


def build_logo() -> None:
    src = Image.open(RAW / "logo-emblem-v2.jpg")
    logo = black_to_alpha(src)
    logo = trim_to_content(logo, 0.10)
    side = max(logo.size)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(logo, ((side - logo.width) // 2, (side - logo.height) // 2), logo)
    canvas = canvas.resize((512, 512), Image.LANCZOS)
    canvas.save(ROOT / "logo-emblem.png")


def build_knob_cap() -> None:
    src = Image.open(RAW / "knob-cap-v1.jpg").convert("RGB")
    # Kappe freistellen: Kreis-Maske mit weicher Kante.
    size = src.size[0]
    mask = Image.new("L", src.size, 0)
    draw = ImageDraw.Draw(mask)
    radius = int(size * 0.445)
    cx = cy = size // 2
    draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(1.2))
    cap = src.convert("RGBA")
    cap.putalpha(mask)
    bbox = (cx - radius, cy - radius, cx + radius, cy + radius)
    cap = cap.crop(bbox).resize((512, 512), Image.LANCZOS)
    cap.save(ROOT / "knob-cap.png")


def _make_seamless(image: Image.Image, blend: int = 48) -> Image.Image:
    """Blendet gegenueberliegende Kanten weich, um Kachelnaehte zu vermeiden."""
    img = image.convert("RGB")
    w, h = img.size
    b = max(2, min(blend, w // 4, h // 4))

    # Horizontal: linker Rand bekommt Verlauf vom rechten Rand.
    left = img.crop((0, 0, b, h))
    right = img.crop((w - b, 0, w, h))
    mask_h = Image.linear_gradient("L").resize((b, h)).transpose(Image.FLIP_LEFT_RIGHT)
    img.paste(Image.composite(right, left, mask_h), (0, 0))

    # Vertikal: oberer Rand bekommt Verlauf vom unteren Rand.
    top = img.crop((0, 0, w, b))
    bottom = img.crop((0, h - b, w, h))
    mask_v = (
        Image.linear_gradient("L")
        .rotate(90, expand=True)
        .resize((w, b))
        .transpose(Image.FLIP_TOP_BOTTOM)
    )
    img.paste(Image.composite(bottom, top, mask_v), (0, 0))

    return img


def build_metal() -> None:
    src = Image.open(RAW / "metal-brushed-v1.jpg").convert("RGB")
    src = src.resize((512, 512), Image.LANCZOS)
    src = _make_seamless(src, blend=56)
    src.save(ROOT / "metal-brushed.png")


def build_track() -> None:
    src = Image.open(RAW / "fader-track-v1.jpg").convert("RGB")
    src = src.resize((128, 512), Image.LANCZOS)
    # Wird gestreckt dargestellt; Kanten glatten, damit keine harten Bildraender sichtbar sind.
    src = _make_seamless(src, blend=18)
    src.save(ROOT / "fader-track.png")


def build_grain() -> None:
    random.seed(7)
    size = 128
    base = Image.new("RGBA", (size, size))
    px = base.load()
    for y in range(size):
        for x in range(size):
            v = random.randint(0, 255)
            a = 10 if v > 140 else (5 if v > 90 else 0)
            px[x, y] = (v, v, v, a)

    # Kachelbar machen: Offset um halbe Kachel + Kantenblend eliminiert Naehte.
    offset = ImageChops.offset(base, size // 2, size // 2)
    blend = 20
    mask = Image.new("L", (size, size), 0)
    mpx = mask.load()
    for y in range(size):
        dy = min(y, size - 1 - y)
        for x in range(size):
            dx = min(x, size - 1 - x)
            d = min(dx, dy)
            mpx[x, y] = 255 if d >= blend else int(255 * d / blend)
    tile = Image.composite(base, offset, mask)
    tile = tile.filter(ImageFilter.GaussianBlur(0.35))
    tile.save(ROOT / "grain.png")


if __name__ == "__main__":
    build_logo()
    build_knob_cap()
    build_metal()
    build_track()
    build_grain()
    for name in ("logo-emblem", "knob-cap", "metal-brushed", "fader-track", "grain"):
        p = ROOT / f"{name}.png"
        im = Image.open(p)
        alpha = im.getchannel("A") if im.mode == "RGBA" else None
        bbox = alpha.getbbox() if alpha else None
        print(f"{p.name}: {im.size} {im.mode} bytes={p.stat().st_size} content={bbox}")
