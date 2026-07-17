"""Small programmatically-drawn icons (no bundled image assets needed)."""

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QFont, QIcon, QPainter, QPen, QPixmap, QPolygonF

ACCENT_GREEN = QColor(39, 201, 63)
ACCENT_RED = QColor(232, 71, 71)
ACCENT_BLUE = QColor(86, 156, 214)
DARK_BG = QColor(30, 30, 33)
LIGHT = QColor(235, 235, 235)
GRAY = QColor(150, 150, 150)
FOLDER_COLOR = QColor(240, 185, 90)
FOLDER_DARK = QColor(214, 160, 70)

# Neutral mid-tones for icon glyphs that sit directly on the toolbar/panel
# background (rather than on a colored badge), so they stay visible in both
# the Light and Dark themes.
ICON_FG = QColor(140, 140, 140)
ICON_FG_MUTED = QColor(95, 95, 95)


def _pixmap(size: int) -> QPixmap:
    pm = QPixmap(size, size)
    pm.fill(Qt.transparent)
    return pm


def _painter(pm: QPixmap) -> QPainter:
    p = QPainter(pm)
    p.setRenderHint(QPainter.Antialiasing)
    return p


def connect_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(ACCENT_GREEN)
    p.drawEllipse(QRectF(1, 1, size - 2, size - 2))
    p.setBrush(LIGHT)
    p.drawPolygon(QPolygonF([
        QPointF(size * 0.36, size * 0.27),
        QPointF(size * 0.36, size * 0.73),
        QPointF(size * 0.76, size * 0.5),
    ]))
    p.end()
    return QIcon(pm)


def disconnect_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(ACCENT_RED)
    p.drawEllipse(QRectF(1, 1, size - 2, size - 2))
    p.setBrush(LIGHT)
    s = size * 0.26
    p.drawRect(QRectF(size / 2 - s / 2, size / 2 - s / 2, s, s))
    p.end()
    return QIcon(pm)


def multi_exec_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(ACCENT_BLUE)
    gap = size * 0.1
    cell = (size - 3 * gap) / 2
    for row in range(2):
        for col in range(2):
            x = gap + col * (cell + gap)
            y = gap + row * (cell + gap)
            p.drawRoundedRect(QRectF(x, y, cell, cell), 2, 2)
    p.end()
    return QIcon(pm)


def terminal_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(DARK_BG)
    p.drawRoundedRect(QRectF(1, 1, size - 2, size - 2), 3, 3)

    pen = QPen(ACCENT_GREEN)
    pen.setWidthF(size * 0.1)
    pen.setCapStyle(Qt.RoundCap)
    pen.setJoinStyle(Qt.RoundJoin)
    p.setPen(pen)
    m = size * 0.22
    mid_y = size * 0.5
    p.drawLine(QPointF(m, size * 0.32), QPointF(m + size * 0.16, mid_y))
    p.drawLine(QPointF(m + size * 0.16, mid_y), QPointF(m, size * 0.68))

    pen.setColor(LIGHT)
    p.setPen(pen)
    p.drawLine(QPointF(size * 0.46, size * 0.68), QPointF(size * 0.78, size * 0.68))
    p.end()
    return QIcon(pm)


def app_icon() -> QIcon:
    """Multi-resolution application icon (matches installer/app.ico) for
    the window/taskbar icon, so Windows doesn't fall back to a default
    icon when it requests a size other than the single one used elsewhere."""
    icon = QIcon()
    for size in (16, 24, 32, 48, 64, 128, 256):
        icon.addPixmap(terminal_icon(size).pixmap(size, size))
    return icon


def folder_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(FOLDER_DARK)
    p.drawRoundedRect(QRectF(1, size * 0.32, size - 2, size * 0.6), 2, 2)
    p.setBrush(FOLDER_COLOR)
    p.drawRoundedRect(QRectF(1, size * 0.22, size * 0.45, size * 0.16), 2, 2)
    p.drawRoundedRect(QRectF(1, size * 0.34, size - 2, size * 0.56), 2, 2)
    p.end()
    return QIcon(pm)


def directory_icon(size: int = 24) -> QIcon:
    return folder_icon(size)


def file_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(ICON_FG)
    fold = size * 0.28
    poly = QPolygonF([
        QPointF(size * 0.22, size * 0.06),
        QPointF(size * 0.78 - fold, size * 0.06),
        QPointF(size * 0.78, size * 0.06 + fold),
        QPointF(size * 0.78, size * 0.94),
        QPointF(size * 0.22, size * 0.94),
    ])
    p.drawPolygon(poly)
    p.setBrush(ICON_FG_MUTED)
    p.drawPolygon(QPolygonF([
        QPointF(size * 0.78 - fold, size * 0.06),
        QPointF(size * 0.78, size * 0.06 + fold),
        QPointF(size * 0.78 - fold, size * 0.06 + fold),
    ]))
    pen = QPen(ICON_FG_MUTED)
    pen.setWidthF(size * 0.06)
    p.setPen(pen)
    for i in range(3):
        y = size * (0.45 + i * 0.14)
        p.drawLine(QPointF(size * 0.32, y), QPointF(size * 0.68, y))
    p.end()
    return QIcon(pm)


def up_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(ICON_FG)
    p.drawPolygon(QPolygonF([
        QPointF(size * 0.5, size * 0.18),
        QPointF(size * 0.82, size * 0.55),
        QPointF(size * 0.62, size * 0.55),
        QPointF(size * 0.62, size * 0.85),
        QPointF(size * 0.38, size * 0.85),
        QPointF(size * 0.38, size * 0.55),
        QPointF(size * 0.18, size * 0.55),
    ]))
    p.end()
    return QIcon(pm)


def refresh_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    pen = p.pen()
    pen.setColor(ICON_FG)
    pen.setWidthF(size * 0.12)
    pen.setCapStyle(Qt.RoundCap)
    p.setPen(pen)
    p.setBrush(Qt.NoBrush)
    rect = QRectF(size * 0.18, size * 0.18, size * 0.64, size * 0.64)
    p.drawArc(rect, 30 * 16, 280 * 16)
    p.setBrush(ICON_FG)
    p.setPen(Qt.NoPen)
    p.drawPolygon(QPolygonF([
        QPointF(size * 0.82, size * 0.18),
        QPointF(size * 0.82, size * 0.4),
        QPointF(size * 0.6, size * 0.3),
    ]))
    p.end()
    return QIcon(pm)


def sessions_icon(size: int = 24) -> QIcon:
    return terminal_icon(size)


def macros_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(ICON_FG)
    p.drawPolygon(QPolygonF([
        QPointF(size * 0.58, size * 0.04),
        QPointF(size * 0.2, size * 0.56),
        QPointF(size * 0.46, size * 0.56),
        QPointF(size * 0.42, size * 0.96),
        QPointF(size * 0.8, size * 0.44),
        QPointF(size * 0.54, size * 0.44),
    ]))
    p.end()
    return QIcon(pm)


def settings_icon(size: int = 24) -> QIcon:
    pm = _pixmap(size)
    p = _painter(pm)
    p.translate(size / 2, size / 2)

    p.setPen(Qt.NoPen)
    p.setBrush(ICON_FG)
    radius = size * 0.34
    tooth_w = size * 0.16
    tooth_h = size * 0.16
    for i in range(8):
        p.save()
        p.rotate(i * 45)
        p.drawRoundedRect(QRectF(-tooth_w / 2, -(radius + tooth_h * 0.55), tooth_w, tooth_h), 1.5, 1.5)
        p.restore()
    p.drawEllipse(QRectF(-radius, -radius, radius * 2, radius * 2))

    p.setCompositionMode(QPainter.CompositionMode_Clear)
    hole = radius * 0.5
    p.drawEllipse(QRectF(-hole, -hole, hole * 2, hole * 2))

    p.end()
    return QIcon(pm)


def down_arrow_pixmap(color: QColor, size: int = 10) -> QPixmap:
    """A small downward-pointing triangle, used as the QComboBox drop-down
    indicator so combo boxes are clearly recognizable as such."""
    pm = _pixmap(size)
    p = _painter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(color)
    margin = size * 0.2
    p.drawPolygon(QPolygonF([
        QPointF(margin, size * 0.35),
        QPointF(size - margin, size * 0.35),
        QPointF(size / 2, size * 0.7),
    ]))
    p.end()
    return pm
