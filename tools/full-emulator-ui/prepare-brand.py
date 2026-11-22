"""Prepare the user's supplied artwork without redrawing or recoloring it.
Requires Pillow only for offline asset preparation, not install/runtime.
Usage: python prepare-brand.py PATH-TO-USER-PNG
"""
import hashlib
import json
from pathlib import Path
import shutil
import sys
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
out=ROOT/'assets/full-emulator-ui/Branding'
out.mkdir(parents=True,exist_ok=True)
source=Path(sys.argv[1]);original=source.read_bytes()
shutil.copyfile(source,out/'VOID-Original.png')
image=Image.open(source).convert('RGBA')
# The supplied transparent image has a near-transparent full-canvas fringe.
# Trim only that fringe, preserving the visible artwork and its purple glow.
alpha=image.getchannel('A').point(lambda value:0 if value<=3 else value)
image.putalpha(alpha)
bounds=alpha.getbbox()
assert bounds,'No visible logo artwork.'
image=image.crop(bounds)
image.thumbnail((512,512),Image.Resampling.LANCZOS)
image.save(out/'VOID-Mark.png')
canvas=Image.new('RGBA',(256,256))
mark=image.copy();mark.thumbnail((232,232),Image.Resampling.LANCZOS)
canvas.alpha_composite(mark,((256-mark.width)//2,(256-mark.height)//2))
canvas.save(out/'VOID-Emulator.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
(out/'PROVENANCE.json').write_text(json.dumps({'source':'User-supplied image-40.png','sha256':hashlib.sha256(original).hexdigest(),'operations':['retain original bytes','trim alpha <= 3 fringe','crop transparent bounds','resize with Lanczos','center unchanged artwork for Windows shortcut icon'],'colorsChanged':False},indent=2)+'\n')
print('Prepared user artwork:',bounds,image.size)
