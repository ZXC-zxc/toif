from mako.template import Template
import sys
import os
from PIL import Image
from typing import Sequence, Tuple
import struct


def _from_pil_grayscale(pixels: Sequence[int]) -> bytes:
    data = bytearray()
    for i in range(0, len(pixels), 2):
        left, right = pixels[i], pixels[i + 1]
        c = (left & 0xF0) | ((right & 0xF0) >> 4)
        data += struct.pack(">B", c)
    return bytes(data)

RGBPixel = Tuple[int, int, int]
def gci(filepath):
    files_list = []
    files = os.listdir(filepath)
    for fi in files:
        fi_d = os.path.join(filepath, fi)
        if os.path.isdir(fi_d):
            files_list.append(gci(fi_d))
        else:
            files_list.append(os.path.join(filepath, fi_d))
    return files_list


def _from_pil_rgb(pixels: Sequence[RGBPixel]) -> bytes:
    data = bytearray()
    for r, g, b in pixels:
        c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3)
        data += struct.pack(">H", c)
    return bytes(data)


files_list = gci(sys.path[0]+'\\home')
with open(sys.path[0] + '\\home.h', "w") as dst:
    images = {}
    sizes = {}
    for file in files_list:
        background = (0, 0, 0, 255)
        image = Image.open(file)
        image.save(sys.path[0]+'\\home\\home1.jpg')
        if image.mode == "RGBA":
            background = Image.new("RGBA", image.size, background)
            blend = Image.alpha_composite(background, image)
            image = blend.convert("RGB")

        if image.mode == "L":
            toif_data = _from_pil_grayscale(image.getdata())
        elif image.mode == "RGB":
            toif_data = _from_pil_rgb(image.getdata())
        else:
            raise ValueError("Unsupported image mode: {}".format(image.mode))


        images[file.split('\\')[-1].split('.')[0]] = toif_data
        sizes[file.split('\\')[-1].split('.')[0]] = image.size

    mytemplate = Template(filename=sys.path[0] + '\\rgb565.h.mako')
    result = mytemplate.render(images=images,sizes=sizes)
    dst.write(result)
