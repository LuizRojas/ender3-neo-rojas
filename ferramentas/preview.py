#!/usr/bin/env python3
"""Previa da grade de icones como ficaria no display 128x64 (aproximada)."""
import sys
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, sys.path[0]); from icons import ICONS, parse

def circle(px, cx, cy, r, fill):
    # midpoint como o u8g_DrawCircle / DrawDisc
    for y in range(-r, r + 1):
        for x in range(-r, r + 1):
            d = x * x + y * y
            if fill and d <= r * r + r: px.add((cx + x, cy + y))
            if not fill and r * r - r <= d <= r * r + r: px.add((cx + x, cy + y))

def render(items, sel, out, scale=6):
    img = Image.new("1", (128, 64), 0); dr = ImageDraw.Draw(img)
    on = set()
    for i, (icon, label) in enumerate(items):
        col, row = i % 4, i // 4
        cx, cy = 16 + 32 * col, 12 + 26 * row
        ring = set(); circle(ring, cx, cy, 12, i == sel)
        on |= ring
        rows = parse(ICONS[icon])
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                if c == "#":
                    p = (cx - 8 + x, cy - 8 + y)
                    if i == sel: on.discard(p)
                    else: on.add(p)
    for p in on:
        if 0 <= p[0] < 128 and 0 <= p[1] < 64: img.putpixel(p, 1)
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 9)
    label = items[sel][1]
    w = dr.textlength(label, font=font)
    dr.text(((128 - w) / 2, 53), label, fill=1, font=font)
    rgb = Image.new("RGB", (128, 64))
    bg, fg = (40, 60, 210), (225, 235, 255)
    rgb.putdata([fg if v else bg for v in img.getdata()])
    rgb = rgb.resize((128 * scale, 64 * scale), Image.NEAREST)
    return rgb

if __name__ == "__main__":
    idle = [("back","Voltar"),("print","Imprimir do cartão"),("heat","Temperatura"),("move","Movimento"),
            ("level","Nivelamento"),("filament","Trocar Filamento"),("settings","Configuração"),("more","Mais opções")]
    printing = [("back","Voltar"),("tune","Ajustar"),("pause","Pausar impressão"),("stop","Parar impressão"),
                ("zoffset","Ajuste fino do Z"),("filament","Trocar Filamento"),("heat","Temperatura"),("more","Mais opções")]
    a = render(idle, 1, None); b = render(printing, 2, None)
    pad = 24
    sheet = Image.new("RGB", (a.width, a.height * 2 + pad), (255, 255, 255))
    sheet.paste(a, (0, 0)); sheet.paste(b, (0, a.height + pad))
    sheet.save(sys.argv[1])
