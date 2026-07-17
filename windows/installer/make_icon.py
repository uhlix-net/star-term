"""Generates installer/app.ico from the same icon used for the "Sessions"
activity-bar button and the main application window (icons.terminal_icon),
so the Start Menu/Desktop shortcuts and taskbar icon all match."""

import os
import sys

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from PIL import Image
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QApplication

from star_term import icons

SIZES = (16, 24, 32, 48, 64, 128, 256)


def main():
    app = QApplication([])

    images = []
    for size in SIZES:
        pm = icons.terminal_icon(size).pixmap(size, size)
        qimg = pm.toImage().convertToFormat(QImage.Format_RGBA8888)
        buf = bytes(qimg.constBits())
        images.append(Image.frombuffer("RGBA", (size, size), buf, "raw", "RGBA", 0, 1).copy())

    # PIL's ICO writer only includes sizes <= the base image's size, so use
    # the largest image as the base and the rest as append_images.
    images[-1].save("app.ico", format="ICO", sizes=[(s, s) for s in SIZES], append_images=images[:-1])
    print(f"wrote app.ico ({len(SIZES)} sizes)")


if __name__ == "__main__":
    main()
