"""Normalize user-selected Special Lineal downloads into UI-only tint masks.
Usage: python import-special-lineal.py PATH_TO_DOWNLOADS
Requires Pillow at build time only. No runtime download or icon font dependency.
Source files are not redistributed as a standalone stock collection.
"""
import hashlib
import json
from pathlib import Path
import sys
from PIL import Image, ImageChops, ImageOps

ROOT=Path(__file__).resolve().parents[2]
SOURCES=[
    ('site-magnific-com-icon-special-lineal-se-1.png',['preferences','advanced'],'https://www.magnific.com/icon/settings_2274355'),
    ('site-magnific-com-icon-special-lineal-he-1.png',['devices','headphones'],'https://magnific.com/no/ikon/headphones_1376488'),
    ('site-magnific-com-icon-special-lineal-ra-1.png',['memory'],'https://freepik.com/icon/ram_7934030'),
    ('special-lineal-palette-icon-1.png',['palette','graphics'],'https://de.freepik.com/icon/paletten_4228810'),
    ('special-lineal-processor-icon-1.png',['cpu','performance'],'https://www.freepik.com/icon/processor_2303054'),
    ('special-lineal-keyboard-icon-1.png',['shortcuts'],'https://www.freepik.com/icon/hands-keyboard_103677'),
]

def main():
    downloads=Path(sys.argv[1]);out=ROOT/'assets/full-emulator-ui/StudioIcons';rows=[]
    for filename,kinds,url in SOURCES:
        raw=(downloads/filename).read_bytes()
        image=Image.open(downloads/filename).convert('RGBA')
        # Some previews have opaque white interiors: alpha alone would tint
        # their whole silhouette. Retain dark linework only, with antialiasing.
        alpha=ImageChops.multiply(image.getchannel('A'),ImageOps.invert(image.convert('RGB').convert('L')))
        bounds=alpha.getbbox()
        if not bounds:raise ValueError('Empty icon: '+filename)
        mask=alpha.crop(bounds);mask.thumbnail((54,54),Image.Resampling.LANCZOS)
        canvas=Image.new('RGBA',(64,64),(255,255,255,0));a=Image.new('L',(64,64));a.paste(mask,((64-mask.width)//2,(64-mask.height)//2));canvas.putalpha(a)
        for kind in kinds:canvas.save(out/(kind+'.png'))
        rows.append({'source':url,'sourceSha256':hashlib.sha256(raw).hexdigest(),'kinds':kinds,'outputSha256':hashlib.sha256((out/(kinds[0]+'.png')).read_bytes()).hexdigest(),'modifications':'Dark linework retained, cropped, padded, resized to 64px; theme tint applied at runtime.'})
    manifest={'family':'Special Lineal','creator':'Magnific / Freepik','collection':'https://www.magnific.com/author/magnific/icons/special-lineal_7','license':'Magnific Content license; NOT MIT. Attribution required. UI incorporation only, no standalone stock/icon redistribution.','terms':'https://www.magnific.com/legal/terms-of-use','icons':rows,'unchangedKinds':'Other symbols retain the existing project-drawn Studio family; no claim that they are Special Lineal.'}
    (ROOT/'assets/full-emulator-ui/THIRD-PARTY-ICONS.json').write_text(json.dumps(manifest,indent=2)+'\n')

if __name__=='__main__':main()
