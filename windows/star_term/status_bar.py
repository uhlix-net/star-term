from PySide6.QtWidgets import QLabel, QStatusBar


class SystemStatusBar(QStatusBar):
    """Displays CPU/load/RAM/swap stats for the remote host of the currently
    selected session. Hidden in Multi-Exec View."""

    def __init__(self, parent=None):
        super().__init__(parent)

        self.cpu_label = QLabel()
        self.load_label = QLabel()
        self.ram_label = QLabel()
        self.swap_label = QLabel()

        for label in (self.cpu_label, self.load_label, self.ram_label, self.swap_label):
            label.setMinimumWidth(110)
            self.addPermanentWidget(label)

        self.update_stats(None)

    def update_stats(self, stats: dict | None):
        if not stats:
            self.cpu_label.setText("CPU: N/A")
            self.load_label.setText("Load: N/A")
            self.ram_label.setText("RAM: N/A")
            self.swap_label.setText("Swap: N/A")
            return

        self.cpu_label.setText(f"CPU: {stats['cpu']:.0f}%")
        self.load_label.setText(f"Load: {stats['load']:.2f}")
        self.ram_label.setText(f"RAM: {stats['ram']:.0f}%")
        self.swap_label.setText(f"Swap: {stats['swap']:.0f}%")
