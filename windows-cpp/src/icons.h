#pragma once
#include <QIcon>
#include <QPixmap>
#include <QColor>

// Programmatically-drawn icons (no bundled image assets needed).
//
// Every icon is rendered at 1x, 2x and 3x and handed to QIcon as three
// pixmaps, so Windows display scaling picks a sharp one instead of
// stretching a 16px bitmap. Results are cached per (icon, size).

namespace Icons {

// Color constants
inline const QColor ACCENT_GREEN  = QColor(39, 201, 63);
inline const QColor ACCENT_RED    = QColor(232, 71, 71);
inline const QColor ACCENT_BLUE   = QColor(86, 156, 214);
inline const QColor DARK_BG       = QColor(30, 30, 33);
inline const QColor LIGHT         = QColor(235, 235, 235);
inline const QColor GRAY          = QColor(150, 150, 150);
inline const QColor FOLDER_COLOR  = QColor(240, 185, 90);
inline const QColor FOLDER_DARK   = QColor(214, 160, 70);
// Mid grays, chosen to hold up on both the navy dark theme and the
// neutral light theme (~3.6:1 and ~5.3:1 against their backgrounds).
inline const QColor ICON_FG       = QColor(110, 110, 110);
inline const QColor ICON_FG_MUTED = QColor(78, 78, 78);

// Session glyph palette (saved-session tree, issue #39)
// Slate, not black: dark enough to read as a terminal on the light theme,
// light enough to separate from the navy background on the dark one.
inline const QColor TERM_TILE     = QColor(51, 62, 82);    // slate tile
inline const QColor TERM_PROMPT   = QColor(49, 208, 122);  // prompt chevron
inline const QColor TERM_CURSOR   = QColor(207, 216, 230); // cursor bar
inline const QColor RDP_FRAME     = QColor(47, 111, 208);  // monitor body
inline const QColor RDP_SCREEN    = QColor(226, 237, 252); // lit screen
inline const QColor RDP_BAR       = QColor(150, 187, 233); // window title bar

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
QIcon    logIcon(int size = 24);
QPixmap  downArrowPixmap(const QColor &color, int size = 10);

} // namespace Icons
