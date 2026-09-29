#include "icons.h"

#include <QApplication>
#include <QCoreApplication>
#include <QHash>
#include <QPainter>
#include <QPolygonF>
#include <QPointF>
#include <QRectF>
#include <QPen>
#include <cmath>

using namespace Icons;

// -----------------------------------------------------------------------
// Rendering helper
//
// Each icon is a draw function that paints into a square of edge length
// `s`, with every coordinate expressed as a fraction of `s`. makeIcon()
// runs it three times — at 1x, 2x and 3x the requested size — and adds all
// three to the QIcon. Qt then picks the pixmap matching the current
// device pixel ratio, so the icon stays sharp at 150%, 200% and 300%
// Windows scaling instead of being a stretched 16px bitmap.
// -----------------------------------------------------------------------
using DrawFn = void (*)(QPainter &, qreal);

static QHash<QString, QIcon> s_iconCache;

// The cache holds pixmaps, which must not outlive QApplication — drop them
// while it is still being torn down rather than at static destruction time.
static void releaseIconCache() { s_iconCache.clear(); }

static QIcon makeIcon(const char *key, int size, DrawFn draw) {
    const QString cacheKey = QString::asprintf("%s@%d", key, size);
    auto cached = s_iconCache.constFind(cacheKey);
    if (cached != s_iconCache.constEnd()) return *cached;

    static bool cleanupRegistered = false;
    if (!cleanupRegistered) {
        qAddPostRoutine(releaseIconCache);
        cleanupRegistered = true;
    }

    QIcon icon;
    for (int scale : {1, 2, 3}) {
        const int px = size * scale;
        QPixmap pm(px, px);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        draw(p, static_cast<qreal>(px));
        p.end();
        icon.addPixmap(pm);
    }
    s_iconCache.insert(cacheKey, icon);
    return icon;
}

// -----------------------------------------------------------------------
// connect_icon  — green circle with white play triangle
// -----------------------------------------------------------------------
static void drawConnect(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);
    p.setBrush(ACCENT_GREEN);
    p.drawEllipse(QRectF(s * 0.04, s * 0.04, s * 0.92, s * 0.92));
    p.setBrush(LIGHT);
    p.drawPolygon(QPolygonF({
        QPointF(s * 0.36, s * 0.27),
        QPointF(s * 0.36, s * 0.73),
        QPointF(s * 0.76, s * 0.5),
    }));
}

QIcon Icons::connectIcon(int size) {
    return makeIcon("connect", size, drawConnect);
}

// -----------------------------------------------------------------------
// disconnect_icon  — red circle with white square
// -----------------------------------------------------------------------
static void drawDisconnect(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);
    p.setBrush(ACCENT_RED);
    p.drawEllipse(QRectF(s * 0.04, s * 0.04, s * 0.92, s * 0.92));
    p.setBrush(LIGHT);
    const qreal side = s * 0.26;
    p.drawRoundedRect(QRectF(s / 2.0 - side / 2.0, s / 2.0 - side / 2.0, side, side),
                      s * 0.03, s * 0.03);
}

QIcon Icons::disconnectIcon(int size) {
    return makeIcon("disconnect", size, drawDisconnect);
}

// -----------------------------------------------------------------------
// multi_exec_icon  — 2x2 grid of blue rounded squares
// -----------------------------------------------------------------------
static void drawMultiExec(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);
    p.setBrush(ACCENT_BLUE);
    const qreal gap  = s * 0.1;
    const qreal cell = (s - 3.0 * gap) / 2.0;
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 2; ++col) {
            const qreal x = gap + col * (cell + gap);
            const qreal y = gap + row * (cell + gap);
            p.drawRoundedRect(QRectF(x, y, cell, cell), s * 0.08, s * 0.08);
        }
    }
}

QIcon Icons::multiExecIcon(int size) {
    return makeIcon("multiexec", size, drawMultiExec);
}

// -----------------------------------------------------------------------
// terminal tile  — shared by terminalIcon / sessionsIcon / sshIcon
//
// Rounded slate tile, green prompt chevron, light cursor bar. Tuned to
// stay readable at 16px in the saved-sessions tree: generous corner
// radius, one chevron rather than a chevron plus an underline, and a
// solid cursor block instead of a hairline.
// -----------------------------------------------------------------------
static void drawTerminal(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);
    p.setBrush(TERM_TILE);
    p.drawRoundedRect(QRectF(s * 0.06, s * 0.10, s * 0.88, s * 0.80),
                      s * 0.19, s * 0.19);

    QPen pen(TERM_PROMPT);
    pen.setWidthF(s * 0.085);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawPolyline(QPolygonF({
        QPointF(s * 0.27, s * 0.36),
        QPointF(s * 0.43, s * 0.50),
        QPointF(s * 0.27, s * 0.64),
    }));

    p.setPen(Qt::NoPen);
    p.setBrush(TERM_CURSOR);
    p.drawRoundedRect(QRectF(s * 0.50, s * 0.575, s * 0.23, s * 0.085),
                      s * 0.04, s * 0.04);
}

QIcon Icons::terminalIcon(int size) {
    return makeIcon("terminal", size, drawTerminal);
}

// -----------------------------------------------------------------------
// app_icon  — loaded from bundled PNG resource
// -----------------------------------------------------------------------
QIcon Icons::appIcon() {
    return QIcon(":/icon.png");
}

// -----------------------------------------------------------------------
// folder_icon  — two-tone folder, back sheet + tab + front sheet
// -----------------------------------------------------------------------
static void drawFolder(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);
    const qreal r = s * 0.11;

    // Tab and back sheet
    p.setBrush(FOLDER_DARK);
    p.drawRoundedRect(QRectF(s * 0.07, s * 0.18, s * 0.40, s * 0.20), r * 0.6, r * 0.6);
    p.drawRoundedRect(QRectF(s * 0.07, s * 0.26, s * 0.86, s * 0.56), r, r);

    // Front sheet, offset down so the back edge stays visible
    p.setBrush(FOLDER_COLOR);
    p.drawRoundedRect(QRectF(s * 0.07, s * 0.36, s * 0.86, s * 0.46), r, r);
}

QIcon Icons::folderIcon(int size) {
    return makeIcon("folder", size, drawFolder);
}

QIcon Icons::directoryIcon(int size) {
    return folderIcon(size);
}

// -----------------------------------------------------------------------
// file_icon
// -----------------------------------------------------------------------
static void drawFile(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    const qreal fold = s * 0.28;
    p.drawPolygon(QPolygonF({
        QPointF(s * 0.22, s * 0.06),
        QPointF(s * 0.78 - fold, s * 0.06),
        QPointF(s * 0.78, s * 0.06 + fold),
        QPointF(s * 0.78, s * 0.94),
        QPointF(s * 0.22, s * 0.94),
    }));
    p.setBrush(ICON_FG_MUTED);
    p.drawPolygon(QPolygonF({
        QPointF(s * 0.78 - fold, s * 0.06),
        QPointF(s * 0.78, s * 0.06 + fold),
        QPointF(s * 0.78 - fold, s * 0.06 + fold),
    }));
    QPen pen(ICON_FG_MUTED);
    pen.setWidthF(s * 0.06);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    for (int i = 0; i < 3; ++i) {
        const qreal y = s * (0.45 + i * 0.14);
        p.drawLine(QPointF(s * 0.32, y), QPointF(s * 0.68, y));
    }
}

QIcon Icons::fileIcon(int size) {
    return makeIcon("file", size, drawFile);
}

// -----------------------------------------------------------------------
// up_icon
// -----------------------------------------------------------------------
static void drawUp(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    p.drawPolygon(QPolygonF({
        QPointF(s * 0.5,  s * 0.18),
        QPointF(s * 0.82, s * 0.55),
        QPointF(s * 0.62, s * 0.55),
        QPointF(s * 0.62, s * 0.85),
        QPointF(s * 0.38, s * 0.85),
        QPointF(s * 0.38, s * 0.55),
        QPointF(s * 0.18, s * 0.55),
    }));
}

QIcon Icons::upIcon(int size) {
    return makeIcon("up", size, drawUp);
}

// -----------------------------------------------------------------------
// refresh_icon
// -----------------------------------------------------------------------
static void drawRefresh(QPainter &p, qreal s) {
    QPen pen(ICON_FG);
    pen.setWidthF(s * 0.12);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(s * 0.18, s * 0.18, s * 0.64, s * 0.64), 30 * 16, 280 * 16);
    p.setBrush(ICON_FG);
    p.setPen(Qt::NoPen);
    p.drawPolygon(QPolygonF({
        QPointF(s * 0.82, s * 0.18),
        QPointF(s * 0.82, s * 0.4),
        QPointF(s * 0.6,  s * 0.3),
    }));
}

QIcon Icons::refreshIcon(int size) {
    return makeIcon("refresh", size, drawRefresh);
}

QIcon Icons::sessionsIcon(int size) {
    return terminalIcon(size);
}

// -----------------------------------------------------------------------
// macros_icon  — lightning bolt
// -----------------------------------------------------------------------
static void drawMacros(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    p.drawPolygon(QPolygonF({
        QPointF(s * 0.58, s * 0.04),
        QPointF(s * 0.2,  s * 0.56),
        QPointF(s * 0.46, s * 0.56),
        QPointF(s * 0.42, s * 0.96),
        QPointF(s * 0.8,  s * 0.44),
        QPointF(s * 0.54, s * 0.44),
    }));
}

QIcon Icons::macrosIcon(int size) {
    return makeIcon("macros", size, drawMacros);
}

// -----------------------------------------------------------------------
// settings_icon  — gear
// -----------------------------------------------------------------------
static void drawSettings(QPainter &p, qreal s) {
    p.translate(s / 2.0, s / 2.0);

    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    const qreal radius = s * 0.34;
    const qreal toothW = s * 0.16;
    const qreal toothH = s * 0.16;
    for (int i = 0; i < 8; ++i) {
        p.save();
        p.rotate(i * 45.0);
        p.drawRoundedRect(
            QRectF(-toothW / 2.0, -(radius + toothH * 0.55), toothW, toothH),
            s * 0.06, s * 0.06
        );
        p.restore();
    }
    p.drawEllipse(QRectF(-radius, -radius, radius * 2, radius * 2));

    p.setCompositionMode(QPainter::CompositionMode_Clear);
    const qreal hole = radius * 0.5;
    p.drawEllipse(QRectF(-hole, -hole, hole * 2, hole * 2));
}

QIcon Icons::settingsIcon(int size) {
    return makeIcon("settings", size, drawSettings);
}

// -----------------------------------------------------------------------
// sshIcon  — the terminal tile (saved SSH/WSL sessions)
// -----------------------------------------------------------------------
QIcon Icons::sshIcon(int size) {
    return terminalIcon(size);
}

// -----------------------------------------------------------------------
// rdpIcon  — monitor with a lit screen and a window title bar
//
// Paired with the SSH tile: different silhouette (wide monitor on a
// stand vs. a square tile) and different color, so the two session types
// are told apart by shape alone at 16px, not just by color.
// -----------------------------------------------------------------------
static void drawRdp(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);

    // Monitor body
    p.setBrush(RDP_FRAME);
    p.drawRoundedRect(QRectF(s * 0.04, s * 0.13, s * 0.92, s * 0.60),
                      s * 0.11, s * 0.11);

    // Lit screen
    p.setBrush(RDP_SCREEN);
    p.drawRoundedRect(QRectF(s * 0.14, s * 0.22, s * 0.72, s * 0.42),
                      s * 0.05, s * 0.05);

    // Window title bar inside the screen — the "remote desktop" hint
    p.setBrush(RDP_BAR);
    p.drawRoundedRect(QRectF(s * 0.21, s * 0.29, s * 0.58, s * 0.09),
                      s * 0.035, s * 0.035);

    // Neck and base
    p.setBrush(RDP_FRAME);
    p.drawRect(QRectF(s * 0.44, s * 0.73, s * 0.12, s * 0.11));
    p.drawRoundedRect(QRectF(s * 0.26, s * 0.83, s * 0.48, s * 0.09),
                      s * 0.045, s * 0.045);
}

QIcon Icons::rdpIcon(int size) {
    return makeIcon("rdp", size, drawRdp);
}

// -----------------------------------------------------------------------
// sidebarToggleIcon  — panel outline with a filled left column
// -----------------------------------------------------------------------
static void drawSidebarToggle(QPainter &p, qreal s) {
    const QRectF outline(s * 0.1, s * 0.16, s * 0.8, s * 0.68);
    QPen pen(ICON_FG);
    pen.setWidthF(s * 0.08);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(outline, s * 0.08, s * 0.08);

    p.setPen(Qt::NoPen);
    p.setBrush(ICON_FG);
    const qreal colW = outline.width() * 0.36;
    p.drawRect(QRectF(outline.left(), outline.top(), colW, outline.height()));
}

QIcon Icons::sidebarToggleIcon(int size) {
    return makeIcon("sidebartoggle", size, drawSidebarToggle);
}

// -----------------------------------------------------------------------
// logIcon — document with horizontal lines + red record dot
// -----------------------------------------------------------------------
static void drawLog(QPainter &p, qreal s) {
    p.setPen(Qt::NoPen);

    // Document body
    p.setBrush(ICON_FG);
    const qreal fold = s * 0.24;
    p.drawPolygon(QPolygonF({
        QPointF(s * 0.16, s * 0.06),
        QPointF(s * 0.76 - fold, s * 0.06),
        QPointF(s * 0.76, s * 0.06 + fold),
        QPointF(s * 0.76, s * 0.94),
        QPointF(s * 0.16, s * 0.94),
    }));

    // Fold corner
    p.setBrush(ICON_FG_MUTED);
    p.drawPolygon(QPolygonF({
        QPointF(s * 0.76 - fold, s * 0.06),
        QPointF(s * 0.76,        s * 0.06 + fold),
        QPointF(s * 0.76 - fold, s * 0.06 + fold),
    }));

    // Text lines (cleared from document)
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    for (int i = 0; i < 4; ++i) {
        const qreal y = s * (0.38 + i * 0.135);
        const qreal w = (i == 3) ? s * 0.28 : s * 0.42;
        p.fillRect(QRectF(s * 0.26, y, w, s * 0.07), Qt::black);
    }

    // Red record dot (top-right overlay)
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setBrush(ACCENT_RED);
    const qreal dotR = s * 0.16;
    p.drawEllipse(QRectF(s * 0.62, s * 0.02, dotR * 2, dotR * 2));
}

QIcon Icons::logIcon(int size) {
    return makeIcon("log", size, drawLog);
}

// -----------------------------------------------------------------------
// downArrowPixmap
//
// Written to a PNG for QSS url() references, so it is rendered at the
// primary screen's device pixel ratio rather than the 1x/2x/3x set —
// a stylesheet can only name one file.
// -----------------------------------------------------------------------
QPixmap Icons::downArrowPixmap(const QColor &color, int size) {
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const int px = qMax(1, qRound(size * dpr));

    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    const qreal s = size;
    const qreal margin = s * 0.2;
    p.drawPolygon(QPolygonF({
        QPointF(margin,     s * 0.35),
        QPointF(s - margin, s * 0.35),
        QPointF(s / 2.0,    s * 0.7),
    }));
    p.end();
    return pm;
}
