#!/usr/bin/env python3
"""Arte de boot 128x64 (1 bit) para o Ender-3 Neo. Gera PNG de previa e _Bootscreen.h."""
import sys
from PIL import Image, ImageDraw, ImageFont

W, H = 128, 64
B = "/usr/share/fonts/truetype/dejavu/"

def font(name, size):
    f = ImageFont.truetype(B + name, size)
    return f

def art():
    img = Image.new("1", (W, H), 0)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"   # sem antialias

    # Dragao (convertido da imagem enviada pelo Luiz, 62x62)
    import os
    dr = Image.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "dragao.png")).convert("1")
    dr.putpixel((6, 31), 0)                        # olho
    img.paste(dr, (0, (H - dr.height) // 2))

    # Texto
    f1 = font("DejaVuSans-Bold.ttf", 12)
    tw = d.textlength("ENDER-3", font=f1)
    d.text((96 - tw / 2, 4), "ENDER-3", font=f1, fill=1)
    # caixa "NEO" invertida, cantos arredondados
    d.rounded_rectangle([66, 22, 126, 45], radius=6, fill=1)
    f2 = font("DejaVuSans-Bold.ttf", 20)
    tw = d.textlength("NEO", font=f2)
    d.text((96 - tw / 2, 21), "NEO", font=f2, fill=0)
    f3 = font("DejaVuSansMono-Bold.ttf", 9)
    tag = "EDIÇÃO ROJAS"
    tw = d.textlength(tag, font=f3)
    d.text((95 - tw / 2, 50), tag, font=f3, fill=1)
    return img

def to_header(img):
    px = img.load()
    out = []
    for y in range(H):
        for xb in range(W // 8):
            v = 0
            for b in range(8):
                v = (v << 1) | (1 if px[xb * 8 + b, y] else 0)
            out.append(v)
    rows = []
    for y in range(H):
        rows.append("  " + ", ".join("0x%02X" % v for v in out[y * 16:(y + 1) * 16]) + ",")
    return ("/**\n * Arte de boot 128x64 do Ender-3 Neo (gerada por tools/bootart.py)\n */\n"
            "#pragma once\n\n#define CUSTOM_BOOTSCREEN_BMPWIDTH  128\n\n"
            "const unsigned char custom_start_bmp[] PROGMEM = {\n" + "\n".join(rows) + "\n};\n")

if __name__ == "__main__":
    img = art()
    open(sys.argv[1], "w").write(to_header(img))
    s = 6
    rgb = Image.new("RGB", (W, H)); bg, fg = (40, 60, 210), (225, 235, 255)
    rgb.putdata([fg if v else bg for v in img.getdata()])
    rgb.resize((W * s, H * s), Image.NEAREST).save(sys.argv[2])
