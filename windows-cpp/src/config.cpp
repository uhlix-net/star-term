#include "config.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStandardPaths>

static const QString APP_DIR_NAME   = "star_term";
static const QString SESSIONS_FILE  = "sessions.json";
static const QString SETTINGS_FILE  = "star-term-settings.json";
static const QString KNOWN_HOSTS_FILE = "known_hosts";

// Legacy per-purpose filenames (migrated into SETTINGS_FILE on first load).
static const QMap<QString,QString> LEGACY_FILES = {
    {"settings", "settings.json"},
    {"folders",  "folders.json"},
    {"macros",   "macros.json"},
};

// -----------------------------------------------------------------------
// getAppDataDir
// -----------------------------------------------------------------------
QString getAppDataDir() {
    QString appdata = qEnvironmentVariable("APPDATA");
    QString base;
    if (!appdata.isEmpty()) {
        base = appdata;
    } else {
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        // Strip the Qt-appended app name subfolder if present
        QDir d(base);
        d.cdUp();
        base = d.absolutePath();
    }
    QString path = base + "/" + APP_DIR_NAME;
    QDir().mkpath(path);
    return path;
}

QString getKnownHostsPath() {
    return getAppDataDir() + "/" + KNOWN_HOSTS_FILE;
}

// -----------------------------------------------------------------------
// Internal combined-file helpers
// -----------------------------------------------------------------------
static QJsonObject loadFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}

static QJsonArray loadArrayFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError) return {};
    if (doc.isArray()) return doc.array();
    return {};
}

static void saveJsonFile(const QString &path, const QJsonDocument &doc) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
}

static QJsonObject loadCombined() {
    QString dir  = getAppDataDir();
    QString path = dir + "/" + SETTINGS_FILE;

    if (QFile::exists(path)) {
        return loadFile(path);
    }

    // Migration: look for legacy per-purpose files
    QJsonObject combined;
    combined["settings"] = QJsonObject();
    combined["folders"]  = QJsonArray();
    combined["macros"]   = QJsonArray();

    bool migrated = false;
    for (auto it = LEGACY_FILES.cbegin(); it != LEGACY_FILES.cend(); ++it) {
        QString legacyPath = dir + "/" + it.value();
        if (!QFile::exists(legacyPath)) continue;

        if (it.key() == "settings") {
            combined["settings"] = loadFile(legacyPath);
        } else {
            combined[it.key()] = loadArrayFile(legacyPath);
        }
        migrated = true;
    }

    if (migrated) {
        QJsonDocument doc(combined);
        saveJsonFile(path, doc);
        for (auto it = LEGACY_FILES.cbegin(); it != LEGACY_FILES.cend(); ++it) {
            QFile::remove(dir + "/" + it.value());
        }
    }

    return combined;
}

static void saveCombined(const QJsonObject &data) {
    QString path = getAppDataDir() + "/" + SETTINGS_FILE;
    saveJsonFile(path, QJsonDocument(data));
}

// -----------------------------------------------------------------------
// Sessions (kept in their own file, matching Python behaviour)
// -----------------------------------------------------------------------
QJsonArray loadSessions() {
    QString path = getAppDataDir() + "/" + SESSIONS_FILE;
    return loadArrayFile(path);
}

void saveSessions(const QJsonArray &sessions) {
    QString path = getAppDataDir() + "/" + SESSIONS_FILE;
    saveJsonFile(path, QJsonDocument(sessions));
}

// -----------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------
QJsonObject loadSettings() {
    return loadCombined().value("settings").toObject();
}

void saveSettings(const QJsonObject &settings) {
    QJsonObject combined = loadCombined();
    combined["settings"] = settings;
    saveCombined(combined);
}

// -----------------------------------------------------------------------
// Macros
// -----------------------------------------------------------------------
QJsonArray loadMacros() {
    return loadCombined().value("macros").toArray();
}

void saveMacros(const QJsonArray &macros) {
    QJsonObject combined = loadCombined();
    combined["macros"] = macros;
    saveCombined(combined);
}

// -----------------------------------------------------------------------
// Folders
// -----------------------------------------------------------------------
QJsonArray loadFolders() {
    return loadCombined().value("folders").toArray();
}

void saveFolders(const QJsonArray &folders) {
    QJsonObject combined = loadCombined();
    combined["folders"] = folders;
    saveCombined(combined);
}
