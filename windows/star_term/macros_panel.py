from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QCheckBox, QDialog, QDialogButtonBox, QFormLayout, QHBoxLayout, QLabel,
    QLineEdit, QListWidget, QListWidgetItem, QPlainTextEdit, QPushButton,
    QVBoxLayout, QWidget,
)

from .config import load_macros, save_macros


class MacroEditDialog(QDialog):
    def __init__(self, parent=None, macro=None):
        super().__init__(parent)
        self.setWindowTitle("Macro")
        macro = macro or {}

        self.name_edit = QLineEdit(macro.get("name", ""))

        self.commands_edit = QPlainTextEdit(macro.get("commands", ""))
        self.commands_edit.setPlaceholderText(
            "Commands/keystrokes to send, one per line.\n"
            "Each line but the last is sent followed by Enter."
        )
        self.commands_edit.setMinimumSize(360, 160)

        self.auto_execute_checkbox = QCheckBox(
            "Press Enter after the last line (execute immediately)"
        )
        self.auto_execute_checkbox.setChecked(macro.get("auto_execute", False))

        form = QFormLayout()
        form.addRow("Name:", self.name_edit)
        form.addRow("Commands:", self.commands_edit)
        form.addRow(self.auto_execute_checkbox)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)

        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(buttons)

    def get_macro(self) -> dict:
        return {
            "name": self.name_edit.text().strip() or "Macro",
            "commands": self.commands_edit.toPlainText(),
            "auto_execute": self.auto_execute_checkbox.isChecked(),
        }


class MacrosPanel(QWidget):
    """Saved keystroke/command sequences that can be sent ("pasted") to the
    active remote session, or broadcast to all sessions in Multi-Exec View."""

    run_requested = Signal(str, bool)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.macros = load_macros()

        self.list_widget = QListWidget()
        self.list_widget.itemDoubleClicked.connect(self._on_run)

        add_btn = QPushButton("Add")
        add_btn.clicked.connect(self._on_add)
        edit_btn = QPushButton("Edit")
        edit_btn.clicked.connect(self._on_edit)
        remove_btn = QPushButton("Remove")
        remove_btn.clicked.connect(self._on_remove)

        edit_row = QWidget()
        edit_layout = QHBoxLayout(edit_row)
        edit_layout.setContentsMargins(0, 0, 0, 0)
        edit_layout.setSpacing(6)
        edit_layout.addWidget(add_btn)
        edit_layout.addWidget(edit_btn)
        edit_layout.addWidget(remove_btn)

        run_btn = QPushButton("Send to Session")
        run_btn.clicked.connect(self._on_run)

        note = QLabel(
            "Sends the macro's commands to the active session, or to all "
            "sessions when Multi-Exec View is on. Unless 'Execute "
            "immediately' is set, the last line is typed but not run."
        )
        note.setObjectName("mutedNote")
        note.setWordWrap(True)

        title = QLabel("Macros")
        title.setObjectName("sectionTitle")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        layout.addWidget(title)
        layout.addWidget(self.list_widget)
        layout.addWidget(edit_row)
        layout.addWidget(run_btn)
        layout.addWidget(note)

        self._populate()

    def _populate(self):
        self.list_widget.clear()
        for macro in self.macros:
            self.list_widget.addItem(QListWidgetItem(macro["name"]))

    def _current_index(self):
        row = self.list_widget.currentRow()
        return row if 0 <= row < len(self.macros) else None

    def _on_add(self):
        dialog = MacroEditDialog(self)
        if dialog.exec():
            self.macros.append(dialog.get_macro())
            save_macros(self.macros)
            self._populate()

    def _on_edit(self):
        idx = self._current_index()
        if idx is None:
            return
        dialog = MacroEditDialog(self, self.macros[idx])
        if dialog.exec():
            self.macros[idx] = dialog.get_macro()
            save_macros(self.macros)
            self._populate()

    def _on_remove(self):
        idx = self._current_index()
        if idx is None:
            return
        del self.macros[idx]
        save_macros(self.macros)
        self._populate()

    def _on_run(self):
        idx = self._current_index()
        if idx is None:
            return
        macro = self.macros[idx]
        self.run_requested.emit(macro["commands"], macro.get("auto_execute", False))
