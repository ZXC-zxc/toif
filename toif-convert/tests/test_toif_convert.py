import sys
import os
sys.path.append(os.path.abspath("."))
print(sys.path)
from toif_convert import __version__
from toif_convert import convert


def test_version():
    assert __version__ == '0.1.0'


if __name__ == "__main__":
    convert.png_to_toif(os.path.abspath("tests/book.png"),os.path.abspath("tests/book.toif"))
    convert.toif_to_png(os.path.abspath("tests/book.toif"),os.path.abspath("tests/book2.png"))
    convert.png_to_toif(os.path.abspath("tests/shi.png"),os.path.abspath("tests/shi.toif"))
    convert.png_to_toif(os.path.abspath("tests/yong.png"),
                        os.path.abspath("tests/yong.toif"))
