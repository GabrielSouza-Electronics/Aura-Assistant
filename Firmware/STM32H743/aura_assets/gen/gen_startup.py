"""Approved startup text: fixed dot positions, native antialiasing, QSPI pixels."""
from pathlib import Path
import numpy as np
from PIL import Image
import gen_text as gt
import preview_startup as preview

ROOT=Path(__file__).resolve().parents[2]

def main():
    label=gt.render_text('STARTING, PLEASE WAIT',14,gt.FONT_MED)
    dot=gt.render_text('.',14,gt.FONT_MED)
    sprites=[]
    for count in range(4):
        im=Image.new('RGBA',(300,24))
        preview.center(im,label,141,12)
        for i in range(count):
            preview.center(im,dot,141+label.width/2+2+i*5,12)
        sprites.append(im)
    sprites.extend([gt.render_text('DEVELOPED BY:',11,gt.FONT_MED),
                    gt.render_text('GABRIEL SOUZA',16,gt.FONT_MED)])
    out=ROOT/'aura_assets/compiled/startup'
    out.mkdir(parents=True,exist_ok=True)
    cpp=['#include "StartupAssets.hpp"\n#include <touchgfx/hal/Config.hpp>\n']
    entries=[]
    for i,im in enumerate(sprites):
        rgba=np.asarray(im.rotate(180),dtype=np.uint32)
        assert len(np.unique(rgba.reshape(-1,4),axis=0))<=256
        assert not np.any(rgba[[0,-1],:,3]) and not np.any(rgba[:,[0,-1],3])
        words=((rgba[:,:,3]<<24)|(rgba[:,:,0]<<16)|(rgba[:,:,1]<<8)|rgba[:,:,2]).ravel()
        cpp.append(f'LOCATION_PRAGMA("ExtFlashSection")\nKEEP static const uint32_t pixels{i}[] LOCATION_ATTRIBUTE("ExtFlashSection") = {{\n')
        cpp.extend('    '+','.join(f'0x{v:08x}' for v in words[j:j+12])+',\n' for j in range(0,len(words),12))
        cpp.append('};\n')
        entries.append(f'    {{ pixels{i}, {im.height}, {im.width} }}')
    cpp.append('const StartupSprite startupSprites[6] = {\n'+',\n'.join(entries)+'\n};\n')
    (out/'StartupAssets.hpp').write_text('#pragma once\n#include <stdint.h>\nstruct StartupSprite { const uint32_t* pixels; int16_t width,height; };\nextern const StartupSprite startupSprites[6];\n')
    (out/'StartupAssets.cpp').write_text(''.join(cpp))
    preview.main()
    print('6 startup sprites validated and generated.')

if __name__=='__main__':
    main()
