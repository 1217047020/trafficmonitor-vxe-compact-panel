"""Verify CPU-only raster changes against preserved v1; make focused preview."""
import json
import re
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT / 'work/cpu-icon-v1/preview'
NEW = ROOT / 'evidence/compact-preview'
before = json.loads((OLD / 'metrics.json').read_text(encoding='utf-8'))
after = json.loads((NEW / 'metrics.json').read_text(encoding='utf-8'))
scales = {row['dpi']: row for row in after['scales']}
for row in before['scales']:
    for field, value in row.items():
        assert scales[row['dpi']][field] == value, (row['dpi'], field)

checked = []
for path in sorted(NEW.glob('*.png')):
    match = re.fullmatch(r'(light|dark)-(\d+)-(.+)\.png', path.name)
    if not match or match[3].endswith('-4x'):
        continue
    a = Image.open(OLD / path.name).convert('RGB')
    b = Image.open(path).convert('RGB')
    assert a.size == b.size, path.name
    difference = ImageChops.difference(a, b)
    bbox = difference.getbbox()
    if match[3] == 'previous':
        assert bbox is None, path.name
    else:
        rect = scales[int(match[2])]['cpu_rect_px']
        assert bbox is not None, path.name
        assert bbox[0] >= rect[0] and bbox[1] >= rect[1], (path.name, bbox, rect)
        assert bbox[2] <= rect[2] and bbox[3] <= rect[3], (path.name, bbox, rect)
        # Clear the CPU cell; every other pixel must remain byte-identical.
        ImageDraw.Draw(difference).rectangle((rect[0], rect[1], rect[2]-1, rect[3]-1), fill=0)
        assert difference.getbbox() is None, path.name
    if match[3] != 'previous':
        # Isolate the glyph including background, then compare both mirror axes.
        glyph = b.crop(rect)
        glyph_bbox = ImageChops.difference(glyph, Image.new('RGB', glyph.size, glyph.getpixel((0,0)))).getbbox()
        assert glyph_bbox is not None, path.name
        glyph = glyph.crop(glyph_bbox)
        assert ImageChops.difference(glyph, glyph.transpose(Image.Transpose.FLIP_LEFT_RIGHT)).getbbox() is None, path.name
        assert ImageChops.difference(glyph, glyph.transpose(Image.Transpose.FLIP_TOP_BOTTOM)).getbbox() is None, path.name
    checked.append({'file': path.name, 'changed_bbox': bbox})

dpi = after['host_dpi']
font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 20)
normal = Image.open(NEW / f'light-{dpi}-normal.png')
zoom = Image.open(NEW / f'light-{dpi}-normal-4x.png')
width = max(980, zoom.width + 40)
height = 150 + zoom.height*2 + 70
canvas = Image.new('RGB', (width, height), '#e5e7eb')
draw = ImageDraw.Draw(canvas)
draw.text((20, 12), 'CPU 第三版：中央核心 + 四边各三根引脚，逐像素对称', font=font, fill='#111827')
draw.text((20, 43), f'当前 {dpi/96*100:g}% · 原尺寸 · 9pt · 浅色 / 深色', font=font, fill='#111827')
canvas.paste(normal, (20, 77))
canvas.paste(Image.open(NEW / f'dark-{dpi}-normal.png'), (normal.width+48, 77))
draw.text((20, 152), '浅色 · 4 倍像素放大', font=font, fill='#111827')
canvas.paste(zoom, (20, 181))
draw.text((20, 195+zoom.height), '深色 · 4 倍像素放大', font=font, fill='#111827')
canvas.paste(Image.open(NEW / f'dark-{dpi}-normal-4x.png'), (20, 224+zoom.height))
canvas.save(NEW / 'cpu-update.png')
# Distinct filename prevents the app from reusing the previous preview URL.
canvas.save(NEW / 'cpu-symmetric-v3.png')

# Show the actual prior render beside the new one, not a recreated "old" icon.
proof=Image.new('RGB',(760,570),'#e5e7eb')
proof_draw=ImageDraw.Draw(proof)
proof_draw.text((20,10),'上次实际图标                 本次：核心 + 12根对称引脚',font=font,fill='#111827')
rect=scales[dpi]['cpu_rect_px']
crop=(rect[0]-2,rect[1],rect[2]+2,rect[3])
for row,theme in enumerate(['light','dark']):
    y=48+row*258
    for column,folder in enumerate([ROOT/'work/cpu-icon-v2/preview',NEW]):
        icon=Image.open(folder/f'{theme}-{dpi}-normal.png').crop(crop)
        icon=icon.resize((icon.width*7,icon.height*7),Image.Resampling.NEAREST)
        proof.paste(icon,(20+column*360,y))
proof.save(NEW/'cpu-before-after-v3.png')
(ROOT / 'evidence/cpu-redraw-tests.json').write_text(json.dumps({
    'baseline': str(OLD), 'checked_images': len(checked),
    'layout_metrics_unchanged': True, 'only_cpu_pixels_changed': True,
    'horizontal_vertical_pixel_symmetry': True,
    'images': checked,
}, ensure_ascii=False, indent=2), encoding='utf-8')
print(f'{len(checked)} raster comparisons passed; layout/other pixels unchanged')
