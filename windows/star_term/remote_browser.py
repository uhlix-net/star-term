"""SFTP-backed remote file browser: lists the remote working directory,
supports drag-and-drop upload/download, and follows the terminal's current
directory as the user `cd`s around (pyte/paramiko don't surface OSC 7 cwd
sequences, so the cwd is inferred by watching typed `cd` commands)."""

import posixpath
import stat as stat_module
import tempfile
from pathlib import Path

from PySide6.QtCore import QMimeData, QObject, QThread, QUrl, Qt, Signal
from PySide6.QtGui import QDrag
from PySide6.QtWidgets import (
    QAbstractItemView, QFileDialog, QHBoxLayout, QLabel, QLineEdit, QListWidget,
    QListWidgetItem, QMenu, QProgressBar, QPushButton, QToolButton, QVBoxLayout,
    QWidget,
)

from . import icons


class CwdTracker(QObject):
    """Infers the remote working directory by watching bytes typed into the
    terminal for `cd` commands."""

    cwd_changed = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.home = None
        self.cwd = None
        self._buffer = ""
        self._escape_state = None

    def set_home(self, home: str):
        self.home = home
        if self.cwd is None:
            self.cwd = home
            self.cwd_changed.emit(self.cwd)

    def feed_input(self, data: bytes):
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError:
            return

        for ch in text:
            if self._escape_state is not None:
                self._consume_escape(ch)
                continue

            if ch in ("\r", "\n"):
                self._process_line(self._buffer)
                self._buffer = ""
            elif ch in ("\x7f", "\x08"):  # backspace
                self._buffer = self._buffer[:-1]
            elif ch in ("\x03", "\x15"):  # Ctrl-C / Ctrl-U
                self._buffer = ""
            elif ch == "\x1b":  # start of an escape sequence (arrow keys, etc.)
                self._escape_state = "esc"
            elif ch.isprintable():
                self._buffer += ch

    def _consume_escape(self, ch):
        if self._escape_state == "esc":
            self._escape_state = "csi" if ch == "[" else None
        elif self._escape_state == "csi":
            if ch.isalpha() or ch == "~":
                self._escape_state = None

    def _process_line(self, line: str):
        line = line.strip()
        if line == "cd" or line.startswith("cd ") or line.startswith("cd\t"):
            arg = line[2:].strip()
            new_cwd = self._resolve(arg)
            if new_cwd and new_cwd != self.cwd:
                self.cwd = new_cwd
                self.cwd_changed.emit(self.cwd)

    def _resolve(self, arg: str):
        if not arg:
            return self.home
        if arg == "-":
            return None  # OLDPWD tracking not supported
        if arg == "~":
            return self.home
        if arg.startswith("~/"):
            return posixpath.normpath(posixpath.join(self.home, arg[2:])) if self.home else None
        if arg.startswith("/"):
            return posixpath.normpath(arg)
        if self.cwd:
            return posixpath.normpath(posixpath.join(self.cwd, arg))
        return None


class SFTPWorker(QThread):
    """Runs a single SFTP operation on a background thread."""

    listed = Signal(str, list)      # path, [(name, is_dir, size), ...]
    home_resolved = Signal(str)
    transferred = Signal(str, str)  # mode ("upload"/"download"), path
    progress = Signal(int, int)     # bytes transferred, total bytes
    error = Signal(str)

    def __init__(self, client, op, parent=None, **kwargs):
        super().__init__(parent)
        self.client = client
        self.op = op
        self.kwargs = kwargs

    def run(self):
        try:
            sftp = self.client.open_sftp()
            try:
                if self.op == "list":
                    path = self.kwargs["path"]
                    entries = []
                    for attr in sftp.listdir_attr(path):
                        is_dir = stat_module.S_ISDIR(attr.st_mode)
                        entries.append((attr.filename, is_dir, attr.st_size))
                    self.listed.emit(path, entries)
                elif self.op == "home":
                    self.home_resolved.emit(sftp.normalize("."))
                elif self.op == "upload":
                    sftp.put(self.kwargs["local_path"], self.kwargs["remote_path"])
                    self.transferred.emit("upload", self.kwargs["remote_path"])
                elif self.op == "download":
                    sftp.get(
                        self.kwargs["remote_path"],
                        self.kwargs["local_path"],
                        callback=lambda done, total: self.progress.emit(done, total),
                    )
                    self.transferred.emit("download", self.kwargs["local_path"])
            finally:
                sftp.close()
        except Exception as exc:
            self.error.emit(str(exc))


class RemoteFileList(QListWidget):
    """File list supporting drop-to-upload and drag-out-to-download."""

    upload_requested = Signal(list)  # local file paths

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.setDragEnabled(True)
        self.setAcceptDrops(True)
        self.setDragDropMode(QAbstractItemView.DragDrop)
        self.client = None
        self.remote_path = "/"

    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
        else:
            super().dragEnterEvent(event)

    def dragMoveEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
        else:
            super().dragMoveEvent(event)

    def dropEvent(self, event):
        if event.mimeData().hasUrls():
            paths = [u.toLocalFile() for u in event.mimeData().urls() if u.isLocalFile()]
            if paths:
                self.upload_requested.emit(paths)
            event.acceptProposedAction()
        else:
            super().dropEvent(event)

    def startDrag(self, supportedActions):
        if not self.client:
            return

        names = []
        for item in self.selectedItems():
            name, is_dir = item.data(Qt.UserRole)
            if not is_dir:
                names.append(name)
        if not names:
            return

        tmpdir = tempfile.mkdtemp(prefix="star_term_")
        urls = []
        try:
            with self.client.open_sftp() as sftp:
                for name in names:
                    remote_path = posixpath.join(self.remote_path, name)
                    local_path = str(Path(tmpdir) / name)
                    sftp.get(remote_path, local_path)
                    urls.append(QUrl.fromLocalFile(local_path))
        except Exception:
            return

        if not urls:
            return

        mime = QMimeData()
        mime.setUrls(urls)
        drag = QDrag(self)
        drag.setMimeData(mime)
        drag.exec(Qt.CopyAction)


class RemoteFileBrowser(QWidget):
    """SFTP browser for the remote session's current working directory."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.pane = None
        self.client = None
        self.current_path = None
        self._workers = []

        title = QLabel("Remote Files")
        title.setObjectName("sectionTitle")

        self.path_edit = QLineEdit()
        self.path_edit.returnPressed.connect(self._on_path_entered)

        up_btn = QToolButton()
        up_btn.setIcon(icons.up_icon())
        up_btn.setToolTip("Up one directory")
        up_btn.clicked.connect(self._go_up)

        refresh_btn = QToolButton()
        refresh_btn.setIcon(icons.refresh_icon())
        refresh_btn.setToolTip("Refresh")
        refresh_btn.clicked.connect(self.refresh)

        path_row = QWidget()
        path_layout = QHBoxLayout(path_row)
        path_layout.setContentsMargins(0, 0, 0, 0)
        path_layout.setSpacing(4)
        path_layout.addWidget(self.path_edit)
        path_layout.addWidget(up_btn)
        path_layout.addWidget(refresh_btn)

        self.list_widget = RemoteFileList()
        self.list_widget.itemDoubleClicked.connect(self._on_item_double_clicked)
        self.list_widget.upload_requested.connect(self._on_upload_requested)
        self.list_widget.setContextMenuPolicy(Qt.CustomContextMenu)
        self.list_widget.customContextMenuRequested.connect(self._on_context_menu)

        self.status_label = QLabel("")
        self.status_label.setStyleSheet("color: #8a8a8a;")

        self.follow_btn = QPushButton("Follow Current Directory")
        self.follow_btn.setCheckable(True)
        self.follow_btn.setChecked(True)

        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setVisible(False)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        layout.addWidget(title)
        layout.addWidget(path_row)
        layout.addWidget(self.list_widget)
        layout.addWidget(self.status_label)
        layout.addWidget(self.follow_btn)
        layout.addWidget(self.progress_bar)

        self.setEnabled(False)

    # ------------------------------------------------------------------
    # Pane wiring
    # ------------------------------------------------------------------

    def set_pane(self, pane):
        if self.pane is pane and self.client is not None:
            return

        if self.pane is not None and self.pane.cwd_tracker is not None:
            try:
                self.pane.cwd_tracker.cwd_changed.disconnect(self._on_cwd_changed)
            except (TypeError, RuntimeError):
                pass

        self.pane = pane
        self.client = None
        self.current_path = None
        self.list_widget.clear()
        self.list_widget.client = None
        self.path_edit.clear()
        self.status_label.setText("")

        if pane is None:
            self.setEnabled(False)
            return

        if pane.cwd_tracker is not None:
            pane.cwd_tracker.cwd_changed.connect(self._on_cwd_changed)

        if pane.session and pane.session.client:
            self.client = pane.session.client
            self.list_widget.client = self.client
            self.setEnabled(True)
            if pane.cwd_tracker is not None and pane.cwd_tracker.cwd:
                self.set_path(pane.cwd_tracker.cwd)
            else:
                self._resolve_home()
        else:
            self.setEnabled(False)

    # ------------------------------------------------------------------
    # Navigation
    # ------------------------------------------------------------------

    def _resolve_home(self):
        if not self.client:
            return
        worker = SFTPWorker(self.client, "home")
        worker.home_resolved.connect(self._on_home_resolved)
        worker.error.connect(self._on_error)
        self._run_worker(worker)

    def _on_home_resolved(self, home):
        if self.pane is not None and self.pane.cwd_tracker is not None:
            self.pane.cwd_tracker.set_home(home)
        self.set_path(home)

    def set_path(self, path):
        self.current_path = path
        self.path_edit.setText(path)
        self.refresh()

    def refresh(self):
        if not self.client or not self.current_path:
            return
        worker = SFTPWorker(self.client, "list", path=self.current_path)
        worker.listed.connect(self._on_listed)
        worker.error.connect(self._on_error)
        self._run_worker(worker)

    def _on_listed(self, path, entries):
        if path != self.current_path:
            return
        self.list_widget.clear()
        self.list_widget.remote_path = path
        entries = sorted(entries, key=lambda e: (not e[1], e[0].lower()))
        for name, is_dir, _size in entries:
            item = QListWidgetItem(name)
            item.setIcon(icons.directory_icon() if is_dir else icons.file_icon())
            item.setData(Qt.UserRole, (name, is_dir))
            self.list_widget.addItem(item)
        self.status_label.setText(f"{len(entries)} item(s)")

    def _on_error(self, message):
        self.status_label.setText(f"Error: {message}")

    def _on_item_double_clicked(self, item):
        name, is_dir = item.data(Qt.UserRole)
        if is_dir and self.current_path:
            self.set_path(posixpath.normpath(posixpath.join(self.current_path, name)))

    def _on_path_entered(self):
        path = self.path_edit.text().strip()
        if path:
            self.set_path(posixpath.normpath(path))

    def _go_up(self):
        if self.current_path and self.current_path != "/":
            self.set_path(posixpath.dirname(self.current_path) or "/")

    def _on_cwd_changed(self, path):
        if self.follow_btn.isChecked():
            self.set_path(path)

    # ------------------------------------------------------------------
    # Context menu
    # ------------------------------------------------------------------

    def _on_context_menu(self, pos):
        item = self.list_widget.itemAt(pos)

        file_names = []
        if item is not None:
            if not item.isSelected():
                for sel in self.list_widget.selectedItems():
                    sel.setSelected(False)
                item.setSelected(True)
                self.list_widget.setCurrentItem(item)
            for sel in self.list_widget.selectedItems():
                name, is_dir = sel.data(Qt.UserRole)
                if not is_dir:
                    file_names.append(name)

        menu = QMenu(self)
        download_action = None
        if file_names:
            download_action = menu.addAction("Download...")
        upload_action = menu.addAction("Upload...")
        action = menu.exec(self.list_widget.mapToGlobal(pos))
        if download_action is not None and action == download_action:
            self._on_download_dialog(file_names)
        elif action == upload_action:
            self._on_upload_dialog()

    def _on_upload_dialog(self):
        if not self.client or not self.current_path:
            return
        paths, _ = QFileDialog.getOpenFileNames(self, "Upload Files")
        if paths:
            self._on_upload_requested(paths)

    def _on_download_dialog(self, names):
        if not self.client or not self.current_path:
            return
        if len(names) == 1:
            local_path, _ = QFileDialog.getSaveFileName(self, "Download File", names[0])
            if not local_path:
                return
            self._on_download_requested([(names[0], local_path)])
        else:
            directory = QFileDialog.getExistingDirectory(self, "Download Files To")
            if not directory:
                return
            self._on_download_requested([(name, str(Path(directory) / name)) for name in names])

    # ------------------------------------------------------------------
    # Transfers
    # ------------------------------------------------------------------

    def _on_upload_requested(self, local_paths):
        if not self.client or not self.current_path:
            return
        for local_path in local_paths:
            remote_path = posixpath.join(self.current_path, Path(local_path).name)
            worker = SFTPWorker(
                self.client, "upload", local_path=local_path, remote_path=remote_path
            )
            worker.transferred.connect(lambda *_args: self.refresh())
            worker.error.connect(self._on_error)
            self._run_worker(worker)

    def _on_download_requested(self, name_local_pairs):
        if not self.client or not self.current_path:
            return
        self._download_queue = list(name_local_pairs)
        self._download_total = len(self._download_queue)
        self._download_index = 0
        self._start_next_download()

    def _start_next_download(self):
        if not self._download_queue:
            self.progress_bar.setVisible(False)
            self.refresh()
            return

        name, local_path = self._download_queue.pop(0)
        self._download_index += 1
        remote_path = posixpath.join(self.current_path, name)

        self.progress_bar.setValue(0)
        self.progress_bar.setVisible(True)
        if self._download_total > 1:
            self.status_label.setText(
                f"Downloading {name} ({self._download_index}/{self._download_total})..."
            )
        else:
            self.status_label.setText(f"Downloading {name}...")

        worker = SFTPWorker(
            self.client, "download", remote_path=remote_path, local_path=local_path
        )
        worker.progress.connect(self._on_download_progress)
        worker.transferred.connect(self._on_download_finished)
        worker.error.connect(self._on_download_error)
        self._run_worker(worker)

    def _on_download_progress(self, transferred, total):
        if total:
            self.progress_bar.setValue(int(transferred * 100 / total))

    def _on_download_finished(self, _mode, _local_path):
        self._start_next_download()

    def _on_download_error(self, message):
        self._download_queue = []
        self.progress_bar.setVisible(False)
        self._on_error(message)

    # ------------------------------------------------------------------
    # Worker lifecycle
    # ------------------------------------------------------------------

    def _run_worker(self, worker):
        self._workers.append(worker)

        def _cleanup():
            if worker in self._workers:
                self._workers.remove(worker)
            worker.deleteLater()

        worker.finished.connect(_cleanup)
        worker.start()
