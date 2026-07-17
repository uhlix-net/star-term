#include "terminalwidget.h"
#include "colors.h"

#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSizePolicy>
#include <QWheelEvent>

// Mapping of Qt keys to terminal escape sequences (mirrors SPECIAL_KEYS in Python)
static QByteArray specialKey(int key) {
    switch (key) {
    case Qt::Key_Return:   return "\r";
    case Qt::Key_Enter:    return "\r";
    case Qt::Key_Backspace:return "\x7f";
    case Qt::Key_Tab:      return "\t";
    case Qt::Key_Escape:   return "\x1b";
    case Qt::Key_Up:       return "\x1b[A";
    case Qt::Key_Down:     return "\x1b[B";
    case Qt::Key_Right:    return "\x1b[C";
    case Qt::Key_Left:     return "\x1b[D";
    case Qt::Key_Home:     return "\x1b[H";
    case Qt::Key_End:      return "\x1b[F";
    case Qt::Key_Delete:   return "\x1b[3~";
    case Qt::Key_PageUp:   return "\x1b[5~";
    case Qt::Key_PageDown: return "\x1b[6~";
    default: return QByteArray();
    }
}

TerminalWidget::TerminalWidget(int cols, int rows,
    const QString &fontFamily, int fontSize,
    const QString &cursorStyle, QWidget *parent)
    : QWidget(parent)
    , m_screen(cols, rows, HISTORY_LINES)
    , m_cursorStyle(cursorStyle)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_scrollbar = new QScrollBar(Qt::Vertical, this);
    connect(m_scrollbar, &QScrollBar::valueChanged,
            this, &TerminalWidget::onScrollbarChanged);

    setFont_(fontFamily, fontSize);
    updateScrollbar();
}

// -----------------------------------------------------------------------
// Font
// -----------------------------------------------------------------------
void TerminalWidget::setFont_(const QString &fontFamily, int fontSize) {
    m_font = QFont(fontFamily, fontSize);
    m_font.setStyleHint(QFont::Monospace);
    setFont(m_font);

    QFontMetrics fm(m_font);
    m_charWidth  = fm.horizontalAdvance('M');
    m_charHeight = fm.height();
    m_scrollbarWidth = m_scrollbar->sizeHint().width();

    setMinimumSize(
        m_charWidth  * MIN_COLS + 2 * PADDING + m_scrollbarWidth,
        m_charHeight * MIN_ROWS + 2 * PADDING
    );
}

QSize TerminalWidget::sizeHint() const {
    return QSize(
        m_charWidth  * m_screen.cols() + 2 * PADDING + m_scrollbarWidth,
        m_charHeight * m_screen.rows() + 2 * PADDING
    );
}

void TerminalWidget::applySettings(const QString &fontFamily, int fontSize, const QString &cursorStyle) {
    m_cursorStyle = cursorStyle;
    setFont_(fontFamily, fontSize);
    int cols = std::max(1, (width()  - 2 * PADDING - m_scrollbarWidth) / m_charWidth);
    int rows = std::max(1, (height() - 2 * PADDING) / m_charHeight);
    if (cols != m_screen.cols() || rows != m_screen.rows()) {
        m_screen.resize(cols, rows);
        emit sizeChanged(cols, rows);
    }
    updateScrollbar();
    update();
}

// -----------------------------------------------------------------------
// Feed
// -----------------------------------------------------------------------
void TerminalWidget::feed(const QByteArray &data) {
    m_screen.feed(data);
    updateScrollbar();
    update();
}

// -----------------------------------------------------------------------
// Resize event
// -----------------------------------------------------------------------
void TerminalWidget::resizeEvent(QResizeEvent *event) {
    m_scrollbar->setGeometry(
        width() - m_scrollbarWidth, 0, m_scrollbarWidth, height()
    );

    int cols = std::max(1, (width()  - 2 * PADDING - m_scrollbarWidth) / m_charWidth);
    int rows = std::max(1, (height() - 2 * PADDING) / m_charHeight);
    if (cols != m_screen.cols() || rows != m_screen.rows()) {
        m_screen.resize(cols, rows);
        updateScrollbar();
        emit sizeChanged(cols, rows);
    }
    update();
    QWidget::resizeEvent(event);
}

// -----------------------------------------------------------------------
// Scrollbar
// -----------------------------------------------------------------------
void TerminalWidget::updateScrollbar() {
    int histTop   = m_screen.historyTop();
    int total     = m_screen.historyTotal();
    int pageStep  = std::max(1, m_screen.rows());

    m_scrollbar->blockSignals(true);
    m_scrollbar->setRange(0, total - m_screen.rows()); // max scroll
    m_scrollbar->setPageStep(pageStep);
    // value = 0 means at top, value = max means at bottom (live)
    int maxVal = std::max(0, total - m_screen.rows());
    m_scrollbar->setValue(maxVal - m_screen.scrollOffset());
    m_scrollbar->blockSignals(false);
}

void TerminalWidget::onScrollbarChanged(int value) {
    int total  = m_screen.historyTotal();
    int maxVal = std::max(0, total - m_screen.rows());
    int wantedOffset = maxVal - value;
    int curOffset    = m_screen.scrollOffset();
    int diff = wantedOffset - curOffset;
    if (diff > 0) m_screen.scrollBack(diff);
    else if (diff < 0) m_screen.scrollForward(-diff);
    update();
}

void TerminalWidget::scrollLines(int lines) {
    if (lines < 0) m_screen.scrollBack(-lines);
    else if (lines > 0) m_screen.scrollForward(lines);
    updateScrollbar();
    update();
}

// -----------------------------------------------------------------------
// Wheel event
// -----------------------------------------------------------------------
void TerminalWidget::wheelEvent(QWheelEvent *event) {
    int steps = event->angleDelta().y() / 120;
    if (steps) scrollLines(-steps);
    event->accept();
}

void TerminalWidget::mousePressEvent(QMouseEvent *event) {
    setFocus();
    QWidget::mousePressEvent(event);
}

// -----------------------------------------------------------------------
// focusNextPrevChild override (critical for Tab to reach shell)
// -----------------------------------------------------------------------
bool TerminalWidget::focusNextPrevChild(bool /*next*/) {
    return false;
}

// -----------------------------------------------------------------------
// Paint event — matches paintEvent in terminal_widget.py exactly
// -----------------------------------------------------------------------
void TerminalWidget::paintEvent(QPaintEvent * /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(DEFAULT_BG));
    painter.setFont(m_font);
    int ascent = painter.fontMetrics().ascent();

    int histTop = m_screen.historyTop();
    int offset  = m_screen.scrollOffset();

    // When scrolled back, compute the absolute start row in history+screen space
    int absStart = histTop - offset;  // first abs row to show

    for (int y = 0; y < m_screen.rows(); ++y) {
        int absY = absStart + y;
        for (int x = 0; x < m_screen.cols(); ++x) {
            Cell cell;
            if (offset > 0) {
                cell = m_screen.historyCell(x, absY);
            } else {
                cell = m_screen.cellAt(x, y);
            }

            if (cell.ch == ' ' && cell.bg == "default" && !cell.reverse)
                continue;

            QString fgColor = paletteColor(cell.fg);
            if (fgColor.isEmpty()) fgColor = cell.fg.startsWith('#') ? cell.fg : DEFAULT_FG;
            QString bgColor = paletteColor(cell.bg);
            if (bgColor.isEmpty()) bgColor = cell.bg.startsWith('#') ? cell.bg : DEFAULT_BG;

            if (cell.reverse) std::swap(fgColor, bgColor);

            int px = PADDING + x * m_charWidth;
            int py = PADDING + y * m_charHeight;

            if (bgColor != DEFAULT_BG)
                painter.fillRect(px, py, m_charWidth, m_charHeight, QColor(bgColor));

            if (cell.ch != ' ') {
                QFont f = m_font;
                if (cell.bold) f.setBold(true);
                painter.setFont(f);
                painter.setPen(QColor(fgColor));
                painter.drawText(px, py + ascent, QString(cell.ch));
                painter.setFont(m_font);
            }
        }
    }

    // Draw cursor only in live view
    if (offset == 0 && !m_screen.cursorHidden()) {
        int cx = PADDING + m_screen.cursorX() * m_charWidth;
        int cy = PADDING + m_screen.cursorY() * m_charHeight;

        if (m_cursorStyle == "block") {
            painter.fillRect(cx, cy, m_charWidth, m_charHeight, QColor(DEFAULT_FG));
            Cell cursorCell = m_screen.cellAt(m_screen.cursorX(), m_screen.cursorY());
            if (cursorCell.ch != ' ') {
                painter.setPen(QColor(DEFAULT_BG));
                painter.drawText(cx, cy + ascent, QString(cursorCell.ch));
            }
        } else {
            int underlineH = std::max(2, m_charHeight / 12);
            painter.fillRect(
                cx, cy + m_charHeight - underlineH,
                m_charWidth, underlineH,
                QColor(DEFAULT_FG)
            );
        }
    }
}

// -----------------------------------------------------------------------
// Key press — matches keyPressEvent in terminal_widget.py exactly
// -----------------------------------------------------------------------
void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    // Scrollback paging: Shift+PageUp / Shift+PageDown
    if (key == Qt::Key_PageUp && (mods & Qt::ShiftModifier)) {
        scrollLines(-m_screen.rows());
        event->accept();
        return;
    }
    if (key == Qt::Key_PageDown && (mods & Qt::ShiftModifier)) {
        scrollLines(m_screen.rows());
        event->accept();
        return;
    }

    // Paste: Shift+Insert or Ctrl+Shift+V
    if ((key == Qt::Key_Insert && (mods & Qt::ShiftModifier)) ||
        (key == Qt::Key_V && (mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier))) {
        QString text = QApplication::clipboard()->text();
        if (!text.isEmpty()) emit dataToSend(text.toUtf8());
        event->accept();
        return;
    }

    // Copy: Ctrl+Shift+C
    if (key == Qt::Key_C && (mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)) {
        QApplication::clipboard()->setText(m_screen.display().join('\n'));
        event->accept();
        return;
    }

    QByteArray data = keyToBytes(event);
    if (!data.isEmpty()) emit dataToSend(data);
    event->accept();
}

QByteArray TerminalWidget::keyToBytes(QKeyEvent *event) {
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();
    QString text = event->text();

    QByteArray special = specialKey(key);
    if (!special.isEmpty()) return special;

    // Ctrl+A..Z → \x01..\x1a
    if ((mods & Qt::ControlModifier) && key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QByteArray(1, static_cast<char>(key - Qt::Key_A + 1));
    }

    if (!text.isEmpty()) return text.toUtf8();
    return QByteArray();
}
