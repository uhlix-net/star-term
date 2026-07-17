"""A small programmatically-drawn "key" icon (no bundled image assets needed)."""

from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QColor, QIcon, QPainter, QPixmap

ACCENT_GOLD = QColor(230, 185, 70)


def _pixmap(size: int) -> QPixmap:
    pm = QPixmap(size, size)
    pm.fill(Qt.transparent)
    return pm


def _painter(pm: QPixmap) -> QPainter:
    p = QPainter(pm)
    p.setRenderHint(QPainter.Antialiasing)
    return p


def key_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(ACCENT_GOLD)

    # Head of the key: a ring (circle with a punched-out hole).
    head = size * 0.46
    p.drawEllipse(QRectF(size * 0.04, size * 0.04, head, head))
    p.setCompositionMode(QPainter.CompositionMode_Clear)
    hole = head * 0.45
    offset = (head - hole) / 2
    p.drawEllipse(QRectF(size * 0.04 + offset, size * 0.04 + offset, hole, hole))
    p.setCompositionMode(QPainter.CompositionMode_SourceOver)

    # Shaft and teeth, angled down-right from the head.
    p.translate(size * 0.5, size * 0.5)
    p.rotate(45)
    shaft_w = size * 0.16
    shaft_len = size * 0.5
    p.drawRect(QRectF(0, -shaft_w / 2, shaft_len, shaft_w))
    tooth = size * 0.14
    p.drawRect(QRectF(shaft_len - tooth, shaft_w / 2, tooth * 0.6, tooth))
    p.drawRect(QRectF(shaft_len - tooth * 2.2, shaft_w / 2, tooth * 0.6, tooth * 0.7))

    p.end()
    return QIcon(pm)


def app_icon() -> QIcon:
    """Multi-resolution application icon (matches installer/app.ico) for
    the window/taskbar icon."""
    icon = QIcon()
    for size in (16, 24, 32, 48, 64, 128, 256):
        icon.addPixmap(key_icon(size).pixmap(size, size))
    return icon
