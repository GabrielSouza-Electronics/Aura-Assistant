# Typography review — pending visual approval

Requested: bring all remaining UI text/assets up to the calendar's rendering
quality, especially larger Settings text. User approved implementation.

Run `python -B aura_assets/gen/preview_typography.py` to reproduce these outputs.
The script reuses the current vector panel/icon generators and creates only
review assets in this directory. It never invokes their installation routines.

## Proposal

- Poppins Medium, 6x supersampling, one Lanczos downsample to native dimensions.
- Constant light text color with alpha antialiasing, matching calendar glyphs;
  remove the old text color ramp and sharpening. Reduce letter spacing to
  allow larger letters within the same circular display.
- Settings labels 12 -> 18 px; values 11 -> 18 px; embedded icons 26 -> 30 px.
- Menu titles 20 -> 24 px, context/subtitle 9 -> 12 px, generic messages
  12 -> 14 px, READY 30 px, WAVE TO BEGIN 14 px. Keep emerald selection.
- Regenerate the five menu icons at 36/48/64 px and Wi-Fi/battery symbols at 6x.
  Preserve the existing circuit, logo/hero design, rim and calendar styling.

105 candidate PNG assets validated: <=256 RGBA colors, dithering disabled,
transparent sprite edges. Settings layouts tested with 100%, 10/10, and a
32-character SSID truncated with an ellipsis. Review native 480x480 images;
enlarging a screen preview is not a higher-resolution LCD.

## Review outputs

- `settings.png`: normal preview with increased typography.
- `settings_leak.png`: lightened background for contrast inspection.
- `settings_long_ssid.png`: name truncation and alternate values.
- `comparison.png`: old vs proposed menu/standby text and menu icons.
- `manifest.json`: native dimensions, palette counts, sizes and glyph advances.

## Implementation

Installed via `python -B aura_assets/gen/install_typography.py`.
Glyph advances now use uint16_t (maximum 301 in 1/16 px units), value columns
use the enlarged label widths, and GUI centering reads actual bitmap dimensions.
Gesture hints use separate 14 px pixel sprites in QSPI. Existing Designer image
names and rotation settings are preserved. Settings embedded icons use 30 px;
the proposal's Python default argument had retained 26 px despite its 30 px
configuration; installation corrects that default to match the requested size.

Official TouchGFX image conversion and Debug compilation/linking passed.
See `tests/TYPOGRAPHY_VALIDATION.md` for validation and memory use.
