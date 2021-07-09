import construct as c
import sys
from PIL import ImageFont, ImageDraw, Image
from trezorlib import toif
from mako.template import Template

tlv = c.Struct(
    "type" / c.Bytes(3),
    "data" / c.Prefixed(c.Int16ul, c.GreedyBytes),
)

atlv = c.Struct(
    "type" / c.Bytes(1),
    "data" / c.Prefixed(c.Int16ul, c.GreedyBytes),
)

#按需添加更多的文字
character = "使用教程"
fonts = []
for ch in character:
    font = ImageFont.truetype(sys.path[0] +"\\xiangsu.ttf", 12)
    image = Image.new('RGB', font.getsize(ch), (0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), ch, fill='white', font=font)
    image = image.convert("L")
    toi = toif.from_image(image)
    fonts.append(tlv.build(dict(type=ch.encode(
        encoding='UTF-8', errors='strict'), data=toi.to_bytes()[4:])))


#处理英文
ansi = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
afonts = []
for a in ansi:
    font = ImageFont.truetype(sys.path[0] + "\\xiangsu.ttf", 12)
    image = Image.new('RGB', (6,11), (0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), a, fill='white', font=font)
    image = image.convert("L")
    toi = toif.from_image(image)
    afonts.append(atlv.build(dict(type=a.encode(
        encoding='UTF-8', errors='strict'), data=toi.to_bytes()[4:])))

with open(sys.path[0] + '\\fonts.h', "w") as dst:
    mytemplate = Template(filename=sys.path[0] + '\\fonts.h.mako')
    result = mytemplate.render(fonts=fonts, ansi=afonts)
    dst.write(result)
