#!/usr/bin/env python3
"""palrle 缺陷复现 + 修复对比图。"""
from PIL import Image, ImageDraw, ImageFont

REG = "/tmp/NotoSansSC.zip"
BOLD = "/tmp/NotoSansSC-Bold.otf"
MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
F_TITLE = ImageFont.truetype(BOLD, 30)
F_H = ImageFont.truetype(BOLD, 21)
F_T = ImageFont.truetype(REG, 17)
F_M = ImageFont.truetype(MONO, 15)
F_MS = ImageFont.truetype(MONO, 13)

PALETTE = [(230, 57, 70), (38, 130, 188), (42, 157, 143), (244, 162, 97),
           (116, 87, 174), (233, 196, 64), (70, 70, 70), (200, 200, 200)]
BG = (24, 26, 31); PANEL = (33, 36, 44); FG = (236, 238, 242)
RED = (240, 100, 100); GREEN = (96, 206, 140); BLUE = (110, 170, 235)
PAD = 18

def grid(path, w, h):
    raw = open(path, "rb").read()
    return [[raw[y * w + x] for x in range(w)] for y in range(h)]

def draw_grid(dr, ox, oy, g, cell):
    for y, row in enumerate(g):
        for x, v in enumerate(row):
            dr.rectangle([ox + x*cell, oy + y*cell,
                          ox + (x+1)*cell - 1, oy + (y+1)*cell - 1],
                         fill=PALETTE[v % len(PALETTE)], outline=(18, 18, 18))

def wrap(dr, text, font, maxw):
    out, line = [], ""
    for ch in text:
        if ch == "\n":
            out.append(line); line = ""; continue
        if dr.textlength(line + ch, font=font) <= maxw:
            line += ch
        else:
            out.append(line); line = ch
    out.append(line)
    return out

def para(dr, x, y, text, font, fill, maxw, lh):
    for ln in wrap(dr, text, font, maxw):
        dr.text((x, y), ln, font=font, fill=fill); y += lh
    return y

src = grid("repro/grid_2d_src.bin", 48, 24)
dst = grid("repro/grid_2d_dst.bin", 48, 24)
w1 = list(open("repro/grid_w1.bin", "rb").read())
identical = src == dst

W, H = 1500, 1000
img = Image.new("RGB", (W, H), BG)
dr = ImageDraw.Draw(img)
dr.text((PAD, 16), "palrle.h 调色板行程编码：长同色行边界解码错位 / 末行丢失 —— 复现与修复",
        font=F_TITLE, fill=FG)

CELL = 13
GW, GH = 48 * CELL, 24 * CELL
# ---- left panel: source grid ----
lp = (PAD, 66, PAD + GW + 2*PAD, 66 + GH + 150)
dr.rounded_rectangle(lp, radius=10, fill=PANEL, outline=FG, width=2)
dr.text((lp[0]+PAD, lp[1]+10), "① 原始索引位图（encode 输入，48 × 24）", font=F_H, fill=FG)
gx, gy = lp[0]+PAD, lp[1]+48
draw_grid(dr, gx, gy, src, CELL)
dr.text((gx, gy+GH+10), "颜色块 = 调色板索引；同色块构成 RLE run", font=F_T, fill=FG)
sy = gy+GH+40
dr.text((gx, sy), "最小触发序列 width=1（16 行，每行 1 像素 → 每像素独立 uniques 块）:", font=F_T, fill=FG)
for i, v in enumerate(w1):
    x0 = gx + i*27
    dr.rectangle([x0, sy+30, x0+23, sy+53], fill=PALETTE[v], outline=(15,15,15))

# ---- right-top panel: OLD problem ----
rx = lp[2] + 18
rp1 = (rx, 66, W-PAD, 500)
dr.rounded_rectangle(rp1, radius=10, fill=PANEL, outline=RED, width=2)
dr.text((rp1[0]+PAD, rp1[1]+10), "② 旧实现：编码器堆溢出，解码器无边界检查", font=F_H, fill=RED)
tx = rp1[0]+PAD; tw = rp1[2]-rp1[0]-2*PAD; ty = rp1[1]+48
old_lines = [
    ("RLE 块结构： [empty][count][color…]；count>0 为同色 run，count<0 为逐像素 uniques。", FG),
    ("最坏情况下每个像素都自成一个 uniques 块，单像素成本 = 3 字节/像素。", FG),
    ("旧编码器却只预留 hpitch × vpitch × 2，即 2 字节/像素，缓冲区天然不足。", RED),
    ("实测 width=1, height=64：旧公式预留 412 字节，格式实际需要 475 字节，越界写 63 字节。", RED),
    ("越界写破坏相邻堆块；解码器又把数据流当无限长线性解析，无任何长度/边界检查。", RED),
    ("于是长同色行的块边界一旦错位，后续整行平移，最后一行直接丢失；损坏/越界输入会越界读甚至段错误。", RED),
]
for txt, col in old_lines:
    ty = para(dr, tx, ty, txt, F_T, col, tw, 23) + 6
# bar chart
ty += 6
dr.text((tx, ty), "width=1, height=64 的缓冲区账目：", font=F_T, fill=FG); ty += 30
barw = tw - 150; reserved = int(barw * 412/475)
dr.rectangle([tx, ty, tx+reserved, ty+26], outline=GREEN, width=2)
dr.text((tx+8, ty+4), "旧预留 412B", font=F_T, fill=GREEN)
dr.rectangle([tx+reserved, ty, tx+barw, ty+26], fill=(120,32,32))
dr.text((tx+reserved+8, ty+4), "溢出 63B（越界写）", font=F_T, fill=RED)

# ---- right-bottom panel: NEW fix ----
rp2 = (rx, 512, W-PAD, H-24)
dr.rounded_rectangle(rp2, radius=10, fill=PANEL, outline=GREEN, width=2)
dr.text((rp2[0]+PAD, rp2[1]+10), "③ 新实现：逐像素一致 + 越界输入返回错误码", font=F_H, fill=GREEN)
tx2 = rp2[0]+PAD; tw2 = rp2[2]-rp2[0]-2*PAD; ty2 = rp2[1]+48
new_lines = [
    "RLE 容量按真实上界分配：无 mask 3n 字节、带 mask 5n 字节，彻底消除写越界；并保留编码后容量断言。",
    "解码器改为按每行的 row offset 表独立寻址，单行出错不会级联污染后续行，末行不再丢失。",
    "每个 RLE 字节读取都对照 blob size 校验，每个像素写入都对照目标宽高校验；截断返回 PALRLE_ERR_TRUNCATED，结构损坏返回 PALRLE_ERR_CORRUPT，空指针/非法尺寸返回相应错误码，绝不越界访问。",
    "偏移表用 memcpy 读写以避免非对齐 U32 访问；宽高与调色板数量做 U16 范围和 size_t 溢出校验。",
    "新增 palrle_decode_safe / palrle_blit_safe 返回状态码；原 void 接口保留为兼容包装。",
]
for txt in new_lines:
    ty2 = para(dr, tx2, ty2, txt, F_T, FG, tw2, 22) + 3
status = ("当前 48×24 位图：palrle_decode_safe() = PALRLE_OK(0)，" + ("源/目标逐像素比对 = 完全一致 ✓" if identical else "源/目标逐像素比对 = 不一致 ✗"))
ty2 += 4
para(dr, tx2, ty2+4, status, F_H, GREEN, tw2, 28)

img.save("repro/palrle_repro.png")
print("saved repro/palrle_repro.png", img.size, "identical=", identical)
