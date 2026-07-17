#pragma once
#include <QString>
#include <QMap>

// Light/Dark theme stylesheets. Matches theme.py exactly.

struct ThemeColors {
    QString BG;
    QString PANEL_BG;
    QString ELEVATED_BG;
    QString BORDER;
    QString TEXT;
    QString MUTED_TEXT;
    QString ACCENT;
    QString ACCENT_HOVER;
    QString SELECTION;
    QString INPUT_BG;
    QString BUTTON_BG;
    QString BUTTON_HOVER_BG;
    QString BUTTON_HOVER_BORDER;
    QString DISABLED_BG;
    QString ITEM_HOVER_BG;
    QString SCROLLBAR_HANDLE;
    QString SCROLLBAR_HANDLE_HOVER;
};

QString getStylesheet(const QString &name);
void    clearStylesheetCache();

// Returns the path to a cached down-arrow PNG for QSS url() references.
// Must be called after QApplication is constructed.
QString arrowIconPath(const QString &colorHex);
