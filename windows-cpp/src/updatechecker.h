#pragma once
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Checks the GitHub releases API for a newer version than currentVersion.
// checkAsync() fires once; results arrive via signals.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(const QString &currentVersion, QObject *parent = nullptr);
    void checkAsync();

signals:
    void updateAvailable(const QString &latestVersion, const QString &releaseUrl, const QString &downloadUrl);
    void checkFinished(bool updateAvailable, const QString &latestVersion, const QString &releaseUrl, const QString &downloadUrl);

private slots:
    void onReply(QNetworkReply *reply);

private:
    static int cmpVersion(const QString &a, const QString &b);

    QString                 m_currentVersion;
    QNetworkAccessManager  *m_nam;
};
