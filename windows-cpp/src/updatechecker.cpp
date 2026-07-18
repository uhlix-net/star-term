#include "updatechecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

static const char *RELEASES_URL =
    "https://api.github.com/repos/uhlix-net/star-term/releases/latest";

UpdateChecker::UpdateChecker(const QString &currentVersion, QObject *parent)
    : QObject(parent)
    , m_currentVersion(currentVersion)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this,  &UpdateChecker::onReply);
}

void UpdateChecker::checkAsync() {
    QNetworkRequest req;
    req.setUrl(QUrl(QString::fromLatin1(RELEASES_URL)));
    req.setRawHeader("User-Agent", "star-term-update-check");
    m_nam->get(req);
}

void UpdateChecker::onReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString tagName    = obj.value("tag_name").toString();
    QString releaseUrl = obj.value("html_url").toString();
    if (tagName.isEmpty()) return;

    if (cmpVersion(tagName, m_currentVersion) > 0)
        emit updateAvailable(tagName, releaseUrl);
}

// Returns >0 if a is newer than b, 0 if equal, <0 if older.
int UpdateChecker::cmpVersion(const QString &a, const QString &b) {
    auto strip = [](const QString &v) {
        return QString(v).remove(QRegularExpression("^[vV]")).split('.');
    };
    QStringList pa = strip(a), pb = strip(b);
    int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        int va = (i < pa.size()) ? pa[i].toInt() : 0;
        int vb = (i < pb.size()) ? pb[i].toInt() : 0;
        if (va != vb) return va - vb;
    }
    return 0;
}
