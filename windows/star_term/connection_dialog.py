from PySide6.QtWidgets import (
    QDialog, QFormLayout, QLineEdit, QPushButton, QFileDialog,
    QDialogButtonBox, QHBoxLayout, QWidget,
)


class ConnectionDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("New SSH Connection")

        self.host_edit = QLineEdit()
        self.port_edit = QLineEdit("22")
        self.username_edit = QLineEdit()
        self.password_edit = QLineEdit()
        self.password_edit.setEchoMode(QLineEdit.Password)
        self.key_path_edit = QLineEdit()
        self.key_passphrase_edit = QLineEdit()
        self.key_passphrase_edit.setEchoMode(QLineEdit.Password)

        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_key)

        key_row = QWidget()
        key_layout = QHBoxLayout(key_row)
        key_layout.setContentsMargins(0, 0, 0, 0)
        key_layout.addWidget(self.key_path_edit)
        key_layout.addWidget(browse_btn)

        form = QFormLayout(self)
        form.addRow("Host:", self.host_edit)
        form.addRow("Port:", self.port_edit)
        form.addRow("Username:", self.username_edit)
        form.addRow("Password:", self.password_edit)
        form.addRow("Private key file:", key_row)
        form.addRow("Key passphrase:", self.key_passphrase_edit)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def _browse_key(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select Private Key")
        if path:
            self.key_path_edit.setText(path)

    def get_connection_params(self):
        port_text = self.port_edit.text().strip()
        return {
            "host": self.host_edit.text().strip(),
            "port": int(port_text) if port_text else 22,
            "username": self.username_edit.text().strip(),
            "password": self.password_edit.text() or None,
            "key_path": self.key_path_edit.text().strip() or None,
            "key_passphrase": self.key_passphrase_edit.text() or None,
        }
