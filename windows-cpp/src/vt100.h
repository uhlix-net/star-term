#pragma once
#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QByteArray>

// VT100 terminal emulator replacing pyte.
// Implements the escape sequences documented in the task specification.

struct Cell {
    QChar   ch      = ' ';
    QString fg      = "default";  // color name, "default", or "#rrggbb" for 256
    QString bg      = "default";
    bool    bold    = false;
    bool    reverse = false;
};

class VT100Screen {
public:
    explicit VT100Screen(int cols = 80, int rows = 24, int historyLines = 2000);

    void feed(const QByteArray &data);

    int  cols()          const { return m_cols; }
    int  rows()          const { return m_rows; }
    int  cursorX()       const { return m_cursorX; }
    int  cursorY()       const { return m_cursorY; }
    bool cursorHidden()  const { return m_cursorHidden; }

    // Current visible screen
    Cell        cellAt(int x, int y) const;
    QStringList display()            const;

    // Scrollback
    int  historyTop()                         const { return static_cast<int>(m_history.size()); }
    int  historyTotal()                       const { return historyTop() + m_rows; }
    Cell historyCell(int x, int absY)         const;
    int  scrollOffset()                       const { return m_scrollOffset; }

    void scrollBack(int lines);
    void scrollForward(int lines);

    void resize(int newCols, int newRows);

private:
    int  m_cols, m_rows;
    int  m_historyLines;
    int  m_cursorX = 0, m_cursorY = 0;
    int  m_savedCursorX = 0, m_savedCursorY = 0;
    bool m_cursorHidden = false;
    int  m_scrollTop = 0, m_scrollBottom = 0;   // scroll region (0-based rows)
    int  m_scrollOffset = 0;                     // 0 = bottom (live view)

    // Current screen buffer: m_screen[row][col]
    QVector<QVector<Cell>> m_screen;

    // History (oldest first)
    QVector<QVector<Cell>> m_history;

    // Current SGR attributes
    Cell m_currentAttr;

    // Parser state machine
    enum class State { Normal, Escape, CSI, OSC } m_state = State::Normal;
    QString m_csiParam;
    QChar   m_oscTerminator;

    // --- Internal helpers ---
    void processChar(QChar ch);
    void processESC(QChar ch);
    void processCSI(QChar finalByte);
    void processOSC();

    QVector<int> parseCsiParams(int defaultVal = 1) const;

    void cursorUp(int n);
    void cursorDown(int n);
    void cursorRight(int n);
    void cursorLeft(int n);
    void cursorPos(int row, int col);

    void eraseDisplay(int mode);
    void eraseLine(int mode);

    void scrollUp(int n);
    void scrollDown(int n);

    void insertLines(int n);
    void deleteLines(int n);
    void deleteChars(int n);
    void insertChars(int n);

    void applySGR(const QVector<int> &params);

    void newline();
    void scrollRegionUp(int n);
    void scrollRegionDown(int n);

    void pushToHistory(const QVector<Cell> &line);
    QVector<Cell> blankLine() const;

    Cell &cell(int x, int y);
    const Cell &constCell(int x, int y) const;
};
