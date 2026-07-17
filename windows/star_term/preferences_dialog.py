from PySide6.QtGui import QFont, QIntValidator
from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFileDialog,
    QFontComboBox, QFormLayout, QHBoxLayout, QLabel, QLineEdit, QPushButton,
    QTabWidget, QVBoxLayout, QWidget,
)

from . import debug
from .config import load_settings, save_settings


class PreferencesDialog(QDialog):
    """Tabbed dialog combining the SSH Key and Terminal settings that were
    previously separate Settings menu entries."""

    def __init__(self, parent=None, font_family="Courier New",
                 font_size=10, cursor_style="underline", theme_name="dark"):
        super().__init__(parent)
        self.setWindowTitle("Preferences")
        self.settings = load_settings()

        tabs = QTabWidget()
        tabs.addTab(self._build_general_tab(theme_name), "General")
        tabs.addTab(self._build_terminal_tab(font_family, font_size, cursor_style), "Terminal")
        tabs.addTab(self._build_ssh_tab(), "SSH Key")

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)

        layout = QVBoxLayout(self)
        layout.addWidget(tabs)
        layout.addWidget(buttons)

    # ------------------------------------------------------------------
    # General tab
    # ------------------------------------------------------------------

    def _build_general_tab(self, theme_name) -> QWidget:
        widget = QWidget()

        self.theme_combo = QComboBox()
        self.theme_combo.addItems(["Dark", "Light"])
        self.theme_combo.setCurrentText("Light" if theme_name == "light" else "Dark")

        self.debug_checkbox = QCheckBox("Enable debug logging")
        self.debug_checkbox.setChecked(self.settings.get("debug", False))

        debug_note = QLabel(f"Log file: {debug.get_log_path()}")
        debug_note.setObjectName("mutedNote")
        debug_note.setWordWrap(True)

        form = QFormLayout(widget)
        form.addRow("Appearance:", self.theme_combo)
        form.addRow(self.debug_checkbox)
        form.addRow(debug_note)
        return widget

    def get_general_settings(self) -> dict:
        return {
            "theme": self.theme_combo.currentText().lower(),
            "debug": self.debug_checkbox.isChecked(),
        }

    # ------------------------------------------------------------------
    # SSH Key tab
    # ------------------------------------------------------------------

    def _build_ssh_tab(self) -> QWidget:
        widget = QWidget()

        self.key_path_edit = QLineEdit(self.settings.get("ssh_key_path", ""))
        self.key_path_edit.setReadOnly(True)

        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_key)
        remove_btn = QPushButton("Remove")
        remove_btn.clicked.connect(self._remove_key)

        key_row = QWidget()
        key_layout = QHBoxLayout(key_row)
        key_layout.setContentsMargins(0, 0, 0, 0)
        key_layout.addWidget(self.key_path_edit)
        key_layout.addWidget(browse_btn)
        key_layout.addWidget(remove_btn)

        note = QLabel("This option can be over-ridden on the session profile.")
        note.setObjectName("mutedNote")

        form = QFormLayout(widget)
        form.addRow("Default SSH key:", key_row)
        form.addRow(note)
        return widget

    def _browse_key(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select SSH Private Key")
        if not path:
            return
        self.settings["ssh_key_path"] = path
        save_settings(self.settings)
        self.key_path_edit.setText(path)

    def _remove_key(self):
        self.settings.pop("ssh_key_path", None)
        save_settings(self.settings)
        self.key_path_edit.setText("")

    # ------------------------------------------------------------------
    # Terminal tab
    # ------------------------------------------------------------------

    def _build_terminal_tab(self, font_family, font_size, cursor_style) -> QWidget:
        widget = QWidget()

        self.font_combo = QFontComboBox()
        self.font_combo.setCurrentFont(QFont(font_family))

        self.size_combo = QComboBox()
        self.size_combo.setEditable(True)
        self.size_combo.addItems(
            [str(s) for s in (6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36)]
        )
        self.size_combo.setValidator(QIntValidator(6, 72, self.size_combo))
        self.size_combo.setCurrentText(str(font_size))

        self.cursor_combo = QComboBox()
        self.cursor_combo.addItems(["Underline", "Block"])
        self.cursor_combo.setCurrentText("Block" if cursor_style == "block" else "Underline")

        form = QFormLayout(widget)
        form.addRow("Font:", self.font_combo)
        form.addRow("Size:", self.size_combo)
        form.addRow("Cursor style:", self.cursor_combo)
        return widget

    def get_terminal_settings(self) -> dict:
        return {
            "font_family": self.font_combo.currentFont().family(),
            "font_size": int(self.size_combo.currentText()),
            "cursor_style": self.cursor_combo.currentText().lower(),
        }
