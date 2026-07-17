import sys

from PySide6.QtWidgets import (
    QApplication, QFileDialog, QFormLayout, QHBoxLayout, QLabel, QLineEdit,
    QPushButton, QVBoxLayout, QWidget,
)

from . import icons
from .config import load_settings, save_settings
from .crypto import build_license_key


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Star Term License Manager")
        self.setWindowIcon(icons.app_icon())
        self.settings = load_settings()

        self.licensee_edit = QLineEdit()
        self.email_edit = QLineEdit()

        self.key_path_edit = QLineEdit(self.settings.get("private_key_path", ""))
        self.key_path_edit.setReadOnly(True)
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_key)
        key_row = QWidget()
        key_layout = QHBoxLayout(key_row)
        key_layout.setContentsMargins(0, 0, 0, 0)
        key_layout.addWidget(self.key_path_edit)
        key_layout.addWidget(browse_btn)

        generate_btn = QPushButton("Generate License Key")
        generate_btn.clicked.connect(self._generate)

        self.status_label = QLabel()
        self.status_label.setWordWrap(True)

        self.result_edit = QLineEdit()
        self.result_edit.setReadOnly(True)

        copy_btn = QPushButton("Copy to Clipboard")
        copy_btn.clicked.connect(self._copy_result)
        result_row = QWidget()
        result_layout = QHBoxLayout(result_row)
        result_layout.setContentsMargins(0, 0, 0, 0)
        result_layout.addWidget(self.result_edit)
        result_layout.addWidget(copy_btn)

        form = QFormLayout()
        form.addRow("Licensee name:", self.licensee_edit)
        form.addRow("Email:", self.email_edit)
        form.addRow("Private key:", key_row)

        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(generate_btn)
        layout.addWidget(self.status_label)
        layout.addWidget(QLabel("License key:"))
        layout.addWidget(result_row)

    def _browse_key(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select Private Key", "", "PEM Files (*.pem)")
        if not path:
            return
        self.settings["private_key_path"] = path
        save_settings(self.settings)
        self.key_path_edit.setText(path)

    def _generate(self):
        licensee = self.licensee_edit.text().strip()
        email = self.email_edit.text().strip()
        key_path = self.key_path_edit.text().strip()

        if not licensee or not email or not key_path:
            self.status_label.setText("Licensee name, email, and private key are all required.")
            self.result_edit.setText("")
            return

        try:
            key = build_license_key(licensee, email, key_path)
        except Exception as exc:
            self.status_label.setText(f"Could not generate license key: {exc}")
            self.result_edit.setText("")
            return

        self.status_label.setText("")
        self.result_edit.setText(key)

    def _copy_result(self):
        if self.result_edit.text():
            QApplication.clipboard().setText(self.result_edit.text())


def main():
    app = QApplication(sys.argv)
    app.setWindowIcon(icons.app_icon())
    window = MainWindow()
    window.resize(500, 220)
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
