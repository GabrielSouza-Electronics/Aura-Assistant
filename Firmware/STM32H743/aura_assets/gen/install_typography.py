"""Install approved native-resolution assets without deleting unrelated files."""
from pathlib import Path
import numpy as np
from PIL import Image
import preview_typography as p

ROOT = Path(__file__).resolve().parents[2]

def main():
    p.main()
    for src in sorted(p.SOURCE.glob('*.png')):
        name = src.stem
        if name.startswith(('settings_row', 'settings_glow', 'settings_sub_')):
            folder, name = 'settings', name[len('settings_'):]
        elif name.startswith('g_'):
            folder = 'glyphs'
        elif name.startswith(('lbl_', 'ctx_', 'msg_', 'sb_')):
            folder = 'text'
        elif name.startswith(('wifi_', 'batt_')):
            folder = 'status'
        else:
            folder = 'icons'
        im = Image.open(src).convert('RGBA')
        for base, rotated in [('aura_assets/gen', False),
                              ('aura_assets/assets_rotacionados', True),
                              ('TouchGFX/assets/images/aura', True)]:
            dest = ROOT/base/folder/(name+'.png')
            dest.parent.mkdir(parents=True, exist_ok=True)
            (im.rotate(90, expand=True) if rotated else im).save(dest)
    p.gs.write_layout(p.gs.build_all())
    p.gs.write_glyphs(*p.gs.build_glyphs())
    out = ROOT/'aura_assets/compiled/typography'
    out.mkdir(parents=True, exist_ok=True)
    header = ['#pragma once\n#include <stdint.h>\n',
              'struct TypographyHint { const uint32_t* pixels; int16_t width, height; };\n',
              'extern const TypographyHint typographyHints[2];\n']
    cpp = ['#include "TypographyHints.hpp"\n#include <touchgfx/hal/Config.hpp>\n']
    entries = []
    for i, label in enumerate(['RIGHT SELECT  LEFT BACK', 'UP/DOWN CHANGE  LEFT DONE']):
        im = p.text(label, 14, (184,223,243))
        assert im.width <= 330
        rgba = np.asarray(im.rotate(180), dtype=np.uint32)
        words = ((rgba[:,:,3]<<24)|(rgba[:,:,0]<<16)|(rgba[:,:,1]<<8)|rgba[:,:,2]).ravel()
        cpp.append(f'LOCATION_PRAGMA("ExtFlashSection")\nKEEP static const uint32_t hint{i}[] LOCATION_ATTRIBUTE("ExtFlashSection") = {{\n')
        cpp.extend('    '+','.join(f'0x{v:08x}' for v in words[j:j+12])+',\n' for j in range(0,len(words),12))
        cpp.append('};\n')
        entries.append(f'    {{ hint{i}, {im.height}, {im.width} }}')
    cpp.append('const TypographyHint typographyHints[2] = {\n'+',\n'.join(entries)+'\n};\n')
    (out/'TypographyHints.hpp').write_text(''.join(header))
    (out/'TypographyHints.cpp').write_text(''.join(cpp))
    print('Installed typography, layout, glyph metrics and gesture hints.')

if __name__ == '__main__':
    main()
