#include "icons.h"

#include <QApplication>
#include <QPainter>
#include <QPolygonF>
#include <QPointF>
#include <QRectF>
#include <QPen>
#include <cmath>

using namespace Icons;

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------
static QPixmap makePixmap(int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    return pm;
}

// -----------------------------------------------------------------------
// connect_icon  — green circle with white play triangle
// -----------------------------------------------------------------------
QIcon Icons::connectIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(ACCENT_GREEN);
    p.drawEllipse(QRectF(1, 1, size - 2, size - 2));
    p.setBrush(LIGHT);
    p.drawPolygon(QPolygonF({
        QPointF(size * 0.36, size * 0.27),
        QPointF(size * 0.36, size * 0.73),
        QPointF(size * 0.76, size * 0.5),
    }));
    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// disconnect_icon  — red circle with white square
// -----------------------------------------------------------------------
QIcon Icons::disconnectIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(ACCENT_RED);
    p.drawEllipse(QRectF(1, 1, size - 2, size - 2));
    p.setBrush(LIGHT);
    double s = size * 0.26;
    p.drawRect(QRectF(size / 2.0 - s / 2.0, size / 2.0 - s / 2.0, s, s));
    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// multi_exec_icon  — 2×2 grid of blue rounded squares
// -----------------------------------------------------------------------
QIcon Icons::multiExecIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(ACCENT_BLUE);
    double gap  = size * 0.1;
    double cell = (size - 3.0 * gap) / 2.0;
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 2; ++col) {
            double x = gap + col * (cell + gap);
            double y = gap + row * (cell + gap);
            p.drawRoundedRect(QRectF(x, y, cell, cell), 2, 2);
        }
    }
    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// terminal_icon  — dark rounded rect, green ">" prompt, white underline
// -----------------------------------------------------------------------
QIcon Icons::terminalIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(DARK_BG);
    p.drawRoundedRect(QRectF(1, 1, size - 2, size - 2), 3, 3);

    QPen pen(ACCENT_GREEN);
    pen.setWidthF(size * 0.1);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    double m    = size * 0.22;
    double midY = size * 0.5;
    p.drawLine(QPointF(m, size * 0.32), QPointF(m + size * 0.16, midY));
    p.drawLine(QPointF(m + size * 0.16, midY), QPointF(m, size * 0.68));

    pen.setColor(LIGHT);
    p.setPen(pen);
    p.drawLine(QPointF(size * 0.46, size * 0.68), QPointF(size * 0.78, size * 0.68));
    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// app_icon  — multi-resolution (matches installer/app.ico)
// -----------------------------------------------------------------------
QIcon Icons::appIcon() {
    QIcon icon;
    for (int sz : {16, 24, 32, 48, 64, 128, 256}) {
        icon.addPixmap(terminalIcon(sz).pixmap(sz, sz));
    }
    return icon;
}

// -----------------------------------------------------------------------
// folder_icon
// -----------------------------------------------------------------------
QIcon Icons::folderIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(FOLDER_DARK);
    p.drawRoundedRect(QRectF(1, size * 0.32, size - 2, size * 0.6), 2, 2);
    p.setBrush(FOLDER_COLOR);
    p.drawRoundedRect(QRectF(1, size * 0.22, size * 0.45, size * 0.16), 2, 2);
    p.drawRoundedRect(QRectF(1, size * 0.34, size - 2, size * 0.56), 2, 2);
    p.end();
    return QIcon(pm);
}

QIcon Icons::directoryIcon(int size) {
    return folderIcon(size);
}

// -----------------------------------------------------------------------
// file_icon
// -----------------------------------------------------------------------
QIcon Icons::fileIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    double fold = size * 0.28;
    p.drawPolygon(QPolygonF({
        QPointF(size * 0.22, size * 0.06),
        QPointF(size * 0.78 - fold, size * 0.06),
        QPointF(size * 0.78, size * 0.06 + fold),
        QPointF(size * 0.78, size * 0.94),
        QPointF(size * 0.22, size * 0.94),
    }));
    p.setBrush(ICON_FG_MUTED);
    p.drawPolygon(QPolygonF({
        QPointF(size * 0.78 - fold, size * 0.06),
        QPointF(size * 0.78, size * 0.06 + fold),
        QPointF(size * 0.78 - fold, size * 0.06 + fold),
    }));
    QPen pen(ICON_FG_MUTED);
    pen.setWidthF(size * 0.06);
    p.setPen(pen);
    for (int i = 0; i < 3; ++i) {
        double y = size * (0.45 + i * 0.14);
        p.drawLine(QPointF(size * 0.32, y), QPointF(size * 0.68, y));
    }
    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// up_icon
// -----------------------------------------------------------------------
QIcon Icons::upIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    p.drawPolygon(QPolygonF({
        QPointF(size * 0.5,  size * 0.18),
        QPointF(size * 0.82, size * 0.55),
        QPointF(size * 0.62, size * 0.55),
        QPointF(size * 0.62, size * 0.85),
        QPointF(size * 0.38, size * 0.85),
        QPointF(size * 0.38, size * 0.55),
        QPointF(size * 0.18, size * 0.55),
    }));
    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// refresh_icon
// -----------------------------------------------------------------------
QIcon Icons::refreshIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen;
    pen.setColor(ICON_FG);
    pen.setWidthF(size * 0.12);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QRectF rect(size * 0.18, size * 0.18, size * 0.64, size * 0.64);
    p.drawArc(rect, 30 * 16, 280 * 16);
    p.setBrush(ICON_FG);
    p.setPen(Qt::NoPen);
    p.drawPolygon(QPolygonF({
        QPointF(size * 0.82, size * 0.18),
        QPointF(size * 0.82, size * 0.4),
        QPointF(size * 0.6,  size * 0.3),
    }));
    p.end();
    return QIcon(pm);
}

QIcon Icons::sessionsIcon(int size) {
    return terminalIcon(size);
}

// -----------------------------------------------------------------------
// macros_icon  — lightning bolt
// -----------------------------------------------------------------------
QIcon Icons::macrosIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    p.drawPolygon(QPolygonF({
        QPointF(size * 0.58, size * 0.04),
        QPointF(size * 0.2,  size * 0.56),
        QPointF(size * 0.46, size * 0.56),
        QPointF(size * 0.42, size * 0.96),
        QPointF(size * 0.8,  size * 0.44),
        QPointF(size * 0.54, size * 0.44),
    }));
    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// settings_icon  — gear
// -----------------------------------------------------------------------
QIcon Icons::settingsIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(size / 2.0, size / 2.0);

    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    double radius  = size * 0.34;
    double toothW  = size * 0.16;
    double toothH  = size * 0.16;
    for (int i = 0; i < 8; ++i) {
        p.save();
        p.rotate(i * 45.0);
        p.drawRoundedRect(
            QRectF(-toothW / 2.0, -(radius + toothH * 0.55), toothW, toothH),
            1.5, 1.5
        );
        p.restore();
    }
    p.drawEllipse(QRectF(-radius, -radius, radius * 2, radius * 2));

    p.setCompositionMode(QPainter::CompositionMode_Clear);
    double hole = radius * 0.5;
    p.drawEllipse(QRectF(-hole, -hole, hole * 2, hole * 2));

    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// sshIcon  — green terminal (same shape as terminalIcon)
// -----------------------------------------------------------------------
QIcon Icons::sshIcon(int size) {
    return terminalIcon(size);
}

// -----------------------------------------------------------------------
// rdpIcon  — blue monitor with two mini windows in the screen area
// -----------------------------------------------------------------------
QIcon Icons::rdpIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    // Monitor bezel
    p.setBrush(ACCENT_BLUE);
    p.drawRoundedRect(QRectF(1, size * 0.06, size - 2, size * 0.66), 2, 2);

    // Screen area (dark fill)
    p.setBrush(DARK_BG);
    p.drawRect(QRectF(size * 0.08, size * 0.13, size * 0.84, size * 0.52));

    // Stand
    p.setBrush(ACCENT_BLUE);
    p.drawRect(QRectF(size * 0.42, size * 0.72, size * 0.16, size * 0.14));
    p.drawRoundedRect(QRectF(size * 0.22, size * 0.84, size * 0.56, size * 0.06), 2, 2);

    // Two small windows on the screen (remote-desktop hint)
    p.setBrush(ICON_FG_MUTED);
    p.drawRoundedRect(QRectF(size * 0.13, size * 0.19, size * 0.31, size * 0.2),  1, 1);
    p.drawRoundedRect(QRectF(size * 0.52, size * 0.26, size * 0.31, size * 0.2),  1, 1);

    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// sidebarToggleIcon  — panel outline with a filled left column
// -----------------------------------------------------------------------
QIcon Icons::sidebarToggleIcon(int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF outline(size * 0.1, size * 0.16, size * 0.8, size * 0.68);
    QPen pen(ICON_FG);
    pen.setWidthF(size * 0.08);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(outline, 2, 2);

    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    double colW = outline.width() * 0.36;
    p.drawRect(QRectF(outline.left(), outline.top(), colW, outline.height()));

    p.end();
    return QIcon(pm);
}

// -----------------------------------------------------------------------
// downArrowPixmap
// -----------------------------------------------------------------------
QPixmap Icons::downArrowPixmap(const QColor &color, int size) {
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    double margin = size * 0.2;
    p.drawPolygon(QPolygonF({
        QPointF(margin,        size * 0.35),
        QPointF(size - margin, size * 0.35),
        QPointF(size / 2.0,   size * 0.7),
    }));
    p.end();
    return pm;
}
