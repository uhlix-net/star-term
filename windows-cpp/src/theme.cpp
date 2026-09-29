#include "theme.h"
#include "icons.h"

#include <QColor>
#include <QDir>
#include <QMap>
#include <QStandardPaths>

// -----------------------------------------------------------------------
// Color dictionaries (mirrors Python DARK / LIGHT dicts)
// -----------------------------------------------------------------------
// Palette derived from uhlix.net/product/star-term/ site design.
// Dark: deep navy bg + electric cyan accent (#00d9ff from --accent).
// Light: deliberately NOT a tinted complement of the dark theme — plain
// neutral grays in the Windows light-mode idiom, with blue reserved for
// selection/focus/pressed states only.
static ThemeColors DARK_COLORS = {
    "#0a0e17",  // BG            -- site --bg
    "#0d1220",  // PANEL_BG      -- site --bg-alt
    "#111828",  // ELEVATED_BG   -- menus / toolbars
    "#1e2a40",  // BORDER        -- site surface-border approximated opaque
    "#e7edf6",  // TEXT          -- site --text
    "#93a1b8",  // MUTED_TEXT    -- site --text-muted
    "#00d9ff",  // ACCENT        -- site --accent (electric cyan)
    "#33e4ff",  // ACCENT_HOVER  -- lighter cyan for focus rings / tab underline
    "#0a3040",  // SELECTION     -- dark teal list/text selection
    "#0d1220",  // INPUT_BG
    "#141e30",  // BUTTON_BG
    "#1a2840",  // BUTTON_HOVER_BG
    "#00d9ff",  // BUTTON_HOVER_BORDER
    "#0c1018",  // DISABLED_BG
    "#101827",  // ITEM_HOVER_BG
    "#1c2e4a",  // SCROLLBAR_HANDLE
    "#2a4060",  // SCROLLBAR_HANDLE_HOVER
    "#ff6b7a",  // ERROR_TEXT    -- warm red, legible on the navy background
};

static ThemeColors LIGHT_COLORS = {
    "#f3f3f3",  // BG            -- neutral window gray, no color cast
    "#ffffff",  // PANEL_BG
    "#fafafa",  // ELEVATED_BG   -- menus / toolbars, a hair above the window
    "#d4d4d4",  // BORDER        -- neutral gray rules
    "#1a1a1a",  // TEXT          -- near-black, ~15:1 on the window gray
    "#6b6b6b",  // MUTED_TEXT    -- ~4.9:1 on the window gray
    "#0067c0",  // ACCENT        -- standard blue; white text on it is ~5.6:1
    "#005a9e",  // ACCENT_HOVER  -- focus rings / selected tab underline
    "#cce4f7",  // SELECTION     -- conventional light selection fill
    "#ffffff",  // INPUT_BG
    "#fdfdfd",  // BUTTON_BG
    "#f0f0f0",  // BUTTON_HOVER_BG
    "#b3b3b3",  // BUTTON_HOVER_BORDER -- gray, so hover reads as depth not color
    "#ebebeb",  // DISABLED_BG
    "#ededed",  // ITEM_HOVER_BG
    "#c4c4c4",  // SCROLLBAR_HANDLE
    "#a0a0a0",  // SCROLLBAR_HANDLE_HOVER
    "#c42b1c",  // ERROR_TEXT    -- standard error red, legible on white
};

// -----------------------------------------------------------------------
// Down-arrow PNG cache (must be created after QApplication)
// -----------------------------------------------------------------------
static QMap<QString, QString> s_arrowCache;

QString arrowIconPath(const QString &colorHex) {
    if (s_arrowCache.contains(colorHex)) return s_arrowCache[colorHex];

    QString cleanHex = colorHex;
    cleanHex.remove('#');
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path = tempDir + "/star_term_arrow_" + cleanHex + ".png";
    path.replace("\\", "/");

    QPixmap pm = Icons::downArrowPixmap(QColor(colorHex));
    pm.save(path, "PNG");

    s_arrowCache[colorHex] = path;
    return path;
}

// -----------------------------------------------------------------------
// Stylesheet builder (mirrors _build_stylesheet in theme.py)
// -----------------------------------------------------------------------
static QString buildStylesheet(const ThemeColors &c) {
    // Arrow icon path is needed inside the stylesheet
    QString arrowPath = arrowIconPath(c.TEXT);

    return QString(R"(
QMainWindow, QDialog {
    background-color: %1;
    color: %5;
}

QWidget {
    color: %5;
    font-size: 10pt;
}

QLabel {
    background: transparent;
}

QLabel#sectionTitle {
    font-weight: bold;
    font-size: 11pt;
    padding: 2px 0 8px 0;
    border-bottom: 1px solid %4;
    margin-bottom: 4px;
}

QMenuBar {
    background-color: %3;
    border-bottom: 1px solid %4;
    padding: 2px;
}
QMenuBar::item {
    padding: 4px 10px;
    background: transparent;
    border-radius: 3px;
}
QMenuBar::item:selected, QMenuBar::item:pressed {
    background-color: %9;
}

QMenu {
    background-color: %3;
    border: 1px solid %4;
    padding: 4px;
}
QMenu::item {
    padding: 4px 24px 4px 12px;
    border-radius: 3px;
}
QMenu::item:selected {
    background-color: %9;
}
QMenu::separator {
    height: 1px;
    background: %4;
    margin: 4px 6px;
}

QToolBar {
    background-color: %3;
    border: none;
    border-bottom: 1px solid %4;
    spacing: 4px;
    padding: 4px;
}
QToolBar::separator {
    background: %4;
    width: 1px;
    margin: 4px 4px;
}
QToolButton {
    background: transparent;
    border: none;
    border-radius: 4px;
    padding: 4px;
}
QToolButton:hover {
    background-color: %9;
}
QToolButton:pressed {
    background-color: %7;
    color: #ffffff;
}
QToolButton:checked {
    background-color: %7;
    color: #ffffff;
}

QStatusBar {
    background-color: %3;
    border-top: 1px solid %4;
}
QStatusBar QLabel {
    padding: 0 8px;
    color: %6;
}

QPushButton {
    background-color: %11;
    border: 1px solid %4;
    border-radius: 4px;
    padding: 5px 14px;
}
QPushButton:hover {
    background-color: %12;
    border-color: %13;
}
QPushButton:pressed {
    background-color: %7;
    color: #ffffff;
}
QPushButton:disabled {
    color: %6;
    background-color: %14;
}
QPushButton:checked {
    background-color: %7;
    border-color: %8;
    color: #ffffff;
}

QLineEdit, QSpinBox, QComboBox, QFontComboBox, QTextEdit, QPlainTextEdit {
    background-color: %10;
    color: %5;
    border: 1px solid %4;
    border-radius: 3px;
    padding: 3px 6px;
    selection-background-color: %9;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QFontComboBox:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border: 1px solid %8;
}
QLineEdit:read-only {
    color: %6;
}
QComboBox::drop-down, QFontComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 20px;
    border-left: 1px solid %4;
    border-top-right-radius: 3px;
    border-bottom-right-radius: 3px;
    background-color: %11;
}
QComboBox::down-arrow, QFontComboBox::down-arrow {
    image: url(%16);
    width: 10px;
    height: 10px;
}
QComboBox QAbstractItemView {
    background-color: %10;
    border: 1px solid %4;
    selection-background-color: %9;
    color: %5;
    outline: none;
}
QLabel#mutedNote {
    color: %6;
    font-size: 9pt;
}
QLabel#errorNote {
    color: %19;
}
QCheckBox {
    spacing: 6px;
}

QScrollArea {
    background-color: %2;
    border: 1px solid %4;
    border-radius: 3px;
}
QScrollArea > QWidget > QWidget {
    background-color: %2;
}

QTreeWidget, QListWidget {
    background-color: %2;
    border: 1px solid %4;
    border-radius: 3px;
    outline: none;
}
QTreeWidget::item, QListWidget::item {
    padding: 2px 4px;
    border-radius: 3px;
}
QTreeWidget::item:selected, QListWidget::item:selected {
    background-color: %9;
}
QTreeWidget::item:hover, QListWidget::item:hover {
    background-color: %15;
}

QTabWidget::pane {
    border: 1px solid %4;
    border-radius: 3px;
    top: -1px;
}
QTabBar::tab {
    background: %3;
    color: %6;
    padding: 6px 16px;
    border: 1px solid %4;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
}
QTabBar::tab:selected {
    background: %2;
    color: %5;
    border-bottom: 2px solid %8;
}
QTabBar::tab:hover:!selected {
    color: %5;
}
QTabBar::close-button {
    margin-right: 4px;
}

QScrollBar:vertical {
    background: %1;
    width: 12px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: %17;
    min-height: 24px;
    border-radius: 5px;
    margin: 2px;
}
QScrollBar::handle:vertical:hover {
    background: %18;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}

QScrollBar:horizontal {
    background: %1;
    height: 12px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: %17;
    min-width: 24px;
    border-radius: 5px;
    margin: 2px;
}
QScrollBar::handle:horizontal:hover {
    background: %18;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}

QSplitter::handle {
    background-color: %4;
}
)")
        .arg(c.BG)            // %1
        .arg(c.PANEL_BG)      // %2
        .arg(c.ELEVATED_BG)   // %3
        .arg(c.BORDER)        // %4
        .arg(c.TEXT)          // %5
        .arg(c.MUTED_TEXT)    // %6
        .arg(c.ACCENT)        // %7
        .arg(c.ACCENT_HOVER)  // %8
        .arg(c.SELECTION)     // %9
        .arg(c.INPUT_BG)      // %10
        .arg(c.BUTTON_BG)     // %11
        .arg(c.BUTTON_HOVER_BG)     // %12
        .arg(c.BUTTON_HOVER_BORDER) // %13
        .arg(c.DISABLED_BG)         // %14
        .arg(c.ITEM_HOVER_BG)       // %15
        .arg(arrowPath)             // %16
        .arg(c.SCROLLBAR_HANDLE)    // %17
        .arg(c.SCROLLBAR_HANDLE_HOVER) // %18
        .arg(c.ERROR_TEXT);         // %19
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------
static QMap<QString, QString> s_cache;

QString getStylesheet(const QString &name) {
    QString key = (name == "light") ? "light" : "dark";
    if (s_cache.contains(key)) return s_cache[key];
    QString ss = buildStylesheet(key == "light" ? LIGHT_COLORS : DARK_COLORS);
    s_cache[key] = ss;
    return ss;
}

void clearStylesheetCache() {
    s_cache.clear();
}
