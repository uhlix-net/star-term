from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QAbstractItemView, QCheckBox, QComboBox, QDialog, QDialogButtonBox,
    QFileDialog, QFormLayout, QHBoxLayout, QInputDialog, QLineEdit, QLabel,
    QMenu, QPushButton, QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget,
)

from . import icons
from .config import load_folders, load_sessions, save_folders, save_sessions

DEFAULT_FOLDER = "General"
FOLDER_ROLE = Qt.UserRole + 1


class SessionEditDialog(QDialog):
    def __init__(self, parent=None, session=None, folders=None):
        super().__init__(parent)
        self.setWindowTitle("Session")
        session = session or {}
        folders = folders or []

        self.name_edit = QLineEdit(session.get("name", ""))
        self.folder_edit = QComboBox()
        self.folder_edit.setEditable(True)
        self.folder_edit.addItems(folders)
        self.folder_edit.setCurrentText(session.get("folder", ""))
        self.folder_edit.lineEdit().setPlaceholderText(DEFAULT_FOLDER)
        self.host_edit = QLineEdit(session.get("host", ""))
        self.port_edit = QLineEdit(str(session.get("port", 22)))
        self.username_edit = QLineEdit(session.get("username", ""))
        self.use_key_checkbox = QCheckBox("Use SSH key authentication")
        self.use_key_checkbox.setChecked(session.get("use_key", False))

        self.key_path_edit = QLineEdit(session.get("key_path", ""))
        self.key_path_edit.setPlaceholderText("(uses Settings key)")
        self.key_path_edit.setMinimumWidth(160)
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_key)
        clear_btn = QPushButton("Clear")
        clear_btn.clicked.connect(self.key_path_edit.clear)

        key_row = QWidget()
        key_layout = QHBoxLayout(key_row)
        key_layout.setContentsMargins(0, 0, 0, 0)
        key_layout.addWidget(self.key_path_edit)
        key_layout.addWidget(browse_btn)
        key_layout.addWidget(clear_btn)

        form = QFormLayout(self)
        form.addRow("Name:", self.name_edit)
        form.addRow("Folder:", self.folder_edit)
        form.addRow("Host:", self.host_edit)
        form.addRow("Port:", self.port_edit)
        form.addRow("Username:", self.username_edit)
        form.addRow(self.use_key_checkbox)
        form.addRow("SSH key override:", key_row)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def _browse_key(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select Private Key")
        if path:
            self.key_path_edit.setText(path)

    def get_session(self) -> dict:
        port_text = self.port_edit.text().strip()
        host = self.host_edit.text().strip()
        return {
            "name": self.name_edit.text().strip() or host,
            "folder": self.folder_edit.currentText().strip(),
            "host": host,
            "port": int(port_text) if port_text else 22,
            "username": self.username_edit.text().strip(),
            "use_key": self.use_key_checkbox.isChecked(),
            "key_path": self.key_path_edit.text().strip(),
        }


class SessionTreeWidget(QTreeWidget):
    """A QTreeWidget that lets session items be dragged onto a different
    folder (header or session) to move them there."""

    session_dropped = Signal(int, str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setDragDropMode(QAbstractItemView.InternalMove)
        self.setDragEnabled(True)

    def dropEvent(self, event):
        source_item = self.currentItem()
        if source_item is None or source_item.data(0, Qt.UserRole) is None:
            event.ignore()
            return

        target_item = self.itemAt(event.position().toPoint())
        if target_item is None:
            event.ignore()
            return

        if target_item.data(0, Qt.UserRole) is not None:
            target_folder = target_item.parent().data(0, FOLDER_ROLE)
        else:
            target_folder = target_item.data(0, FOLDER_ROLE)

        if target_folder is None:
            event.ignore()
            return

        event.accept()
        self.session_dropped.emit(source_item.data(0, Qt.UserRole), target_folder)


class SessionSidebar(QWidget):
    connect_requested = Signal(dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.sessions = load_sessions()
        self.folders = load_folders()

        self.list_widget = SessionTreeWidget()
        self.list_widget.setHeaderHidden(True)
        self.list_widget.setIndentation(12)
        self.list_widget.itemDoubleClicked.connect(self._on_connect)
        self.list_widget.setContextMenuPolicy(Qt.CustomContextMenu)
        self.list_widget.customContextMenuRequested.connect(self._on_context_menu)
        self.list_widget.session_dropped.connect(self._on_session_dropped)

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

        connect_btn = QPushButton("Connect")
        connect_btn.clicked.connect(self._on_connect)

        title = QLabel("Sessions")
        title.setObjectName("sectionTitle")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        layout.addWidget(title)
        layout.addWidget(self.list_widget)
        layout.addWidget(edit_row)
        layout.addWidget(connect_btn)

        self._populate()

    def _all_folder_names(self):
        names = set(self.folders)
        for session in self.sessions:
            names.add(session.get("folder") or DEFAULT_FOLDER)
        names.add(DEFAULT_FOLDER)
        return sorted(names, key=str.lower)

    def _folder_session_count(self, folder_name):
        return sum(
            1 for s in self.sessions if (s.get("folder") or DEFAULT_FOLDER) == folder_name
        )

    def _populate(self):
        self.list_widget.clear()

        folders = {}
        for idx, session in enumerate(self.sessions):
            folder = session.get("folder") or DEFAULT_FOLDER
            folders.setdefault(folder, []).append(idx)

        all_folders = set(folders) | set(self.folders) | {DEFAULT_FOLDER}

        for folder in sorted(all_folders, key=str.lower):
            header = QTreeWidgetItem([folder])
            header.setFlags(Qt.ItemIsEnabled | Qt.ItemIsDropEnabled)
            font = header.font(0)
            font.setBold(True)
            header.setFont(0, font)
            header.setIcon(0, icons.folder_icon())
            header.setData(0, FOLDER_ROLE, folder)
            self.list_widget.addTopLevelItem(header)

            for idx in sorted(folders.get(folder, []), key=lambda i: self.sessions[i]["name"].lower()):
                item = QTreeWidgetItem([self.sessions[idx]["name"]])
                item.setData(0, Qt.UserRole, idx)
                header.addChild(item)

        self.list_widget.expandAll()

    def _current_index(self):
        item = self.list_widget.currentItem()
        if item is None:
            return None
        return item.data(0, Qt.UserRole)

    def _on_add(self, folder=None):
        dialog = SessionEditDialog(self, folders=self._all_folder_names())
        if folder:
            dialog.folder_edit.setCurrentText(folder)
        if dialog.exec():
            self.sessions.append(dialog.get_session())
            save_sessions(self.sessions)
            self._populate()

    def _on_edit(self):
        idx = self._current_index()
        if idx is None:
            return
        dialog = SessionEditDialog(self, self.sessions[idx], folders=self._all_folder_names())
        if dialog.exec():
            self.sessions[idx] = dialog.get_session()
            save_sessions(self.sessions)
            self._populate()

    def _on_remove(self):
        idx = self._current_index()
        if idx is None:
            return
        del self.sessions[idx]
        save_sessions(self.sessions)
        self._populate()

    def _on_context_menu(self, pos):
        item = self.list_widget.itemAt(pos)

        if item is not None and item.data(0, Qt.UserRole) is not None:
            self.list_widget.setCurrentItem(item)
            idx = item.data(0, Qt.UserRole)
            folder = self.sessions[idx].get("folder") or DEFAULT_FOLDER

            menu = QMenu(self)
            new_session_action = menu.addAction("New Session...")
            edit_action = menu.addAction("Edit")
            remove_action = menu.addAction("Remove")
            action = menu.exec(self.list_widget.mapToGlobal(pos))

            if action == new_session_action:
                self._on_add(folder)
            elif action == edit_action:
                self._on_edit()
            elif action == remove_action:
                self._on_remove()
            return

        folder_name = item.data(0, FOLDER_ROLE) if item is not None else None

        menu = QMenu(self)
        new_session_action = menu.addAction("New Session...")
        new_folder_action = menu.addAction("New Folder...")
        rename_folder_action = None
        if folder_name:
            rename_folder_action = menu.addAction("Rename Folder")
        remove_folder_action = None
        if folder_name and folder_name in self.folders and self._folder_session_count(folder_name) == 0:
            remove_folder_action = menu.addAction("Remove Folder")

        action = menu.exec(self.list_widget.mapToGlobal(pos))

        if action == new_session_action:
            self._on_add(folder_name)
        elif action == new_folder_action:
            self._on_new_folder()
        elif rename_folder_action is not None and action == rename_folder_action:
            self._on_rename_folder(folder_name)
        elif remove_folder_action is not None and action == remove_folder_action:
            self._on_remove_folder(folder_name)

    def _on_new_folder(self):
        name, ok = QInputDialog.getText(self, "New Folder", "Folder name:")
        name = name.strip()
        if not ok or not name:
            return
        if name not in self.folders:
            self.folders.append(name)
            save_folders(self.folders)
        self._populate()

    def _on_rename_folder(self, folder_name):
        new_name, ok = QInputDialog.getText(self, "Rename Folder", "Folder name:", text=folder_name)
        new_name = new_name.strip()
        if not ok or not new_name or new_name == folder_name:
            return

        for session in self.sessions:
            if (session.get("folder") or DEFAULT_FOLDER) == folder_name:
                session["folder"] = "" if new_name == DEFAULT_FOLDER else new_name
        save_sessions(self.sessions)

        if folder_name in self.folders:
            self.folders.remove(folder_name)
        if new_name != DEFAULT_FOLDER and new_name not in self.folders:
            self.folders.append(new_name)
        save_folders(self.folders)

        self._populate()

    def _on_remove_folder(self, folder_name):
        if folder_name in self.folders:
            self.folders.remove(folder_name)
            save_folders(self.folders)
        self._populate()

    def _on_session_dropped(self, session_idx, target_folder):
        current_folder = self.sessions[session_idx].get("folder") or DEFAULT_FOLDER
        if target_folder == current_folder:
            return
        self.sessions[session_idx]["folder"] = "" if target_folder == DEFAULT_FOLDER else target_folder
        save_sessions(self.sessions)
        self._populate()

    def _on_connect(self):
        idx = self._current_index()
        if idx is None:
            return
        self.connect_requested.emit(self.sessions[idx])

    def reload(self):
        self.sessions = load_sessions()
        self.folders = load_folders()
        self._populate()
