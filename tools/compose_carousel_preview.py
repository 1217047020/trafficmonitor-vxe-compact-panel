"""Arrange existing production-GDI renders; no device access or icon generation."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    root = args.directory
    metrics = json.loads((root / 'metrics.json').read_text(encoding='utf-8'))
    dpi = metrics['host_dpi']
    scale = next(s for s in metrics['scales'] if s['dpi'] == dpi)
    title = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 21)
    small = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 17)
    canvas = Image.new('RGB', (1000, 410), '#e5e7eb')
    draw = ImageDraw.Draw(canvas)
    draw.text((20, 16), '电量列轮播：鼠标 → 手柄 → 耳仓（每5秒）', fill='#111827', font=title)
    draw.text((20, 49), f'9pt · 面板 {scale["panel_width_px"]}×{scale["panel_height_px"]} px；数值均为模拟布局样本，不代表当前读数。',
              fill='#374151', font=small)
    for row, theme in enumerate(('light', 'dark')):
        y = 105 + row * 137
        for col, (label, device) in enumerate(zip(('鼠标', '手柄', '耳机仓（模拟）'), ('mouse', 'controller', 'case-mock'))):
            x = 20 + col * 320
            draw.text((x, y), label, fill='#111827', font=small)
            with Image.open(root / f'{theme}-{dpi}-carousel-{device}.png') as rendered:
                canvas.paste(rendered.convert('RGB'), (x, y + 30))
    canvas.save(root / 'carousel-review.png')


if __name__ == '__main__':
    main()
