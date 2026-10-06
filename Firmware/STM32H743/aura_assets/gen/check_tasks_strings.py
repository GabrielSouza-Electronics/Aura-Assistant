"""Audit installed firmware glyph coverage and deadline layout metrics.

This checks the existing Settings atlas, not a Tasks implementation.
"""
from pathlib import Path
import calendar
import json
import re

ROOT = Path(__file__).resolve().parents[2]


def main():
    header = (ROOT / 'TouchGFX/gui/include/gui/common/SettingsGlyphs.hpp').read_text()
    glyphs = {}
    for code, width, advance in re.findall(r"BITMAP_G_([0-9A-F]+)_ID, (\d+), (\d+)", header):
        glyphs[chr(int(code, 16))] = (int(width), int(advance))
        assert (ROOT / f'TouchGFX/assets/images/aura/glyphs/g_{code}.png').is_file()
        assert f'BITMAP_G_{code}_ID' in (ROOT / 'TouchGFX/generated/images/include/images/BitmapDatabase.hpp').read_text()
    space = int(re.search(r'SG_SPACE_ADV16 = (\d+)', header)[1])
    pad = int(re.search(r'SG_CELL_PAD = (\d+)', header)[1])
    limit = 168  # Preview deadline x=135 through tag x=303.
    failures = set()
    longest = {}
    totals = {}
    overflow = {}

    def check(value, group):
        text = value.upper()
        failures.update(c for c in text if c != ' ' and c not in glyphs)
        x16, extent = 0, 0
        for c in text:
            if c == ' ':
                x16 += space
            else:
                width, advance = glyphs.get(c, glyphs['?'])
                extent = max(extent, ((x16+8) >> 4) + width)
                x16 += advance
        totals[group] = totals.get(group, 0)+1
        if extent > longest.get(group, {}).get('width_px', 0):
            longest[group] = {'text': value, 'width_px': extent}
        if extent > limit+2*pad:
            overflow[group] = overflow.get(group, 0)+1

    times = [f'{h:02}:{m:02}' for h in range(24) for m in range(60)]
    for value in times:
        check(value, 'today')
        for word in ('Yesterday', 'Tomorrow'):
            check(f'{word} - {value}', word)
    for month in range(1, 13):
        for day in range(1, calendar.monthrange(2028, month)[1]+1):
            for value in times:
                check(f'{day:02}/{month:02} - {value}', 'date_time')
    for word in ('Personal', 'Work', 'Priority', 'Project',
                 'January', 'February', 'March', 'April', 'May', 'June',
                 'July', 'August', 'September', 'October', 'November', 'December'):
        check(word, 'labels_months')
    assert not failures, failures
    report = {'scope': 'Installed Settings atlas; Tasks preview is not firmware',
              'cases': totals, 'total_cases': sum(totals.values()),
              'missing_characters_with_hyphen': sorted(failures),
              'missing_requested_separator': ['•'] if '•' not in glyphs else [],
              'firmware_case': 'UPPERCASE', 'available_deadline_width_px': limit,
              'widest': longest, 'overflow_cases': overflow}
    out = ROOT / 'aura_assets/preview/tasks_strings_report.json'
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps(report, indent=2, ensure_ascii=True))


if __name__ == '__main__':
    main()
