"""Migrate legacy raw sprite tables to Designer PNGs and bitmap IDs.

Also run after a legacy visual generator: it exports its new raw pixels, then
replaces the raw arrays with metadata. Never overrides Designer rotation.
"""
import json
import re
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
CONFIG = ROOT / "TouchGFX/application.config"
GROUPS = {"startup": "StartupAssets", "typography": "TypographyHints",
          "settings_style": "SettingsStyleAssets", "tasks": "TasksAssets",
          "calendar": "CalendarAssets"}


def main():
    config = json.loads(CONFIG.read_text())
    images = config["image_configuration"]["images"]
    total = 0
    for group, stem in GROUPS.items():
        directory = ROOT / "aura_assets/compiled" / group
        path = directory / (stem + ".cpp")
        source = path.read_text()
        pattern = r'LOCATION_PRAGMA\("ExtFlashSection"\)\s*KEEP static const uint32_t (\w+)\[\] LOCATION_ATTRIBUTE\("ExtFlashSection"\) = \{(.*?)\};'
        arrays = list(re.finditer(pattern, source, re.S))
        if not arrays:
            continue
        table = re.sub(pattern, '', source, flags=re.S)
        table = re.sub(r'reinterpret_cast<const uint8_t\*>\((\w+)\)', r'\1', table)
        for match in arrays:
            name = match[1]
            dimensions = re.search(r'\{\s*' + re.escape(name) + r'\s*,\s*(\d+)\s*,\s*(\d+)', table)
            assert dimensions, name
            width, height = map(int, dimensions.groups())
            if group == "calendar":
                width, height = height, width
            words = [int(v, 16) for v in re.findall(r'0x[0-9a-fA-F]+', match[2])]
            assert len(words) == width * height, (group, name)
            rgba = bytes(channel for v in words for channel in ((v >> 16) & 255, (v >> 8) & 255, v & 255, v >> 24))
            # Legacy arrays are logical W x H rotated 180, despite their
            # widget metadata storing H x W. Restore logical row stride first.
            # Pre-rotate left for Designer Layout Rotation 90, matching the
            # project's existing source PNG convention. Never reshape the
            # flat array directly to widget dimensions: that scrambles rows.
            image = Image.frombytes("RGBA", (height, width), rgba).transpose(Image.Transpose.ROTATE_270)
            filename = f"managed_{group}_{name}.png"
            relative = Path("aura/managed") / group / filename
            destination = ROOT / "TouchGFX/assets/images" / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            image.save(destination)
            # Lossless indexed color when <=256 RGBA colors; preserve larger palettes.
            fmt = "L8_ARGB8888" if image.getcolors(256) is not None else "ARGB8888"
            entry = images.setdefault(str(relative).replace('/', '\\'), {})
            for key, value in {"format": fmt, "dither_algorithm": "0", "alpha_dither": "no",
                               "section": "ExtFlashSection", "extra_section": "ExtFlashSection"}.items():
                entry.setdefault(key, value)
            bitmap_id = "BITMAP_" + filename[:-4].upper() + "_ID"
            table = re.sub(r'\b' + re.escape(name) + r'\b', bitmap_id, table)
            total += 1
        table = table.replace('#include <touchgfx/hal/Config.hpp>', '#include <BitmapDatabase.hpp>')
        table = re.sub(r'\n{3,}', '\n\n', table)
        path.write_text(table)
        header = directory / (stem + ".hpp")
        if header.exists():
            header.write_text(re.sub(r'const uint(?:8|32)_t\* pixels', 'uint16_t bitmapId', header.read_text()))
    CONFIG.write_text(json.dumps(config, indent=2) + '\n')
    print(f"Migrated {total} sprites; Designer controls their conversion settings.")


if __name__ == "__main__":
    main()
