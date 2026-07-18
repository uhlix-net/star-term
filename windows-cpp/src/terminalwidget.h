#pragma once
#include <QWidget>
#include <QFont>
#include <QScrollBar>
#include <QSize>

#include "vt100.h"

// Matches terminal_widget.py (TerminalWidget) exactly.

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(
        int cols = 80, int rows = 24,
        const QString &fontFamily = "Courier New",
        int fontSize = 10,
        const QString &cursorStyle = "underline",
        QWidget *parent = nullptr);

    void  feed(const QByteArray &data);
    void  applySettings(const QString &fontFamily, int fontSize, const QString &cursorStyle);
    void  scrollLines(int lines);

    QSize sizeHint() const override;

    VT100Screen &screen() { return m_screen; }

signals:
    void dataToSend(const QByteArray &data);
    void sizeChanged(int cols, int rows);

protected:
    bool  focusNextPrevChild(bool next) override;
    void  paintEvent(QPaintEvent *event) override;
    void  keyPressEvent(QKeyEvent *event) override;
    void  resizeEvent(QResizeEvent *event) override;
    void  wheelEvent(QWheelEvent *event) override;
    void  mousePressEvent(QMouseEvent *event) override;
    void  mouseMoveEvent(QMouseEvent *event) override;
    void  mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onScrollbarChanged(int value);

private:
    void       setFont_(const QString &fontFamily, int fontSize);
    void       updateScrollbar();
    QByteArray keyToBytes(QKeyEvent *event);

    // Mouse selection helpers
    QPair<int,int> cellFromPos(const QPoint &pos) const;
    bool           cellInSelection(int col, int row) const;
    QString        selectedText() const;
    void           clearSelection();

    // Selection state (all in display-row coords, 0..rows-1)
    bool m_selecting   = false;
    bool m_selActive   = false;
    int  m_selAnchorRow = 0, m_selAnchorCol = 0;
    int  m_selEndRow    = 0, m_selEndCol    = 0;

    VT100Screen m_screen;
    QString     m_cursorStyle;
    QFont       m_font;
    int         m_charWidth  = 8;
    int         m_charHeight = 16;
    int         m_scrollbarWidth = 12;

    QScrollBar *m_scrollbar = nullptr;

    static const int PADDING    = 6;
    static const int MIN_COLS   = 10;
    static const int MIN_ROWS   = 2;
    static const int HISTORY_LINES = 2000;
};
