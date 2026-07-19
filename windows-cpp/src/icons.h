#pragma once
#include <QIcon>
#include <QPixmap>
#include <QColor>

// Programmatically-drawn icons (no bundled image assets needed).
// Matches icons.py exactly.

namespace Icons {

// Color constants matching Python icons.py
inline const QColor ACCENT_GREEN  = QColor(39, 201, 63);
inline const QColor ACCENT_RED    = QColor(232, 71, 71);
inline const QColor ACCENT_BLUE   = QColor(86, 156, 214);
inline const QColor DARK_BG       = QColor(30, 30, 33);
inline const QColor LIGHT         = QColor(235, 235, 235);
inline const QColor GRAY          = QColor(150, 150, 150);
inline const QColor FOLDER_COLOR  = QColor(240, 185, 90);
inline const QColor FOLDER_DARK   = QColor(214, 160, 70);
inline const QColor ICON_FG       = QColor(140, 140, 140);
inline const QColor ICON_FG_MUTED = QColor(95, 95, 95);

QIcon    connectIcon(int size = 24);
QIcon    disconnectIcon(int size = 24);
QIcon    multiExecIcon(int size = 24);
QIcon    terminalIcon(int size = 24);
QIcon    appIcon();
QIcon    folderIcon(int size = 24);
QIcon    directoryIcon(int size = 24);
QIcon    fileIcon(int size = 24);
QIcon    upIcon(int size = 24);
QIcon    refreshIcon(int size = 24);
QIcon    sessionsIcon(int size = 24);
QIcon    macrosIcon(int size = 24);
QIcon    settingsIcon(int size = 24);
QIcon    sshIcon(int size = 24);
QIcon    rdpIcon(int size = 24);
QIcon    sidebarToggleIcon(int size = 24);
QPixmap  downArrowPixmap(const QColor &color, int size = 10);

} // namespace Icons
