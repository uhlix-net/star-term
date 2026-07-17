#pragma once
// Linux standard console (VGA text-mode) 16-color palette.
// Matches colors.py exactly.

#include <QString>
#include <QMap>

inline const QString DEFAULT_FG = "#AAAAAA";
inline const QString DEFAULT_BG = "#000000";

// Returns hex color for a named color, or empty string for "default"
inline QString paletteColor(const QString &name) {
    static const QMap<QString, QString> PALETTE = {
        {"black",        "#000000"},
        {"red",          "#AA0000"},
        {"green",        "#00AA00"},
        {"brown",        "#AA5500"},
        {"yellow",       "#AA5500"},
        {"blue",         "#0000AA"},
        {"magenta",      "#AA00AA"},
        {"cyan",         "#00AAAA"},
        {"white",        "#AAAAAA"},
        {"brightblack",  "#555555"},
        {"brightred",    "#FF5555"},
        {"brightgreen",  "#55FF55"},
        {"brightbrown",  "#FFFF55"},
        {"brightyellow", "#FFFF55"},
        {"brightblue",   "#5555FF"},
        {"brightmagenta","#FF55FF"},
        {"brightcyan",   "#55FFFF"},
        {"brightwhite",  "#FFFFFF"},
    };
    if (name == "default" || name.isEmpty()) return QString();
    return PALETTE.value(name, QString());
}

// 256-color xterm palette: indices 0-15 are standard colors,
// 16-231 are a 6x6x6 color cube, 232-255 are grayscale.
inline QString palette256(int idx) {
    if (idx < 0 || idx > 255) return DEFAULT_FG;

    // First 16: standard ANSI colors
    static const char* ansi16[] = {
        "#000000","#AA0000","#00AA00","#AA5500",
        "#0000AA","#AA00AA","#00AAAA","#AAAAAA",
        "#555555","#FF5555","#55FF55","#FFFF55",
        "#5555FF","#FF55FF","#55FFFF","#FFFFFF"
    };
    if (idx < 16) return QString(ansi16[idx]);

    // 16-231: 6x6x6 color cube
    if (idx < 232) {
        int n = idx - 16;
        int b = n % 6;
        int g = (n / 6) % 6;
        int r = n / 36;
        auto cv = [](int v) { return v == 0 ? 0 : 55 + v * 40; };
        return QString("#%1%2%3")
            .arg(cv(r), 2, 16, QChar('0'))
            .arg(cv(g), 2, 16, QChar('0'))
            .arg(cv(b), 2, 16, QChar('0'));
    }

    // 232-255: grayscale
    int level = 8 + (idx - 232) * 10;
    return QString("#%1%1%1").arg(level, 2, 16, QChar('0'));
}
