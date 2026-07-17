"""Application-wide Light/Dark theme stylesheets for a consistent, modern look."""

import os
import tempfile

from PySide6.QtGui import QColor

DARK = {
    "BG": "#1e1e1e",
    "PANEL_BG": "#252526",
    "ELEVATED_BG": "#2d2d2d",
    "BORDER": "#3c3c3c",
    "TEXT": "#cccccc",
    "MUTED_TEXT": "#8a8a8a",
    "ACCENT": "#0e639c",
    "ACCENT_HOVER": "#1177bb",
    "SELECTION": "#094771",
    "INPUT_BG": "#3c3c3c",
    "BUTTON_BG": "#3a3d41",
    "BUTTON_HOVER_BG": "#45494e",
    "BUTTON_HOVER_BORDER": "#5a5a5a",
    "DISABLED_BG": "#2d2d2d",
    "ITEM_HOVER_BG": "#2a2d2e",
    "SCROLLBAR_HANDLE": "#5a5a5a",
    "SCROLLBAR_HANDLE_HOVER": "#6e6e6e",
}

LIGHT = {
    "BG": "#f3f3f3",
    "PANEL_BG": "#ffffff",
    "ELEVATED_BG": "#e7e7e7",
    "BORDER": "#cccccc",
    "TEXT": "#1e1e1e",
    "MUTED_TEXT": "#6e6e6e",
    "ACCENT": "#0e639c",
    "ACCENT_HOVER": "#1177bb",
    "SELECTION": "#cce4f7",
    "INPUT_BG": "#ffffff",
    "BUTTON_BG": "#e1e1e1",
    "BUTTON_HOVER_BG": "#d6d6d6",
    "BUTTON_HOVER_BORDER": "#adadad",
    "DISABLED_BG": "#ececec",
    "ITEM_HOVER_BG": "#e5f1fb",
    "SCROLLBAR_HANDLE": "#c1c1c1",
    "SCROLLBAR_HANDLE_HOVER": "#a8a8a8",
}

THEMES = {"dark": DARK, "light": LIGHT}

_ARROW_ICON_CACHE = {}


def _arrow_icon_path(color_hex: str) -> str:
    """Renders (and caches) a small down-arrow PNG in the given color for use
    as the QComboBox drop-down indicator, returning a file:// path usable in
    a QSS url()."""
    if color_hex not in _ARROW_ICON_CACHE:
        from . import icons

        path = os.path.join(tempfile.gettempdir(), f"star_term_arrow_{color_hex.lstrip('#')}.png")
        icons.down_arrow_pixmap(QColor(color_hex)).save(path)
        _ARROW_ICON_CACHE[color_hex] = path.replace("\\", "/")
    return _ARROW_ICON_CACHE[color_hex]


def _build_stylesheet(c: dict) -> str:
    return f"""
QMainWindow, QDialog {{
    background-color: {c['BG']};
    color: {c['TEXT']};
}}

QWidget {{
    color: {c['TEXT']};
    font-size: 10pt;
}}

QLabel {{
    background: transparent;
}}

QLabel#sectionTitle {{
    font-weight: bold;
    font-size: 11pt;
    padding: 2px 0 8px 0;
    border-bottom: 1px solid {c['BORDER']};
    margin-bottom: 4px;
}}

QMenuBar {{
    background-color: {c['ELEVATED_BG']};
    border-bottom: 1px solid {c['BORDER']};
    padding: 2px;
}}
QMenuBar::item {{
    padding: 4px 10px;
    background: transparent;
    border-radius: 3px;
}}
QMenuBar::item:selected, QMenuBar::item:pressed {{
    background-color: {c['SELECTION']};
}}

QMenu {{
    background-color: {c['ELEVATED_BG']};
    border: 1px solid {c['BORDER']};
    padding: 4px;
}}
QMenu::item {{
    padding: 4px 24px 4px 12px;
    border-radius: 3px;
}}
QMenu::item:selected {{
    background-color: {c['SELECTION']};
}}
QMenu::separator {{
    height: 1px;
    background: {c['BORDER']};
    margin: 4px 6px;
}}

QToolBar {{
    background-color: {c['ELEVATED_BG']};
    border: none;
    border-bottom: 1px solid {c['BORDER']};
    spacing: 4px;
    padding: 4px;
}}
QToolBar::separator {{
    background: {c['BORDER']};
    width: 1px;
    margin: 4px 4px;
}}
QToolButton {{
    background: transparent;
    border: none;
    border-radius: 4px;
    padding: 4px;
}}
QToolButton:hover {{
    background-color: {c['SELECTION']};
}}
QToolButton:pressed {{
    background-color: {c['ACCENT']};
    color: #ffffff;
}}
QToolButton:checked {{
    background-color: {c['ACCENT']};
    color: #ffffff;
}}

QStatusBar {{
    background-color: {c['ELEVATED_BG']};
    border-top: 1px solid {c['BORDER']};
}}
QStatusBar QLabel {{
    padding: 0 8px;
    color: {c['MUTED_TEXT']};
}}

QPushButton {{
    background-color: {c['BUTTON_BG']};
    border: 1px solid {c['BORDER']};
    border-radius: 4px;
    padding: 5px 14px;
}}
QPushButton:hover {{
    background-color: {c['BUTTON_HOVER_BG']};
    border-color: {c['BUTTON_HOVER_BORDER']};
}}
QPushButton:pressed {{
    background-color: {c['ACCENT']};
    color: #ffffff;
}}
QPushButton:disabled {{
    color: {c['MUTED_TEXT']};
    background-color: {c['DISABLED_BG']};
}}
QPushButton:checked {{
    background-color: {c['ACCENT']};
    border-color: {c['ACCENT_HOVER']};
    color: #ffffff;
}}

QLineEdit, QSpinBox, QComboBox, QFontComboBox, QTextEdit, QPlainTextEdit {{
    background-color: {c['INPUT_BG']};
    color: {c['TEXT']};
    border: 1px solid {c['BORDER']};
    border-radius: 3px;
    padding: 3px 6px;
    selection-background-color: {c['SELECTION']};
}}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QFontComboBox:focus, QTextEdit:focus, QPlainTextEdit:focus {{
    border: 1px solid {c['ACCENT_HOVER']};
}}
QLineEdit:read-only {{
    color: {c['MUTED_TEXT']};
}}
QComboBox::drop-down, QFontComboBox::drop-down {{
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 20px;
    border-left: 1px solid {c['BORDER']};
    border-top-right-radius: 3px;
    border-bottom-right-radius: 3px;
    background-color: {c['BUTTON_BG']};
}}
QComboBox::down-arrow, QFontComboBox::down-arrow {{
    image: url({_arrow_icon_path(c['TEXT'])});
    width: 10px;
    height: 10px;
}}
QComboBox QAbstractItemView {{
    background-color: {c['INPUT_BG']};
    border: 1px solid {c['BORDER']};
    selection-background-color: {c['SELECTION']};
    color: {c['TEXT']};
    outline: none;
}}
QLabel#mutedNote {{
    color: {c['MUTED_TEXT']};
    font-size: 9pt;
}}
QCheckBox {{
    spacing: 6px;
}}

QScrollArea {{
    background-color: {c['PANEL_BG']};
    border: 1px solid {c['BORDER']};
    border-radius: 3px;
}}
QScrollArea > QWidget > QWidget {{
    background-color: {c['PANEL_BG']};
}}

QTreeWidget, QListWidget {{
    background-color: {c['PANEL_BG']};
    border: 1px solid {c['BORDER']};
    border-radius: 3px;
    outline: none;
}}
QTreeWidget::item, QListWidget::item {{
    padding: 2px 4px;
    border-radius: 3px;
}}
QTreeWidget::item:selected, QListWidget::item:selected {{
    background-color: {c['SELECTION']};
}}
QTreeWidget::item:hover, QListWidget::item:hover {{
    background-color: {c['ITEM_HOVER_BG']};
}}

QTabWidget::pane {{
    border: 1px solid {c['BORDER']};
    border-radius: 3px;
    top: -1px;
}}
QTabBar::tab {{
    background: {c['ELEVATED_BG']};
    color: {c['MUTED_TEXT']};
    padding: 6px 16px;
    border: 1px solid {c['BORDER']};
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
}}
QTabBar::tab:selected {{
    background: {c['PANEL_BG']};
    color: {c['TEXT']};
    border-bottom: 2px solid {c['ACCENT_HOVER']};
}}
QTabBar::tab:hover:!selected {{
    color: {c['TEXT']};
}}
QTabBar::close-button {{
    margin-right: 4px;
}}

QScrollBar:vertical {{
    background: {c['BG']};
    width: 12px;
    margin: 0;
}}
QScrollBar::handle:vertical {{
    background: {c['SCROLLBAR_HANDLE']};
    min-height: 24px;
    border-radius: 5px;
    margin: 2px;
}}
QScrollBar::handle:vertical:hover {{
    background: {c['SCROLLBAR_HANDLE_HOVER']};
}}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
    height: 0;
}}

QScrollBar:horizontal {{
    background: {c['BG']};
    height: 12px;
    margin: 0;
}}
QScrollBar::handle:horizontal {{
    background: {c['SCROLLBAR_HANDLE']};
    min-width: 24px;
    border-radius: 5px;
    margin: 2px;
}}
QScrollBar::handle:horizontal:hover {{
    background: {c['SCROLLBAR_HANDLE_HOVER']};
}}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
    width: 0;
}}

QSplitter::handle {{
    background-color: {c['BORDER']};
}}
"""


_STYLESHEET_CACHE = {}


def get_stylesheet(name: str) -> str:
    if name not in THEMES:
        name = "dark"
    if name not in _STYLESHEET_CACHE:
        _STYLESHEET_CACHE[name] = _build_stylesheet(THEMES[name])
    return _STYLESHEET_CACHE[name]
