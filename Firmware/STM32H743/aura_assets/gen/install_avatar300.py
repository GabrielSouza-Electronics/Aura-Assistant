"""Register supplied pre-rotated PNGs without changing Designer rotation."""
from pathlib import Path
import json
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
groups={'Idle':6,'blink':6,'LookUp':12,'Lookdown':5,'Lookright':8,'Lookleft':9,'Thinking':19,'Goodbye':22,'Coffee':26,'Smile':11}
config=ROOT/'TouchGFX/application.config'
data=json.loads(config.read_text())
entries=data['image_configuration']['images']
header=['#pragma once\n#include <BitmapDatabase.hpp>\n#include <gui/common/AvatarAnimation.hpp>\n']
for name,count in groups.items():
    ids=[]
    for i in range(1,count+1):
        rel=f'aura/avatar300/{name}/{name}{i}.png'
        image=Image.open(ROOT/'TouchGFX/assets/images'/rel).convert('RGBA')
        assert image.size==(300,300) and image.getcolors(256) is not None
        entry=entries.setdefault(rel.replace('/','\\'),{})
        for key,value in {'format':'L8_ARGB8888','dither_algorithm':'0','alpha_dither':'no','section':'ExtFlashSection','extra_section':'ExtFlashSection'}.items():
            entry.setdefault(key,value)
        ids.append(f'BITMAP_{name.upper()}{i}_ID')
    header.append(f'static const uint16_t avatar{name}Ids[] = {{'+','.join(ids)+'};\n')
header.append('static const uint16_t* const avatarIds[AvatarAnimation::Count] = {'+','.join('avatar'+name+'Ids' for name in groups)+'};\n')
(ROOT/'TouchGFX/gui/include/gui/common/AvatarBitmaps.hpp').write_text(''.join(header))
config.write_text(json.dumps(data,indent=2)+'\n')
