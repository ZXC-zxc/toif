from mako.template import Template
import sys
import os
from PIL import Image
from trezorlib import toif


def gci(filepath):
    files_list = []
    files = os.listdir(filepath)  
    for fi in files:    
        fi_d = os.path.join(filepath,fi)    
        if os.path.isdir(fi_d):
            files_list.append(gci(fi_d))    
        else:      
            files_list.append(os.path.join(filepath, fi_d))
    return files_list


files_list = gci(sys.path[0]+'\\images')
with open(sys.path[0] + '\\images.h', "w") as dst:
    images = {}
    for file in files_list:
        im = Image.open(file)
        toi = toif.from_image(im)
        images[file.split('\\')[-1].split('.')[0]] = toi.to_bytes()


    mytemplate = Template(filename=sys.path[0] + '\\images.h.mako')
    result = mytemplate.render(images=images)
    dst.write(result)
