#pragma once
#include <QString>
#include <QList>
#include <QMap>
#include <QStringList>

// -----------------------------------------------------------------------
// Terminal color theme definition
// -----------------------------------------------------------------------
struct TerminalColorTheme {
    QString name;
    QString fg;
    QString bg;
    QMap<QString, QString> palette;
};

inline QList<TerminalColorTheme> builtinTerminalThemes() {
    return {
        {"Default", "#AAAAAA", "#000000", {
            {"black","#000000"},{"red","#AA0000"},{"green","#00AA00"},{"yellow","#AA5500"},
            {"brown","#AA5500"},{"blue","#0000AA"},{"magenta","#AA00AA"},{"cyan","#00AAAA"},
            {"white","#AAAAAA"},{"brightblack","#555555"},{"brightred","#FF5555"},
            {"brightgreen","#55FF55"},{"brightyellow","#FFFF55"},{"brightbrown","#FFFF55"},
            {"brightblue","#5555FF"},{"brightmagenta","#FF55FF"},{"brightcyan","#55FFFF"},
            {"brightwhite","#FFFFFF"}}},
        {"Solarized Dark", "#839496", "#002B36", {
            {"black","#073642"},{"red","#DC322F"},{"green","#859900"},{"yellow","#B58900"},
            {"brown","#B58900"},{"blue","#268BD2"},{"magenta","#D33682"},{"cyan","#2AA198"},
            {"white","#EEE8D5"},{"brightblack","#002B36"},{"brightred","#CB4B16"},
            {"brightgreen","#586E75"},{"brightyellow","#657B83"},{"brightbrown","#657B83"},
            {"brightblue","#839496"},{"brightmagenta","#6C71C4"},{"brightcyan","#93A1A1"},
            {"brightwhite","#FDF6E3"}}},
        {"Solarized Light", "#657B83", "#FDF6E3", {
            {"black","#073642"},{"red","#DC322F"},{"green","#859900"},{"yellow","#B58900"},
            {"brown","#B58900"},{"blue","#268BD2"},{"magenta","#D33682"},{"cyan","#2AA198"},
            {"white","#EEE8D5"},{"brightblack","#002B36"},{"brightred","#CB4B16"},
            {"brightgreen","#586E75"},{"brightyellow","#657B83"},{"brightbrown","#657B83"},
            {"brightblue","#839496"},{"brightmagenta","#6C71C4"},{"brightcyan","#93A1A1"},
            {"brightwhite","#FDF6E3"}}},
        {"Dracula", "#F8F8F2", "#282A36", {
            {"black","#21222C"},{"red","#FF5555"},{"green","#50FA7B"},{"yellow","#F1FA8C"},
            {"brown","#F1FA8C"},{"blue","#BD93F9"},{"magenta","#FF79C6"},{"cyan","#8BE9FD"},
            {"white","#F8F8F2"},{"brightblack","#6272A4"},{"brightred","#FF6E6E"},
            {"brightgreen","#69FF94"},{"brightyellow","#FFFFA5"},{"brightbrown","#FFFFA5"},
            {"brightblue","#D6ACFF"},{"brightmagenta","#FF92DF"},{"brightcyan","#A4FFFF"},
            {"brightwhite","#FFFFFF"}}},
        {"Monokai", "#F8F8F2", "#272822", {
            {"black","#272822"},{"red","#F92672"},{"green","#A6E22E"},{"yellow","#F4BF75"},
            {"brown","#F4BF75"},{"blue","#66D9E8"},{"magenta","#AE81FF"},{"cyan","#A1EFE4"},
            {"white","#F8F8F2"},{"brightblack","#75715E"},{"brightred","#F92672"},
            {"brightgreen","#A6E22E"},{"brightyellow","#F4BF75"},{"brightbrown","#F4BF75"},
            {"brightblue","#66D9E8"},{"brightmagenta","#AE81FF"},{"brightcyan","#A1EFE4"},
            {"brightwhite","#F9F8F5"}}},
        {"Nord", "#D8DEE9", "#2E3440", {
            {"black","#3B4252"},{"red","#BF616A"},{"green","#A3BE8C"},{"yellow","#EBCB8B"},
            {"brown","#EBCB8B"},{"blue","#81A1C1"},{"magenta","#B48EAD"},{"cyan","#88C0D0"},
            {"white","#E5E9F0"},{"brightblack","#4C566A"},{"brightred","#BF616A"},
            {"brightgreen","#A3BE8C"},{"brightyellow","#EBCB8B"},{"brightbrown","#EBCB8B"},
            {"brightblue","#81A1C1"},{"brightmagenta","#B48EAD"},{"brightcyan","#8FBCBB"},
            {"brightwhite","#ECEFF4"}}},
        {"One Dark", "#ABB2BF", "#282C34", {
            {"black","#282C34"},{"red","#E06C75"},{"green","#98C379"},{"yellow","#E5C07B"},
            {"brown","#E5C07B"},{"blue","#61AFEF"},{"magenta","#C678DD"},{"cyan","#56B6C2"},
            {"white","#ABB2BF"},{"brightblack","#5C6370"},{"brightred","#E06C75"},
            {"brightgreen","#98C379"},{"brightyellow","#E5C07B"},{"brightbrown","#E5C07B"},
            {"brightblue","#61AFEF"},{"brightmagenta","#C678DD"},{"brightcyan","#56B6C2"},
            {"brightwhite","#FFFFFF"}}},
        {"Gruvbox Dark", "#EBDBB2", "#282828", {
            {"black","#282828"},{"red","#CC241D"},{"green","#98971A"},{"yellow","#D79921"},
            {"brown","#D79921"},{"blue","#458588"},{"magenta","#B16286"},{"cyan","#689D6A"},
            {"white","#A89984"},{"brightblack","#928374"},{"brightred","#FB4934"},
            {"brightgreen","#B8BB26"},{"brightyellow","#FABD2F"},{"brightbrown","#FABD2F"},
            {"brightblue","#83A598"},{"brightmagenta","#D3869B"},{"brightcyan","#8EC07C"},
            {"brightwhite","#EBDBB2"}}},
        {"Campbell", "#CCCCCC", "#0C0C0C", {
            {"black","#0C0C0C"},{"red","#C50F1F"},{"green","#13A10E"},{"yellow","#C19C00"},
            {"brown","#C19C00"},{"blue","#0037DA"},{"magenta","#881798"},{"cyan","#3A96DD"},
            {"white","#CCCCCC"},{"brightblack","#767676"},{"brightred","#E74856"},
            {"brightgreen","#16C60C"},{"brightyellow","#F9F1A5"},{"brightbrown","#F9F1A5"},
            {"brightblue","#3B78FF"},{"brightmagenta","#B4009E"},{"brightcyan","#61D6D6"},
            {"brightwhite","#F2F2F2"}}},
        {"PowerShell", "#CCCCCC", "#012456", {
            {"black","#012456"},{"red","#C50F1F"},{"green","#13A10E"},{"yellow","#C19C00"},
            {"brown","#C19C00"},{"blue","#0037DA"},{"magenta","#881798"},{"cyan","#3A96DD"},
            {"white","#CCCCCC"},{"brightblack","#767676"},{"brightred","#E74856"},
            {"brightgreen","#16C60C"},{"brightyellow","#F9F1A5"},{"brightbrown","#F9F1A5"},
            {"brightblue","#3B78FF"},{"brightmagenta","#B4009E"},{"brightcyan","#61D6D6"},
            {"brightwhite","#F2F2F2"}}},
    };
}

// -----------------------------------------------------------------------
// Active (mutable) global palette — updated by setColorTheme()
// -----------------------------------------------------------------------
inline QString DEFAULT_FG = "#AAAAAA";
inline QString DEFAULT_BG = "#000000";
inline QMap<QString,QString> g_termPalette = {
    {"black","#000000"},{"red","#AA0000"},{"green","#00AA00"},{"yellow","#AA5500"},
    {"brown","#AA5500"},{"blue","#0000AA"},{"magenta","#AA00AA"},{"cyan","#00AAAA"},
    {"white","#AAAAAA"},{"brightblack","#555555"},{"brightred","#FF5555"},
    {"brightgreen","#55FF55"},{"brightyellow","#FFFF55"},{"brightbrown","#FFFF55"},
    {"brightblue","#5555FF"},{"brightmagenta","#FF55FF"},{"brightcyan","#55FFFF"},
    {"brightwhite","#FFFFFF"}
};

inline void setColorTheme(const QString &name) {
    for (const TerminalColorTheme &t : builtinTerminalThemes()) {
        if (t.name == name) {
            DEFAULT_FG    = t.fg;
            DEFAULT_BG    = t.bg;
            g_termPalette = t.palette;
            return;
        }
    }
}

inline QStringList availableThemeNames() {
    QStringList names;
    for (const TerminalColorTheme &t : builtinTerminalThemes())
        names << t.name;
    return names;
}

// -----------------------------------------------------------------------
// Named-color lookup (uses active theme palette)
// -----------------------------------------------------------------------
inline QString paletteColor(const QString &name) {
    if (name == "default" || name.isEmpty()) return QString();
    return g_termPalette.value(name, QString());
}

// -----------------------------------------------------------------------
// 256-color xterm palette: indices 0-15 are standard ANSI,
// 16-231 are a 6x6x6 color cube, 232-255 are grayscale.
// -----------------------------------------------------------------------
inline QString palette256(int idx) {
    if (idx < 0 || idx > 255) return DEFAULT_FG;

    if (idx < 16) {
        static const char* ansi16[] = {
            "#000000","#AA0000","#00AA00","#AA5500",
            "#0000AA","#AA00AA","#00AAAA","#AAAAAA",
            "#555555","#FF5555","#55FF55","#FFFF55",
            "#5555FF","#FF55FF","#55FFFF","#FFFFFF"
        };
        return QString(ansi16[idx]);
    }

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

    int level = 8 + (idx - 232) * 10;
    return QString("#%1%1%1").arg(level, 2, 16, QChar('0'));
}
