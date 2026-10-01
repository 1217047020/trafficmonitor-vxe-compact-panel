"""Arrange production-GDI PNGs into review sheets and a local HTML index."""
from pathlib import Path
import html
import json
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'evidence/compact-preview'
metrics = json.loads((OUT / 'metrics.json').read_text(encoding='utf-8'))
dpi = metrics['host_dpi']
font_path = 'C:/Windows/Fonts/msyh.ttc'
title_font = ImageFont.truetype(font_path, 22)
small_font = ImageFont.truetype(font_path, 16)

def sheet(name, rows):
    width = 1060
    height = 24 + sum(50 + max(Image.open(OUT / f).height for f in files) + 24
                      for label, files in rows)
    canvas = Image.new('RGB', (width, height), '#e5e7eb')
    draw = ImageDraw.Draw(canvas)
    y = 18
    for label, files in rows:
        draw.text((20, y), label, fill='#111827', font=title_font)
        y += 46
        x = 20
        tallest = 0
        for file in files:
            with Image.open(OUT / file) as image:
                canvas.paste(image, (x, y))
                tallest = max(tallest, image.height)
                x += image.width + 28
        y += tallest + 28
    canvas.save(OUT / name)

sheet('comparison.png', [
    (f'当前环境 {round(dpi / 96 * 100)}% · 同一 9pt 字体 · 旧布局 / 新面板（原尺寸）',
     [f'light-{dpi}-previous.png', f'light-{dpi}-normal.png']),
    ('深色 · 旧布局 / 新面板（原尺寸，数据为模拟样本）',
     [f'dark-{dpi}-previous.png', f'dark-{dpi}-normal.png']),
    ('浅色新面板 · 4 倍像素放大（不是缩放字号）', [f'light-{dpi}-normal-4x.png']),
    ('深色新面板 · 4 倍像素放大', [f'dark-{dpi}-normal-4x.png']),
])
sheet('states.png', [
    ('零网速 / CPU、内存、电量 0%（红色是真实零电量）', [f'light-{dpi}-zero-4x.png']),
    ('1023B / CPU、内存、电量 100%', [f'light-{dpi}-100-4x.png']),
    ('低电量 20% · 橙色', [f'light-{dpi}-low20-4x.png']),
    ('低电量 10% · 红色', [f'light-{dpi}-low10-4x.png']),
    ('离线 -- · 鼠标关闭 / 超时', [f'light-{dpi}-offline-4x.png']),
    ('过期 ? · 旧 85% 不再绘制', [f'light-{dpi}-expired-4x.png']),
    ('不可用 ? · 设备访问失败', [f'light-{dpi}-unavailable-4x.png']),
    ('极大网速（ULL_MAX）· 缩略科学计数，悬停显示精确 B/s', [f'light-{dpi}-huge-4x.png']),
])
parts = ['<!doctype html><meta charset="utf-8"><title>TrafficMonitor 极简面板预览</title>',
         '<style>body{font-family:"Microsoft YaHei",sans-serif;margin:24px;background:#e5e7eb;color:#111827}'
         'img{image-rendering:pixelated;max-width:100%}.row{margin:20px 0}table{border-collapse:collapse}td,th{padding:8px;text-align:left}</style>',
         '<h1>TrafficMonitor 极简数字面板 · 待视觉确认</h1>',
         '<p>真实生产 GDI 绘制代码；数据为模拟样本。未替换安装 DLL、未改配置、未进行 HID 查询。</p>',
         '<p>面板无背景、边框或分隔线；预览背景用于模拟浅色/深色宿主。</p>',
         '<img src="comparison.png"><h2>状态边界</h2><img src="states.png"><h2>DPI 矩阵</h2>']
for scale in metrics['scales']:
    d = scale['dpi']
    parts.append(f'<h3>{round(d / 96 * 100)}% · {scale["panel_width_px"]}×{scale["panel_height_px"]} px · 9pt</h3>')
    for theme in ['light', 'dark']:
        parts.append('<div class="row">')
        for scenario in ['normal', '100', 'low20', 'low10', 'offline', 'expired', 'K-M', 'M-G', 'huge']:
            file = f'{theme}-{d}-{scenario}.png'
            parts.append(f'<figure style="display:inline-block;margin:6px"><figcaption>{html.escape(scenario)}</figcaption><img src="{file}"></figure>')
        parts.append('</div>')
(OUT / 'index.html').write_text('\n'.join(parts), encoding='utf-8')
print(f'Review sheets and HTML: {OUT}')
