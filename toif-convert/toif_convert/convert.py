from PIL import Image
from trezorlib import toif

def png_to_toif(infile,outfile):
    with open(outfile, 'wb') as png:
        im = Image.open(infile)
        toi = toif.from_image(im)
        png.write(toi.to_bytes())
        
def toif_to_png(infile,outfile):
    with open(infile, 'rb') as t:
        toi = toif.from_bytes(t.read())
        im = toi.to_image()
        im.save(outfile)


