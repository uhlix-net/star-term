#include "debug.h"
#include "config.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>

static const QString LOG_FILE = "debug.log";

bool debugIsEnabled() {
    QJsonObject s = loadSettings();
    return s.value("debug").toBool(false);
}

QString debugGetLogPath() {
    return getAppDataDir() + "/" + LOG_FILE;
}

void debugLog(const QString &msg) {
    if (!debugIsEnabled()) return;
    try {
        QFile f(debugGetLogPath());
        if (!f.open(QIODevice::Append | QIODevice::Text)) return;
        QTextStream out(&f);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) << " " << msg << "\n";
        f.close();
    } catch (...) {}
}
