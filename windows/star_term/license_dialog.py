from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QDialog, QDialogButtonBox, QHBoxLayout, QLabel, QLineEdit, QPushButton,
    QVBoxLayout, QWidget,
)

from . import licensing


class LicenseStatusWidget(QWidget):
    """Shows the current trial/license status and lets the user activate a
    license key. Used in both LicenseDialog (Help menu) and
    LicenseGateDialog at startup."""

    activated = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)

        self.status_label = QLabel()
        self.status_label.setObjectName("mutedNote")
        self.status_label.setWordWrap(True)

        self.key_edit = QLineEdit()
        self.key_edit.setPlaceholderText("Paste license key")
        activate_btn = QPushButton("Activate")
        activate_btn.clicked.connect(self._on_activate)

        self.remove_btn = QPushButton("Remove License")
        self.remove_btn.clicked.connect(self._on_remove)

        self.error_label = QLabel()
        self.error_label.setObjectName("mutedNote")
        self.error_label.setWordWrap(True)

        self.key_row = QWidget()
        key_layout = QHBoxLayout(self.key_row)
        key_layout.setContentsMargins(0, 0, 0, 0)
        key_layout.addWidget(self.key_edit)
        key_layout.addWidget(activate_btn)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.status_label)
        layout.addWidget(self.key_row)
        layout.addWidget(self.remove_btn)
        layout.addWidget(self.error_label)

        self.refresh()

    def refresh(self):
        try:
            status = licensing.get_license_status()
        except Exception as exc:
            self.error_label.setText(f"Could not read license status: {exc!r}")
            return
        self.error_label.setText("")

        if status["licensed"]:
            info = status["license_info"]
            self.status_label.setText(
                f"Licensed to: {info.get('licensee')} ({info.get('email')})"
            )
            self.key_row.setVisible(False)
            self.remove_btn.setVisible(True)
        else:
            self.key_row.setVisible(True)
            self.remove_btn.setVisible(False)
            if status["trial_expired"]:
                self.status_label.setText(
                    f"Your {licensing.TRIAL_DAYS}-day trial has expired. "
                    "Enter a license key to continue."
                )
            else:
                days = status["trial_days_remaining"]
                self.status_label.setText(
                    f"Trial: {days} day{'s' if days != 1 else ''} remaining."
                )

    def _on_activate(self):
        key_str = self.key_edit.text().strip()
        if not key_str:
            self.error_label.setText("Paste a license key first.")
            return
        try:
            info = licensing.save_license_key(key_str)
        except Exception as exc:
            self.error_label.setText(f"Could not save license key: {exc!r}")
            return
        if info is None:
            self.error_label.setText("Invalid license key.")
            return
        self.key_edit.clear()
        self.refresh()
        self.activated.emit()

    def _on_remove(self):
        licensing.clear_license_key()
        self.refresh()


class LicenseDialog(QDialog):
    """Help menu dialog showing trial/license status and letting the user
    activate or remove a license key."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("License")

        self.status_widget = LicenseStatusWidget()

        buttons = QDialogButtonBox(QDialogButtonBox.Close)
        buttons.rejected.connect(self.reject)

        layout = QVBoxLayout(self)
        layout.addWidget(self.status_widget)
        layout.addWidget(buttons)


class LicenseGateDialog(QDialog):
    """Blocking startup dialog shown when the trial has expired and no
    valid license is present. The app exits unless the user activates a
    valid license."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Star Term - License Required")

        intro = QLabel(
            f"Your {licensing.TRIAL_DAYS}-day trial of Star Term has expired.\n"
            "Enter a license key to continue, or exit."
        )
        intro.setWordWrap(True)

        self.status_widget = LicenseStatusWidget()
        self.status_widget.activated.connect(self.accept)

        exit_btn = QPushButton("Exit")
        exit_btn.clicked.connect(self.reject)

        layout = QVBoxLayout(self)
        layout.addWidget(intro)
        layout.addWidget(self.status_widget)
        layout.addWidget(exit_btn)
