from collections import defaultdict

import pyte
from PySide6.QtCore import QSize, Qt, Signal
from PySide6.QtGui import QColor, QFont, QFontMetrics, QKeyEvent, QPainter
from PySide6.QtWidgets import QApplication, QScrollBar, QSizePolicy, QWidget

from .colors import DEFAULT_BG, DEFAULT_FG, PALETTE

SPECIAL_KEYS = {
    Qt.Key_Return: b"\r",
    Qt.Key_Enter: b"\r",
    Qt.Key_Backspace: b"\x7f",
    Qt.Key_Tab: b"\t",
    Qt.Key_Escape: b"\x1b",
    Qt.Key_Up: b"\x1b[A",
    Qt.Key_Down: b"\x1b[B",
    Qt.Key_Right: b"\x1b[C",
    Qt.Key_Left: b"\x1b[D",
    Qt.Key_Home: b"\x1b[H",
    Qt.Key_End: b"\x1b[F",
    Qt.Key_Delete: b"\x1b[3~",
    Qt.Key_PageUp: b"\x1b[5~",
    Qt.Key_PageDown: b"\x1b[6~",
}

PADDING = 6
MIN_COLS = 10
MIN_ROWS = 2
HISTORY_LINES = 2000


class TerminalWidget(QWidget):
    data_to_send = Signal(bytes)
    size_changed = Signal(int, int)  # cols, rows

    def __init__(self, cols=80, rows=24, font_family="Courier New",
                 font_size=10, cursor_style="underline", parent=None):
        super().__init__(parent)
        self.setFocusPolicy(Qt.StrongFocus)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        self.cursor_style = cursor_style
        self.screen = pyte.HistoryScreen(cols, rows, history=HISTORY_LINES, ratio=1.0 / rows)
        self.stream = pyte.ByteStream(self.screen)

        self.scrollbar = QScrollBar(Qt.Vertical, self)
        self.scrollbar.valueChanged.connect(self._on_scrollbar_changed)

        self._set_font(font_family, font_size)
        self._update_scrollbar()

    def _set_font(self, font_family, font_size):
        self.font_ = QFont(font_family, font_size)
        self.font_.setStyleHint(QFont.Monospace)
        self.setFont(self.font_)

        metrics = QFontMetrics(self.font_)
        self.char_width = metrics.horizontalAdvance("M")
        self.char_height = metrics.height()
        self.scrollbar_width = self.scrollbar.sizeHint().width()

        # Keep the hard minimum small so panes can shrink to fit a Multi-Exec
        # grid; sizeHint() below still requests the full terminal size.
        self.setMinimumSize(
            self.char_width * MIN_COLS + 2 * PADDING + self.scrollbar_width,
            self.char_height * MIN_ROWS + 2 * PADDING,
        )

    def sizeHint(self):
        return QSize(
            self.char_width * self.screen.columns + 2 * PADDING + self.scrollbar_width,
            self.char_height * self.screen.lines + 2 * PADDING,
        )

    def apply_settings(self, font_family, font_size, cursor_style):
        self.cursor_style = cursor_style
        self._set_font(font_family, font_size)
        cols = max(1, (self.width() - 2 * PADDING - self.scrollbar_width) // self.char_width)
        rows = max(1, (self.height() - 2 * PADDING) // self.char_height)
        if (cols, rows) != (self.screen.columns, self.screen.lines):
            self._resize_screen(rows, cols)
            self._update_history_ratio()
            self.size_changed.emit(cols, rows)
        self.update()

    def _resize_screen(self, rows, cols):
        """Resize the pyte screen. If the height shrinks, pyte's own
        resize() silently drops lines from the top of the screen without
        moving them to scrollback; do that ourselves first so no text is
        lost and the cursor stays on the same line of content."""
        screen = self.screen
        old_lines = screen.lines

        if rows < old_lines:
            diff = old_lines - rows
            for y in range(diff):
                screen.history.top.append(screen.buffer[y])

            new_buffer = defaultdict(screen.buffer.default_factory)
            for y in range(rows):
                new_buffer[y] = screen.buffer[y + diff]
            screen.buffer = new_buffer

            screen.cursor.y = max(0, screen.cursor.y - diff)
            screen.lines = rows
            screen.margins = None

        screen.resize(rows, cols)

    def feed(self, data: bytes):
        history = self.screen.history
        scrolled_back = history.position < history.size
        top_before = len(history.top)

        self.stream.feed(data)

        if scrolled_back:
            # New output snaps pyte's view back to the bottom; restore the
            # user's scroll position so reading old output isn't disrupted.
            while len(self.screen.history.top) > top_before:
                before = len(self.screen.history.top)
                self.screen.prev_page()
                if len(self.screen.history.top) >= before:
                    break

        self._update_scrollbar()
        self.update()

    def resizeEvent(self, event):
        self.scrollbar.setGeometry(
            self.width() - self.scrollbar_width, 0, self.scrollbar_width, self.height()
        )

        cols = max(1, (self.width() - 2 * PADDING - self.scrollbar_width) // self.char_width)
        rows = max(1, (self.height() - 2 * PADDING) // self.char_height)
        if (cols, rows) != (self.screen.columns, self.screen.lines):
            self._resize_screen(rows, cols)
            self._update_history_ratio()
            self._update_scrollbar()
            self.size_changed.emit(cols, rows)
        self.update()
        super().resizeEvent(event)

    # ------------------------------------------------------------------
    # Scrollback
    # ------------------------------------------------------------------

    def _update_history_ratio(self):
        # Keep ~1 scrollback line per page step regardless of terminal height.
        self.screen.history = self.screen.history._replace(
            ratio=1.0 / max(1, self.screen.lines)
        )

    def _update_scrollbar(self):
        history = self.screen.history
        scrolled = history.size - history.position
        total = len(history.top) + scrolled

        self.scrollbar.blockSignals(True)
        self.scrollbar.setRange(0, total)
        self.scrollbar.setPageStep(max(1, self.screen.lines))
        self.scrollbar.setValue(len(history.top))
        self.scrollbar.blockSignals(False)

    def _on_scrollbar_changed(self, value):
        history = self.screen.history
        diff = value - len(history.top)
        if diff > 0:
            for _ in range(diff):
                self.screen.next_page()
        elif diff < 0:
            for _ in range(-diff):
                self.screen.prev_page()
        self.update()

    def scroll_lines(self, lines: int):
        """Scroll the view by `lines` rows: negative scrolls back (older),
        positive scrolls forward (toward live)."""
        if lines < 0:
            for _ in range(-lines):
                self.screen.prev_page()
        elif lines > 0:
            for _ in range(lines):
                self.screen.next_page()
        self._update_scrollbar()
        self.update()

    def wheelEvent(self, event):
        steps = event.angleDelta().y() // 120
        if steps:
            self.scroll_lines(-steps)
        event.accept()

    def mousePressEvent(self, event):
        self.setFocus()
        super().mousePressEvent(event)

    def focusNextPrevChild(self, next):
        # Keep Tab/Shift+Tab from moving focus to another widget so they
        # reach keyPressEvent and get sent to the shell for completion.
        return False

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(DEFAULT_BG))
        painter.setFont(self.font_)
        ascent = painter.fontMetrics().ascent()

        for y in range(self.screen.lines):
            line = self.screen.buffer[y]
            for x in range(self.screen.columns):
                char = line[x]
                if char.data == " " and char.bg == "default" and not char.reverse:
                    continue

                fg = PALETTE.get(char.fg) or DEFAULT_FG
                bg = PALETTE.get(char.bg) or DEFAULT_BG
                if char.reverse:
                    fg, bg = bg, fg

                px = PADDING + x * self.char_width
                py = PADDING + y * self.char_height

                if bg != DEFAULT_BG:
                    painter.fillRect(px, py, self.char_width, self.char_height, QColor(bg))

                if char.data and char.data != " ":
                    painter.setPen(QColor(fg))
                    painter.drawText(px, py + ascent, char.data)

        if not self.screen.cursor.hidden:
            cx = PADDING + self.screen.cursor.x * self.char_width
            cy = PADDING + self.screen.cursor.y * self.char_height

            if self.cursor_style == "block":
                painter.fillRect(cx, cy, self.char_width, self.char_height, QColor(DEFAULT_FG))
                cursor_char = self.screen.buffer[self.screen.cursor.y][self.screen.cursor.x].data
                if cursor_char and cursor_char != " ":
                    painter.setPen(QColor(DEFAULT_BG))
                    painter.drawText(cx, cy + ascent, cursor_char)
            else:
                underline_height = max(2, self.char_height // 12)
                painter.fillRect(
                    cx, cy + self.char_height - underline_height,
                    self.char_width, underline_height, QColor(DEFAULT_FG),
                )

    def keyPressEvent(self, event: QKeyEvent):
        key = event.key()
        modifiers = event.modifiers()

        # Scrollback paging: Shift+PageUp / Shift+PageDown
        if key == Qt.Key_PageUp and modifiers & Qt.ShiftModifier:
            self.scroll_lines(-self.screen.lines)
            event.accept()
            return
        if key == Qt.Key_PageDown and modifiers & Qt.ShiftModifier:
            self.scroll_lines(self.screen.lines)
            event.accept()
            return

        # Paste: Shift+Insert or Ctrl+Shift+V
        if (key == Qt.Key_Insert and modifiers & Qt.ShiftModifier) or (
            key == Qt.Key_V and modifiers & Qt.ControlModifier and modifiers & Qt.ShiftModifier
        ):
            text = QApplication.clipboard().text()
            if text:
                self.data_to_send.emit(text.encode("utf-8"))
            event.accept()
            return

        # Copy current screen contents: Ctrl+Shift+C
        if key == Qt.Key_C and modifiers & Qt.ControlModifier and modifiers & Qt.ShiftModifier:
            QApplication.clipboard().setText("\n".join(self.screen.display))
            event.accept()
            return

        data = self._key_to_bytes(event)
        if data:
            self.data_to_send.emit(data)
        event.accept()

    def _key_to_bytes(self, event: QKeyEvent) -> bytes:
        key = event.key()
        modifiers = event.modifiers()
        text = event.text()

        if key in SPECIAL_KEYS:
            return SPECIAL_KEYS[key]

        if modifiers & Qt.ControlModifier and Qt.Key_A <= key <= Qt.Key_Z:
            return bytes([key - Qt.Key_A + 1])

        if text:
            return text.encode("utf-8")

        return b""
