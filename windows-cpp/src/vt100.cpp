#include "vt100.h"
#include "colors.h"

#include <QDebug>
#include <algorithm>

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------
VT100Screen::VT100Screen(int cols, int rows, int historyLines)
    : m_cols(cols), m_rows(rows), m_historyLines(historyLines)
{
    m_scrollTop    = 0;
    m_scrollBottom = rows - 1;
    m_screen.resize(rows);
    for (auto &row : m_screen) row = blankLine();
}

// -----------------------------------------------------------------------
// Blank line factory
// -----------------------------------------------------------------------
QVector<Cell> VT100Screen::blankLine() const {
    QVector<Cell> row(m_cols);
    for (auto &c : row) {
        c.ch = ' ';
        c.fg = "default";
        c.bg = "default";
        c.bold = false;
        c.reverse = false;
    }
    return row;
}

// -----------------------------------------------------------------------
// Cell accessors
// -----------------------------------------------------------------------
Cell &VT100Screen::cell(int x, int y) {
    static Cell dummy;
    if (y < 0 || y >= m_rows || x < 0 || x >= m_cols)
        return dummy;
    return m_screen[y][x];
}

const Cell &VT100Screen::constCell(int x, int y) const {
    static const Cell def;
    if (y < 0 || y >= m_rows || x < 0 || x >= m_cols) return def;
    return m_screen[y][x];
}

Cell VT100Screen::cellAt(int x, int y) const {
    return constCell(x, y);
}

QStringList VT100Screen::display() const {
    QStringList lines;
    for (int r = 0; r < m_rows; ++r) {
        QString line;
        line.reserve(m_cols);
        for (int c = 0; c < m_cols; ++c)
            line += m_screen[r][c].ch;
        lines << line;
    }
    return lines;
}

// -----------------------------------------------------------------------
// History access
// -----------------------------------------------------------------------
Cell VT100Screen::historyCell(int x, int absY) const {
    static const Cell def;
    if (x < 0 || x >= m_cols) return def;

    int histSize = static_cast<int>(m_history.size());
    if (absY < histSize) {
        const auto &row = m_history[absY];
        if (x < row.size()) return row[x];
        return def;
    }
    int screenY = absY - histSize;
    if (screenY >= 0 && screenY < m_rows && x < m_cols)
        return m_screen[screenY][x];
    return def;
}

// -----------------------------------------------------------------------
// Scrollback
// -----------------------------------------------------------------------
void VT100Screen::pushToHistory(const QVector<Cell> &line) {
    m_history.append(line);
    if (m_history.size() > m_historyLines)
        m_history.removeFirst();
}

void VT100Screen::scrollBack(int lines) {
    int max = historyTop();
    m_scrollOffset = std::min(m_scrollOffset + lines, max);
}

void VT100Screen::scrollForward(int lines) {
    m_scrollOffset = std::max(0, m_scrollOffset - lines);
}

// -----------------------------------------------------------------------
// Resize
// -----------------------------------------------------------------------
void VT100Screen::resize(int newCols, int newRows) {
    // Adjust scroll region if it was tracking the full screen
    bool fullRegion = (m_scrollTop == 0 && m_scrollBottom == m_rows - 1);

    // Enlarge or shrink rows
    while (m_screen.size() < newRows) {
        m_screen.append(QVector<Cell>(newCols));
        for (auto &c : m_screen.last()) { c.ch=' '; c.fg="default"; c.bg="default"; }
    }
    while (m_screen.size() > newRows) {
        pushToHistory(m_screen.first());
        m_screen.removeFirst();
    }

    // Adjust column count
    for (auto &row : m_screen) {
        if (row.size() < newCols) {
            while (row.size() < newCols) {
                Cell blank; blank.ch=' '; blank.fg="default"; blank.bg="default";
                row.append(blank);
            }
        } else if (row.size() > newCols) {
            row.resize(newCols);
        }
    }

    m_cols = newCols;
    m_rows = newRows;

    if (fullRegion) {
        m_scrollTop    = 0;
        m_scrollBottom = m_rows - 1;
    } else {
        m_scrollBottom = std::min(m_scrollBottom, m_rows - 1);
    }

    m_cursorX = std::min(m_cursorX, m_cols - 1);
    m_cursorY = std::min(m_cursorY, m_rows - 1);
    m_scrollOffset = 0;
}

// -----------------------------------------------------------------------
// Feed
// -----------------------------------------------------------------------
void VT100Screen::feed(const QByteArray &data) {
    for (int i = 0; i < data.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);
        QChar ch = QChar(byte);  // single-byte for now

        // Handle UTF-8 multi-byte sequences (simplified: just decode 2-4 bytes)
        if (byte >= 0xC0 && i + 1 < data.size()) {
            uint codepoint = 0;
            int extra = 0;
            if ((byte & 0xE0) == 0xC0) { codepoint = byte & 0x1F; extra = 1; }
            else if ((byte & 0xF0) == 0xE0) { codepoint = byte & 0x0F; extra = 2; }
            else if ((byte & 0xF8) == 0xF0) { codepoint = byte & 0x07; extra = 3; }

            if (extra > 0 && i + extra < data.size()) {
                bool valid = true;
                for (int k = 1; k <= extra; ++k) {
                    unsigned char cb = static_cast<unsigned char>(data[i + k]);
                    if ((cb & 0xC0) != 0x80) { valid = false; break; }
                    codepoint = (codepoint << 6) | (cb & 0x3F);
                }
                if (valid) {
                    ch = QChar(codepoint);
                    i += extra;
                }
            }
        }

        processChar(ch);
    }
    // Snap scroll back to bottom on any feed
    m_scrollOffset = 0;
}

// -----------------------------------------------------------------------
// Parser state machine
// -----------------------------------------------------------------------
void VT100Screen::processChar(QChar ch) {
    switch (m_state) {
    case State::Normal:
        if (ch == '\x1b') {
            m_state = State::Escape;
        } else if (ch == '\r') {
            m_cursorX = 0;
        } else if (ch == '\n') {
            newline();
        } else if (ch == '\x08') {   // BS
            if (m_cursorX > 0) --m_cursorX;
        } else if (ch == '\x07') {   // BEL — ignore
        } else if (ch == '\x09') {   // HT (tab) — advance to next 8-stop
            int nextTab = ((m_cursorX / 8) + 1) * 8;
            m_cursorX = std::min(nextTab, m_cols - 1);
        } else if (ch >= ' ' || ch.unicode() >= 0x80) {
            // Printable character
            if (m_cursorX >= m_cols) {
                m_cursorX = 0;
                newline();
            }
            Cell c = m_currentAttr;
            c.ch = ch;
            m_screen[m_cursorY][m_cursorX] = c;
            ++m_cursorX;
        }
        break;

    case State::Escape:
        if (ch == '[') {
            m_state    = State::CSI;
            m_csiParam = "";
        } else if (ch == ']') {
            m_state    = State::OSC;
            m_csiParam = "";
        } else if (ch == '7') {   // DECSC save cursor
            m_savedCursorX = m_cursorX;
            m_savedCursorY = m_cursorY;
            m_state = State::Normal;
        } else if (ch == '8') {   // DECRC restore cursor
            m_cursorX = m_savedCursorX;
            m_cursorY = m_savedCursorY;
            m_state = State::Normal;
        } else if (ch == 'M') {   // RI reverse index
            if (m_cursorY == m_scrollTop) {
                scrollRegionDown(1);
            } else if (m_cursorY > 0) {
                --m_cursorY;
            }
            m_state = State::Normal;
        } else if (ch == 'c') {   // RIS — full reset
            m_screen.clear();
            m_screen.resize(m_rows);
            for (auto &row : m_screen) row = blankLine();
            m_cursorX = m_cursorY = 0;
            m_currentAttr = Cell();
            m_scrollTop = 0;
            m_scrollBottom = m_rows - 1;
            m_state = State::Normal;
        } else {
            m_state = State::Normal; // unknown Esc sequence
        }
        break;

    case State::CSI:
        if (ch.isLetter() || ch == '@' || ch == '`') {
            processCSI(ch);
            m_state = State::Normal;
        } else {
            m_csiParam += ch;
        }
        break;

    case State::OSC:
        if (ch == '\x07' || ch == '\x9c') {
            processOSC();
            m_state = State::Normal;
        } else if (ch == '\\' && !m_csiParam.isEmpty() &&
                   m_csiParam.back() == '\x1b') {
            // ST = ESC backslash
            m_csiParam.chop(1);
            processOSC();
            m_state = State::Normal;
        } else {
            m_csiParam += ch;
        }
        break;
    }
}

void VT100Screen::processOSC() {
    // We only need to ignore OSC sequences (no window title in C++)
    // Could parse OSC 7 (cwd) if needed in future
}

// -----------------------------------------------------------------------
// CSI parameter parsing
// -----------------------------------------------------------------------
QVector<int> VT100Screen::parseCsiParams(int defaultVal) const {
    QVector<int> params;
    if (m_csiParam.isEmpty() || m_csiParam == "?") {
        params << defaultVal;
        return params;
    }
    QString param = m_csiParam;
    // Strip leading '?'
    if (param.startsWith('?')) param = param.mid(1);

    for (const QString &part : param.split(';')) {
        bool ok = false;
        int val = part.toInt(&ok);
        params << (ok ? val : defaultVal);
    }
    if (params.isEmpty()) params << defaultVal;
    return params;
}

// -----------------------------------------------------------------------
// CSI dispatcher
// -----------------------------------------------------------------------
void VT100Screen::processCSI(QChar final) {
    QVector<int> p = parseCsiParams(0);
    int p0 = p.value(0, 0);

    switch (final.toLatin1()) {
    case 'A': cursorUp(   p0 ? p0 : 1); break;
    case 'B': cursorDown( p0 ? p0 : 1); break;
    case 'C': cursorRight(p0 ? p0 : 1); break;
    case 'D': cursorLeft( p0 ? p0 : 1); break;
    case 'E': // cursor next line
        m_cursorY = std::min(m_cursorY + (p0 ? p0 : 1), m_rows - 1);
        m_cursorX = 0;
        break;
    case 'F': // cursor previous line
        m_cursorY = std::max(m_cursorY - (p0 ? p0 : 1), 0);
        m_cursorX = 0;
        break;
    case 'G': // cursor horizontal absolute
        m_cursorX = std::min(std::max((p0 ? p0 : 1) - 1, 0), m_cols - 1);
        break;
    case 'H': // CUP
    case 'f': { // HVP
        int row = p.value(0, 1) ? p.value(0, 1) : 1;
        int col = p.value(1, 1) ? p.value(1, 1) : 1;
        cursorPos(row - 1, col - 1);
        break;
    }
    case 'J': eraseDisplay(p0); break;
    case 'K': eraseLine(p0);    break;
    case 'L': insertLines(p0 ? p0 : 1); break;
    case 'M': deleteLines(p0 ? p0 : 1); break;
    case 'P': deleteChars(p0 ? p0 : 1); break;
    case '@': insertChars(p0 ? p0 : 1); break;
    case 'S': scrollUp(  p0 ? p0 : 1); break;
    case 'T': scrollDown(p0 ? p0 : 1); break;
    case 'm': applySGR(p); break;
    case 'r': { // DECSTBM scroll region
        int top    = (p.value(0, 1) ? p.value(0, 1) : 1) - 1;
        int bottom = (p.value(1, m_rows) ? p.value(1, m_rows) : m_rows) - 1;
        if (top < bottom && bottom < m_rows) {
            m_scrollTop    = top;
            m_scrollBottom = bottom;
            m_cursorX = m_cursorY = 0;
        }
        break;
    }
    case 's': // save cursor
        m_savedCursorX = m_cursorX;
        m_savedCursorY = m_cursorY;
        break;
    case 'u': // restore cursor
        m_cursorX = m_savedCursorX;
        m_cursorY = m_savedCursorY;
        break;
    case 'h': // set mode
        if (m_csiParam == "?25") m_cursorHidden = false;
        // ?1049 alternate screen — stub
        break;
    case 'l': // reset mode
        if (m_csiParam == "?25") m_cursorHidden = true;
        // ?1049 alternate screen — stub
        break;
    case 'n': // DSR
        break;
    case 'd': // line position absolute
        m_cursorY = std::min(std::max((p0 ? p0 : 1) - 1, 0), m_rows - 1);
        break;
    default:
        break;
    }
}

// -----------------------------------------------------------------------
// Cursor movement
// -----------------------------------------------------------------------
void VT100Screen::cursorUp(int n) {
    m_cursorY = std::max(m_cursorY - n, m_scrollTop);
}

void VT100Screen::cursorDown(int n) {
    m_cursorY = std::min(m_cursorY + n, m_scrollBottom);
}

void VT100Screen::cursorRight(int n) {
    m_cursorX = std::min(m_cursorX + n, m_cols - 1);
}

void VT100Screen::cursorLeft(int n) {
    m_cursorX = std::max(m_cursorX - n, 0);
}

void VT100Screen::cursorPos(int row, int col) {
    m_cursorY = std::min(std::max(row, 0), m_rows - 1);
    m_cursorX = std::min(std::max(col, 0), m_cols - 1);
}

// -----------------------------------------------------------------------
// Newline handling
// -----------------------------------------------------------------------
void VT100Screen::newline() {
    if (m_cursorY == m_scrollBottom) {
        scrollRegionUp(1);
    } else if (m_cursorY < m_rows - 1) {
        ++m_cursorY;
    }
}

// -----------------------------------------------------------------------
// Scroll region
// -----------------------------------------------------------------------
void VT100Screen::scrollRegionUp(int n) {
    for (int i = 0; i < n; ++i) {
        // push top line of region to history if it's the top of screen
        if (m_scrollTop == 0) {
            pushToHistory(m_screen[0]);
        }
        // Move lines up within region
        for (int r = m_scrollTop; r < m_scrollBottom; ++r) {
            m_screen[r] = m_screen[r + 1];
        }
        m_screen[m_scrollBottom] = blankLine();
    }
}

void VT100Screen::scrollRegionDown(int n) {
    for (int i = 0; i < n; ++i) {
        for (int r = m_scrollBottom; r > m_scrollTop; --r) {
            m_screen[r] = m_screen[r - 1];
        }
        m_screen[m_scrollTop] = blankLine();
    }
}

void VT100Screen::scrollUp(int n) {
    scrollRegionUp(n);
}

void VT100Screen::scrollDown(int n) {
    scrollRegionDown(n);
}

// -----------------------------------------------------------------------
// Erase operations
// -----------------------------------------------------------------------
void VT100Screen::eraseDisplay(int mode) {
    switch (mode) {
    case 0: // below cursor
        for (int x = m_cursorX; x < m_cols; ++x)
            m_screen[m_cursorY][x] = Cell();
        for (int r = m_cursorY + 1; r < m_rows; ++r)
            m_screen[r] = blankLine();
        break;
    case 1: // above cursor
        for (int r = 0; r < m_cursorY; ++r)
            m_screen[r] = blankLine();
        for (int x = 0; x <= m_cursorX; ++x)
            m_screen[m_cursorY][x] = Cell();
        break;
    case 2: // all
    case 3: // all + scrollback (treat same as 2)
        for (int r = 0; r < m_rows; ++r)
            m_screen[r] = blankLine();
        m_cursorX = m_cursorY = 0;
        break;
    }
}

void VT100Screen::eraseLine(int mode) {
    switch (mode) {
    case 0: // right of cursor
        for (int x = m_cursorX; x < m_cols; ++x)
            m_screen[m_cursorY][x] = Cell();
        break;
    case 1: // left of cursor
        for (int x = 0; x <= m_cursorX; ++x)
            m_screen[m_cursorY][x] = Cell();
        break;
    case 2: // entire line
        m_screen[m_cursorY] = blankLine();
        break;
    }
}

// -----------------------------------------------------------------------
// Insert / delete lines
// -----------------------------------------------------------------------
void VT100Screen::insertLines(int n) {
    for (int i = 0; i < n; ++i) {
        for (int r = m_scrollBottom; r > m_cursorY; --r)
            m_screen[r] = m_screen[r - 1];
        m_screen[m_cursorY] = blankLine();
    }
}

void VT100Screen::deleteLines(int n) {
    for (int i = 0; i < n; ++i) {
        for (int r = m_cursorY; r < m_scrollBottom; ++r)
            m_screen[r] = m_screen[r + 1];
        m_screen[m_scrollBottom] = blankLine();
    }
}

void VT100Screen::deleteChars(int n) {
    auto &row = m_screen[m_cursorY];
    for (int i = 0; i < n && m_cursorX < m_cols; ++i) {
        row.remove(m_cursorX, 1);
        Cell blank; blank.ch=' '; blank.fg="default"; blank.bg="default";
        row.append(blank);
    }
}

void VT100Screen::insertChars(int n) {
    auto &row = m_screen[m_cursorY];
    for (int i = 0; i < n; ++i) {
        Cell blank; blank.ch=' '; blank.fg="default"; blank.bg="default";
        row.insert(m_cursorX, blank);
        if (row.size() > m_cols) row.resize(m_cols);
    }
}

// -----------------------------------------------------------------------
// SGR (Select Graphic Rendition)
// -----------------------------------------------------------------------
static const char* sgrColorName(int n, bool bright) {
    static const char* names[] = {
        "black","red","green","brown","blue","magenta","cyan","white"
    };
    static const char* brightNames[] = {
        "brightblack","brightred","brightgreen","brightbrown",
        "brightblue","brightmagenta","brightcyan","brightwhite"
    };
    if (n >= 0 && n < 8) return bright ? brightNames[n] : names[n];
    return "default";
}

void VT100Screen::applySGR(const QVector<int> &params) {
    for (int i = 0; i < params.size(); ++i) {
        int p = params[i];
        if (p == 0) {
            m_currentAttr = Cell();
        } else if (p == 1) {
            m_currentAttr.bold = true;
        } else if (p == 7) {
            m_currentAttr.reverse = true;
        } else if (p == 22) {
            m_currentAttr.bold = false;
        } else if (p == 27) {
            m_currentAttr.reverse = false;
        } else if (p >= 30 && p <= 37) {
            m_currentAttr.fg = sgrColorName(p - 30, false);
        } else if (p == 38) {
            // Extended foreground
            if (i + 2 < params.size() && params[i+1] == 5) {
                m_currentAttr.fg = palette256(params[i+2]);
                i += 2;
            } else if (i + 4 < params.size() && params[i+1] == 2) {
                int r = params[i+2], g = params[i+3], b = params[i+4];
                m_currentAttr.fg = QString("#%1%2%3")
                    .arg(r, 2, 16, QChar('0'))
                    .arg(g, 2, 16, QChar('0'))
                    .arg(b, 2, 16, QChar('0'));
                i += 4;
            }
        } else if (p == 39) {
            m_currentAttr.fg = "default";
        } else if (p >= 40 && p <= 47) {
            m_currentAttr.bg = sgrColorName(p - 40, false);
        } else if (p == 48) {
            if (i + 2 < params.size() && params[i+1] == 5) {
                m_currentAttr.bg = palette256(params[i+2]);
                i += 2;
            } else if (i + 4 < params.size() && params[i+1] == 2) {
                int r = params[i+2], g = params[i+3], b = params[i+4];
                m_currentAttr.bg = QString("#%1%2%3")
                    .arg(r, 2, 16, QChar('0'))
                    .arg(g, 2, 16, QChar('0'))
                    .arg(b, 2, 16, QChar('0'));
                i += 4;
            }
        } else if (p == 49) {
            m_currentAttr.bg = "default";
        } else if (p >= 90 && p <= 97) {
            m_currentAttr.fg = sgrColorName(p - 90, true);
        } else if (p >= 100 && p <= 107) {
            m_currentAttr.bg = sgrColorName(p - 100, true);
        }
    }
}
