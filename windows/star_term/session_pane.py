from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QCheckBox, QHBoxLayout, QLabel, QPushButton, QVBoxLayout, QWidget,
)

from .remote_browser import CwdTracker
from .ssh_session import RemoteStatsWorker
from .terminal_widget import TerminalWidget


class SessionPane(QWidget):
    """A single SSH session: terminal, exclude-from-multi-exec checkbox, and
    close button. Owns the SSHSession once connected."""

    data_to_send = Signal(bytes)
    size_changed = Signal(int, int)  # cols, rows
    close_requested = Signal()
    reconnect_requested = Signal()
    stats_updated = Signal(dict)

    def __init__(self, name, font_family="Courier New", font_size=10,
                 cursor_style="underline", parent=None):
        super().__init__(parent)
        self.name = name
        self.session = None
        self.connection_params = None
        self.stats_worker = None
        self.last_stats = None
        self.cwd_tracker = CwdTracker()

        self.title_label = QLabel(name)
        self.title_label.setObjectName("sectionTitle")

        self.terminal = TerminalWidget(
            font_family=font_family, font_size=font_size, cursor_style=cursor_style,
        )

        self.exclude_checkbox = QCheckBox("Exclude from Multi-Exec")

        self.reconnect_btn = QPushButton("Reconnect")
        self.reconnect_btn.setVisible(False)
        self.reconnect_btn.clicked.connect(self.reconnect_requested)

        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.close_requested)

        controls = QWidget()
        controls_layout = QHBoxLayout(controls)
        controls_layout.setContentsMargins(0, 0, 0, 0)
        controls_layout.setSpacing(8)
        controls_layout.addWidget(self.exclude_checkbox)
        controls_layout.addStretch()
        controls_layout.addWidget(self.reconnect_btn)
        controls_layout.addWidget(close_btn)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(4)
        layout.addWidget(self.title_label)
        layout.addWidget(self.terminal, 1)
        layout.addWidget(controls)

        self.terminal.data_to_send.connect(self.data_to_send)
        self.terminal.data_to_send.connect(self.cwd_tracker.feed_input)
        self.terminal.size_changed.connect(self.size_changed)

    def apply_settings(self, font_family, font_size, cursor_style):
        self.terminal.apply_settings(font_family, font_size, cursor_style)

    def start_stats_worker(self):
        if self.session and self.session.client and self.stats_worker is None:
            self.stats_worker = RemoteStatsWorker(self.session.client)
            self.stats_worker.stats_ready.connect(self._on_stats_ready)
            self.stats_worker.start()

    def _on_stats_ready(self, stats: dict):
        self.last_stats = stats
        self.stats_updated.emit(stats)

    def _stop_stats_worker(self):
        if self.stats_worker:
            self.stats_worker.stop()
            self.stats_worker.wait()
            self.stats_worker = None

    def disconnect_session(self):
        self._stop_stats_worker()
        self.last_stats = None
        if self.session:
            self.session.stop()
            self.session.wait()
            self.session = None
