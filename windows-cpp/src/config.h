#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

// Single-file settings: %APPDATA%\star_term\star-term-settings.json
// Sessions kept separate: %APPDATA%\star_term\sessions.json
// Matches config.py exactly.

QString getAppDataDir();
QString getKnownHostsPath();

QJsonObject loadSettings();
void        saveSettings(const QJsonObject &settings);

QJsonArray  loadMacros();
void        saveMacros(const QJsonArray &macros);

QJsonArray  loadFolders();
void        saveFolders(const QJsonArray &folders);

QJsonArray  loadSessions();
void        saveSessions(const QJsonArray &sessions);
