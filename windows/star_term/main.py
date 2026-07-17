import json
import math
import sys
from pathlib import Path

import paramiko
from PySide6.QtCore import QSize, Qt
from PySide6.QtGui import QAction, QActionGroup, QPixmap
from PySide6.QtWidgets import (
    QApplication, QDialog, QDialogButtonBox, QFileDialog, QGridLayout,
    QInputDialog, QLabel, QLineEdit, QMainWindow, QMessageBox, QScrollArea,
    QSplitter, QStackedWidget, QTabWidget, QToolBar, QVBoxLayout, QWidget,
)

from . import __version__, debug, icons, licensing, theme
from .config import load_sessions, load_settings, save_sessions, save_settings
from .connection_dialog import ConnectionDialog
from .license_dialog import LicenseDialog, LicenseGateDialog
from .macros_panel import MacrosPanel
from .preferences_dialog import PreferencesDialog
from .remote_browser import RemoteFileBrowser
from .session_pane import SessionPane
from .sidebar import SessionSidebar
from .ssh_session import SSHSession, load_private_key
from .status_bar import SystemStatusBar

UPDATE_HISTORY = """Version 0.2.0

- Settings toolbar button (gear icon) opens a tabbed Preferences dialog
  with General, Terminal, and SSH Key tabs, replacing the old Settings menu
- General tab lets you switch the application's appearance between Dark
  and Light themes, applied immediately and saved across restarts
- Refreshed, professional Dark/Light UI across menus, toolbars, dialogs,
  and panels
- SSH key is now referenced by path instead of copied, with a per-session
  override on each session's Edit dialog
- Help menu with About and Update History dialogs
- Scrollback buffer preserves your scroll position when new output arrives
  while you're scrolled back
- Tab key is sent to the shell for completion instead of moving keyboard
  focus to another control; clicking the terminal reliably gives it focus
- Installer window now appears in front of other open windows, and the
  Start Menu/Desktop shortcuts use the application icon

Version 0.1.0 - Initial Release

- SSH terminal client with VT100 emulation (pyte) and a PySide6 GUI
- Connection dialog with password or private-key authentication and
  host key fingerprint verification
- Tabbed multi-session support with a Multi-Exec grid view and input
  broadcast across sessions
- Configurable terminal font and cursor style, with a 2000-line
  scrollback buffer (mouse wheel, scrollbar, or Shift+PageUp/PageDown)
- Session sidebar with folder grouping, persistent storage, and
  right-click Edit/Remove/New Folder
- SFTP remote file browser with drag-and-drop upload/download,
  right-click upload, and current-directory following
- Reconnect support for dropped SSH sessions
- System status bar showing live CPU, load average, RAM, and swap for
  the connected host
- Session import/export, and SSH key and terminal appearance settings
- NSIS Windows installer with a splash screen, Python bootstrap, and
  Apps & Features registration
"""


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Star Term")
        self.setWindowIcon(icons.app_icon())

        self.panes = []

        self.sidebar = SessionSidebar()

        self.tabs = QTabWidget()
        self.tabs.setTabsClosable(True)
        self.tabs.tabCloseRequested.connect(self._on_tab_close_requested)
        self.tabs.currentChanged.connect(self._on_tab_changed)

        self.multiexec_container = QWidget()
        self.multiexec_layout = QGridLayout(self.multiexec_container)
        self.multiexec_layout.setContentsMargins(4, 4, 4, 4)
        self.multiexec_layout.setSpacing(4)

        self.view_stack = QStackedWidget()
        self.view_stack.addWidget(self.tabs)
        self.view_stack.addWidget(self.multiexec_container)

        self.remote_browser = RemoteFileBrowser()
        self.macros_panel = MacrosPanel()
        self.macros_panel.run_requested.connect(self._run_macro)

        self.left_stack = QStackedWidget()
        self.left_stack.addWidget(self.sidebar)
        self.left_stack.addWidget(self.remote_browser)
        self.left_stack.addWidget(self.macros_panel)

        self.splitter = QSplitter()
        self.splitter.setHandleWidth(6)
        self.splitter.setContentsMargins(4, 4, 4, 4)
        self.splitter.addWidget(self.left_stack)
        self.splitter.addWidget(self.view_stack)
        self.splitter.setStretchFactor(0, 0)
        self.splitter.setStretchFactor(1, 1)
        self.setCentralWidget(self.splitter)

        self.setStatusBar(SystemStatusBar(self))

        self.connect_action = QAction("Connect...", self)
        self.connect_action.setIcon(icons.connect_icon())
        self.connect_action.triggered.connect(self.open_connection_dialog)

        self.close_session_action = QAction("Close Session", self)
        self.close_session_action.setIcon(icons.disconnect_icon())
        self.close_session_action.triggered.connect(self.disconnect_session)

        export_action = QAction("Export Sessions...", self)
        export_action.triggered.connect(self.export_sessions)

        import_action = QAction("Import Sessions...", self)
        import_action.triggered.connect(self.import_sessions)

        exit_action = QAction("Exit", self)
        exit_action.triggered.connect(self.close)

        file_menu = self.menuBar().addMenu("File")
        file_menu.addAction(self.connect_action)
        file_menu.addAction(self.close_session_action)
        file_menu.addSeparator()
        file_menu.addAction(export_action)
        file_menu.addAction(import_action)
        file_menu.addSeparator()
        file_menu.addAction(exit_action)

        self.multi_exec_action = QAction("Multi-Exec View", self)
        self.multi_exec_action.setIcon(icons.multi_exec_icon())
        self.multi_exec_action.setCheckable(True)
        self.multi_exec_action.toggled.connect(self.toggle_multi_exec_view)

        view_menu = self.menuBar().addMenu("View")
        view_menu.addAction(self.multi_exec_action)

        self.preferences_action = QAction("Preferences...", self)
        self.preferences_action.setIcon(icons.settings_icon())
        self.preferences_action.setToolTip("Settings")
        self.preferences_action.triggered.connect(self.open_preferences_dialog)

        settings_menu = self.menuBar().addMenu("Settings")
        settings_menu.addAction(self.preferences_action)

        about_action = QAction("About Star Term", self)
        about_action.triggered.connect(self.show_about_dialog)

        update_history_action = QAction("Update History", self)
        update_history_action.triggered.connect(self.show_update_history_dialog)

        license_action = QAction("License...", self)
        license_action.triggered.connect(self.show_license_dialog)

        help_menu = self.menuBar().addMenu("Help")
        help_menu.addAction(about_action)
        help_menu.addAction(update_history_action)
        help_menu.addSeparator()
        help_menu.addAction(license_action)

        toolbar = QToolBar("Main Toolbar", self)
        toolbar.setMovable(False)
        toolbar.setIconSize(QSize(24, 24))
        toolbar.addAction(self.connect_action)
        toolbar.addAction(self.close_session_action)
        toolbar.addSeparator()
        toolbar.addAction(self.multi_exec_action)
        toolbar.addSeparator()
        toolbar.addAction(self.preferences_action)
        self.addToolBar(toolbar)

        self.sessions_action = QAction("Sessions", self)
        self.sessions_action.setIcon(icons.sessions_icon())
        self.sessions_action.setCheckable(True)
        self.sessions_action.setChecked(True)
        self.sessions_action.toggled.connect(
            lambda checked: checked and self.left_stack.setCurrentWidget(self.sidebar)
        )

        self.remote_files_action = QAction("Remote Files", self)
        self.remote_files_action.setIcon(icons.directory_icon())
        self.remote_files_action.setCheckable(True)
        self.remote_files_action.toggled.connect(
            lambda checked: checked and self.left_stack.setCurrentWidget(self.remote_browser)
        )

        self.macros_action = QAction("Macros", self)
        self.macros_action.setIcon(icons.macros_icon())
        self.macros_action.setCheckable(True)
        self.macros_action.toggled.connect(
            lambda checked: checked and self.left_stack.setCurrentWidget(self.macros_panel)
        )

        left_panel_group = QActionGroup(self)
        left_panel_group.setExclusive(True)
        left_panel_group.addAction(self.sessions_action)
        left_panel_group.addAction(self.remote_files_action)
        left_panel_group.addAction(self.macros_action)

        activity_bar = QToolBar("Activity Bar", self)
        activity_bar.setMovable(False)
        activity_bar.setOrientation(Qt.Vertical)
        activity_bar.setIconSize(QSize(24, 24))
        activity_bar.addAction(self.sessions_action)
        activity_bar.addAction(self.remote_files_action)
        activity_bar.addAction(self.macros_action)
        self.addToolBar(Qt.LeftToolBarArea, activity_bar)

        self.sidebar.connect_requested.connect(self.connect_saved_session)

    # ------------------------------------------------------------------
    # Pane / view management
    # ------------------------------------------------------------------

    def _add_pane(self, pane: SessionPane):
        self.panes.append(pane)
        pane.data_to_send.connect(lambda data, p=pane: self._on_data_to_send(p, data))
        pane.size_changed.connect(lambda cols, rows, p=pane: self._on_size_changed(p, cols, rows))
        pane.close_requested.connect(lambda p=pane: self._close_pane(p))
        pane.stats_updated.connect(lambda stats, p=pane: self._on_stats_updated(p, stats))

        if self.multi_exec_action.isChecked():
            self._populate_multi_exec_grid()
        else:
            self.tabs.addTab(pane, pane.name)
            self.tabs.setCurrentWidget(pane)

    def _close_pane(self, pane: SessionPane):
        pane.disconnect_session()
        if pane in self.panes:
            self.panes.remove(pane)
        if self.remote_browser.pane is pane:
            self.remote_browser.set_pane(None)

        if self.multi_exec_action.isChecked():
            self._populate_multi_exec_grid()
        else:
            index = self.tabs.indexOf(pane)
            if index >= 0:
                self.tabs.removeTab(index)

        pane.deleteLater()

    def _on_tab_close_requested(self, index):
        pane = self.tabs.widget(index)
        if pane:
            self._close_pane(pane)

    def toggle_multi_exec_view(self, checked):
        if checked:
            self._populate_multi_exec_grid()
            self.view_stack.setCurrentWidget(self.multiexec_container)
            self._resize_for_multi_exec()
        else:
            self._populate_tabs()
            self.view_stack.setCurrentWidget(self.tabs)
        self.statusBar().setVisible(not checked)

    def _resize_for_multi_exec(self):
        """Grow the window (up to the available screen size) so each pane in
        the Multi-Exec grid has room to show a full terminal."""
        if not self.panes:
            return

        columns = max(1, math.ceil(math.sqrt(len(self.panes))))
        rows = math.ceil(len(self.panes) / columns)

        pane_hint = self.panes[0].terminal.sizeHint()
        desired_width = columns * pane_hint.width() + self.sidebar.width() + 40
        desired_height = rows * pane_hint.height() + 80

        available = self.screen().availableGeometry()
        width = min(desired_width, available.width())
        height = min(desired_height, available.height())

        if width > self.width() or height > self.height():
            self.resize(max(self.width(), width), max(self.height(), height))

    def _populate_multi_exec_grid(self):
        while self.multiexec_layout.count():
            self.multiexec_layout.takeAt(0)
        while self.tabs.count():
            self.tabs.removeTab(0)

        # Reset any stretch factors left over from a previous (larger) grid.
        for i in range(self.multiexec_layout.rowCount()):
            self.multiexec_layout.setRowStretch(i, 0)
        for i in range(self.multiexec_layout.columnCount()):
            self.multiexec_layout.setColumnStretch(i, 0)

        if not self.panes:
            return

        columns = max(1, math.ceil(math.sqrt(len(self.panes))))
        rows = math.ceil(len(self.panes) / columns)

        for i, pane in enumerate(self.panes):
            row, col = divmod(i, columns)
            self.multiexec_layout.addWidget(pane, row, col)
            pane.show()

        for col in range(columns):
            self.multiexec_layout.setColumnStretch(col, 1)
        for row in range(rows):
            self.multiexec_layout.setRowStretch(row, 1)

    def _populate_tabs(self):
        while self.multiexec_layout.count():
            self.multiexec_layout.takeAt(0)
        for pane in self.panes:
            self.tabs.addTab(pane, pane.name)

    def _on_data_to_send(self, pane: SessionPane, data: bytes):
        if self.multi_exec_action.isChecked() and not pane.exclude_checkbox.isChecked():
            for p in self.panes:
                if p.session and not p.exclude_checkbox.isChecked():
                    p.session.send(data)
        elif pane.session:
            pane.session.send(data)

    def _run_macro(self, commands: str, auto_execute: bool):
        lines = commands.splitlines()
        if not lines:
            return

        if auto_execute:
            text = "".join(line + "\n" for line in lines)
        else:
            text = "\n".join(lines[:-1])
            if text:
                text += "\n"
            text += lines[-1]
        data = text.encode()

        if self.multi_exec_action.isChecked():
            for p in self.panes:
                if p.session and not p.exclude_checkbox.isChecked():
                    p.session.send(data)
        else:
            pane = self.tabs.currentWidget()
            if pane and pane.session:
                pane.session.send(data)

    def _on_size_changed(self, pane: SessionPane, cols, rows):
        if pane.session:
            pane.session.resize(cols, rows)

    def _on_tab_changed(self, index):
        pane = self.tabs.widget(index)
        self.remote_browser.set_pane(pane)
        if self.multi_exec_action.isChecked():
            return
        self.statusBar().update_stats(pane.last_stats if pane else None)

    def _on_stats_updated(self, pane: SessionPane, stats: dict):
        if self.multi_exec_action.isChecked():
            return
        if pane is self.tabs.currentWidget():
            self.statusBar().update_stats(stats)

    # ------------------------------------------------------------------
    # Menu actions
    # ------------------------------------------------------------------

    def open_connection_dialog(self):
        dialog = ConnectionDialog(self)
        if dialog.exec():
            params = dialog.get_connection_params()
            self.connect_session(params, name=f"{params['username']}@{params['host']}")

    def open_preferences_dialog(self):
        settings = load_settings()
        dialog = PreferencesDialog(
            self,
            font_family=settings.get("font_family", "Courier New"),
            font_size=settings.get("font_size", 10),
            cursor_style=settings.get("cursor_style", "underline"),
            theme_name=settings.get("theme", "dark"),
        )
        if dialog.exec():
            new_settings = dialog.get_terminal_settings()
            for pane in self.panes:
                pane.apply_settings(**new_settings)
            new_settings.update(dialog.get_general_settings())
            settings = load_settings()
            settings.update(new_settings)
            save_settings(settings)
            QApplication.instance().setStyleSheet(theme.get_stylesheet(settings.get("theme", "dark")))

    def show_about_dialog(self):
        dialog = QDialog(self)
        dialog.setWindowTitle("About Star Term")

        image_label = QLabel()
        pixmap = QPixmap(str(Path(__file__).parent / "splash.bmp"))
        if not pixmap.isNull():
            image_label.setPixmap(pixmap)
        image_label.setAlignment(Qt.AlignCenter)

        version_label = QLabel(f"Star Term v{__version__}")
        version_label.setAlignment(Qt.AlignCenter)
        font = version_label.font()
        font.setBold(True)
        font.setPointSize(font.pointSize() + 2)
        version_label.setFont(font)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok)
        buttons.accepted.connect(dialog.accept)

        layout = QVBoxLayout(dialog)
        layout.addWidget(image_label)
        layout.addWidget(version_label)
        layout.addWidget(buttons)

        dialog.exec()

    def show_license_dialog(self):
        dialog = LicenseDialog(self)
        dialog.exec()

    def show_update_history_dialog(self):
        dialog = QDialog(self)
        dialog.setWindowTitle("Update History")

        text_label = QLabel(UPDATE_HISTORY)
        text_label.setWordWrap(True)
        text_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
        text_label.setAlignment(Qt.AlignTop | Qt.AlignLeft)
        text_label.setContentsMargins(8, 8, 8, 8)

        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setWidget(text_label)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok)
        buttons.accepted.connect(dialog.accept)

        layout = QVBoxLayout(dialog)
        layout.addWidget(scroll_area)
        layout.addWidget(buttons)

        dialog.resize(500, 400)
        dialog.exec()

    def export_sessions(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Export Sessions", "sessions.json", "JSON Files (*.json)"
        )
        if not path:
            return
        try:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(load_sessions(), f, indent=2)
        except OSError as exc:
            QMessageBox.critical(self, "Export Failed", str(exc))

    def import_sessions(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Import Sessions", "", "JSON Files (*.json)"
        )
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8") as f:
                sessions = json.load(f)
        except (OSError, ValueError) as exc:
            QMessageBox.critical(self, "Import Failed", str(exc))
            return

        if QMessageBox.question(
            self, "Import Sessions",
            "This will replace your current saved sessions. Continue?",
        ) != QMessageBox.Yes:
            return

        save_sessions(sessions)
        self.sidebar.reload()

    # ------------------------------------------------------------------
    # Session connection
    # ------------------------------------------------------------------

    def connect_saved_session(self, session: dict):
        key_path = None
        key_passphrase = None
        password = None

        if session.get("use_key"):
            key_path = session.get("key_path") or load_settings().get("ssh_key_path")
            if not key_path:
                QMessageBox.warning(
                    self, "No SSH Key",
                    "No SSH key is configured. Set one in Settings > SSH Key... "
                    "or override it on this session's profile.",
                )
                return
            key_passphrase = self._prompt_key_passphrase(key_path)
            if key_passphrase is False:
                return
        else:
            password, ok = QInputDialog.getText(
                self, "Password",
                f"Password for {session['username']}@{session['host']}:",
                QLineEdit.Password,
            )
            if not ok:
                return

        self.connect_session({
            "host": session["host"],
            "port": session["port"],
            "username": session["username"],
            "password": password,
            "key_path": key_path,
            "key_passphrase": key_passphrase,
        }, name=session["name"])

    def _prompt_key_passphrase(self, key_path: str):
        """Return a passphrase if the key needs one, None if it doesn't, or
        False if the user cancelled the prompt."""
        try:
            load_private_key(key_path, None)
            return None
        except paramiko.PasswordRequiredException:
            pass
        except Exception:
            return None

        passphrase, ok = QInputDialog.getText(
            self, "Key Passphrase",
            f"Passphrase for {Path(key_path).name}:",
            QLineEdit.Password,
        )
        return passphrase if ok else False

    def connect_session(self, params, name=None):
        settings = load_settings()
        pane = SessionPane(
            name=name or f"{params['username']}@{params['host']}",
            font_family=settings.get("font_family", "Courier New"),
            font_size=settings.get("font_size", 10),
            cursor_style=settings.get("cursor_style", "underline"),
        )
        self._add_pane(pane)
        pane.reconnect_requested.connect(lambda p=pane: self._reconnect_pane(p))
        self._start_session(pane, params)

    def _start_session(self, pane: SessionPane, params: dict):
        pane.connection_params = params
        pane.reconnect_btn.setVisible(False)

        session = SSHSession(
            host=params["host"],
            port=params["port"],
            username=params["username"],
            password=params["password"],
            key_path=params["key_path"],
            key_passphrase=params["key_passphrase"],
            cols=pane.terminal.screen.columns,
            rows=pane.terminal.screen.lines,
        )
        pane.session = session
        session.data_received.connect(pane.terminal.feed)
        session.connection_error.connect(self.show_error)
        session.connection_error.connect(lambda msg, p=pane: self._on_session_ended(p))
        session.connection_closed.connect(lambda p=pane: self._on_session_ended(p))
        session.connected.connect(pane.start_stats_worker)
        session.connected.connect(lambda p=pane: self._on_session_connected(p))
        session.host_key_unknown.connect(self._on_host_key_unknown)
        session.start()

    def _on_session_connected(self, pane: SessionPane):
        if pane is self.tabs.currentWidget():
            self.remote_browser.set_pane(pane)

    def _on_session_ended(self, pane: SessionPane):
        pane._stop_stats_worker()
        if pane in self.panes:
            pane.reconnect_btn.setVisible(True)

    def _reconnect_pane(self, pane: SessionPane):
        if not pane.connection_params:
            return
        if pane.session:
            pane.session.stop()
            pane.session.wait()
            pane.session = None
        self._start_session(pane, pane.connection_params)

    def _on_host_key_unknown(self, hostname, key_type, fingerprint, event, result):
        answer = QMessageBox.question(
            self, "Unknown Host Key",
            f"The authenticity of host '{hostname}' can't be established.\n"
            f"{key_type} key fingerprint:\n{fingerprint}\n\n"
            "Are you sure you want to continue connecting?",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No,
        )
        result["accepted"] = answer == QMessageBox.Yes
        event.set()

    def disconnect_session(self):
        if self.multi_exec_action.isChecked():
            return
        pane = self.tabs.currentWidget()
        if pane:
            self._close_pane(pane)

    def show_error(self, message: str):
        QMessageBox.critical(self, "Connection Error", message)

    def closeEvent(self, event):
        for pane in list(self.panes):
            pane.disconnect_session()
        super().closeEvent(event)


def _excepthook(exc_type, exc_value, exc_tb):
    import traceback
    debug.log("Unhandled exception:\n" + "".join(
        traceback.format_exception(exc_type, exc_value, exc_tb)
    ))
    sys.__excepthook__(exc_type, exc_value, exc_tb)


def main():
    sys.excepthook = _excepthook

    if sys.platform == "win32":
        # Give the process its own Application User Model ID so Windows
        # shows our icon (not pythonw.exe's) on the taskbar and groups
        # the window separately from other Python processes.
        import ctypes
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("uhlix.net.StarTerm")

    app = QApplication(sys.argv)
    app.setStyleSheet(theme.get_stylesheet(load_settings().get("theme", "dark")))
    app.setWindowIcon(icons.app_icon())

    status = licensing.get_license_status()
    if not status["licensed"] and status["trial_expired"]:
        gate = LicenseGateDialog()
        if gate.exec() != QDialog.Accepted:
            sys.exit(0)

    window = MainWindow()
    window.resize(1200, 800)
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
