"""将源图标裁切为透明圆形，并同步生成项目所需图标。"""

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "icon.png"
PNG_TARGETS = (
    SOURCE,
    ROOT / "website" / "public" / "assets" / "icon.png",
    ROOT / "website" / "public" / "favicon.png",
)
ICO_TARGET = ROOT / "assets" / "icon.ico"
ICON_SIZE = 1024
MASK_SCALE = 4


def circular_icon(source: Path) -> Image.Image:
    image = Image.open(source).convert("RGBA")
    side = min(image.size)
    left = (image.width - side) // 2
    top = (image.height - side) // 2
    image = image.crop((left, top, left + side, top + side))
    image = image.resize((ICON_SIZE, ICON_SIZE), Image.Resampling.LANCZOS)

    mask_size = ICON_SIZE * MASK_SCALE
    mask = Image.new("L", (mask_size, mask_size), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, mask_size - 1, mask_size - 1), fill=255)
    mask = mask.resize((ICON_SIZE, ICON_SIZE), Image.Resampling.LANCZOS)
    image.putalpha(mask)
    return image


def main() -> None:
    image = circular_icon(SOURCE)
    for target in PNG_TARGETS:
        target.parent.mkdir(parents=True, exist_ok=True)
        image.save(target, "PNG", optimize=True)

    image.save(
        ICO_TARGET,
        "ICO",
        sizes=((16, 16), (24, 24), (32, 32), (48, 48), (64, 64),
               (128, 128), (256, 256)),
    )


if __name__ == "__main__":
    main()
