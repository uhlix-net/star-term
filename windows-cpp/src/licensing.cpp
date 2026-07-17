#include "licensing.h"
#include "config.h"

#include <QByteArray>
#include <QDate>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

#include <openssl/evp.h>
#include <openssl/err.h>

#ifdef _WIN32
#include <windows.h>
#include <winreg.h>
#endif

static const char* REGISTRY_KEY_PATH = "Software\\StarTerm";

// -----------------------------------------------------------------------
// Base64url → standard base64
// -----------------------------------------------------------------------
static QByteArray base64urlDecode(const QString &input) {
    QString s = input;
    s.replace('-', '+');
    s.replace('_', '/');
    // Add padding
    int mod = s.size() % 4;
    if (mod == 2)      s += "==";
    else if (mod == 3) s += "=";
    return QByteArray::fromBase64(s.toUtf8());
}

// -----------------------------------------------------------------------
// parseLicenseKey
// Layout: base64url( uint16BE(payloadLen) || payload_json || sig_64bytes )
// -----------------------------------------------------------------------
QJsonObject parseLicenseKey(const QString &keyStr) {
    // Remove whitespace
    QString cleaned = keyStr.simplified().remove(' ');

    QByteArray token = base64urlDecode(cleaned);
    if (token.size() < 2) return {};

    quint16 payloadLen = (static_cast<quint8>(token[0]) << 8) |
                          static_cast<quint8>(token[1]);

    if (token.size() < 2 + payloadLen + 64) return {};

    const unsigned char *payloadBytes =
        reinterpret_cast<const unsigned char*>(token.constData() + 2);
    const unsigned char *sig =
        reinterpret_cast<const unsigned char*>(token.constData() + 2 + payloadLen);

    // Parse public key from hex
    QByteArray pubKeyBytes = QByteArray::fromHex(PUBLIC_KEY_HEX);
    if (pubKeyBytes.size() != 32) return {};

    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const unsigned char*>(pubKeyBytes.constData()), 32
    );
    if (!pkey) return {};

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return {};
    }

    bool ok = false;
    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1) {
        int result = EVP_DigestVerify(ctx, sig, 64, payloadBytes, payloadLen);
        ok = (result == 1);
    }

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    if (!ok) return {};

    QByteArray payloadJson(reinterpret_cast<const char*>(payloadBytes), payloadLen);
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payloadJson, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};

    QJsonObject payload = doc.object();
    if (payload.value("product").toString() != QString(PRODUCT)) return {};

    return payload;
}

// -----------------------------------------------------------------------
// getLicenseInfo
// -----------------------------------------------------------------------
QJsonObject getLicenseInfo() {
    QString keyStr = loadSettings().value("license_key").toString();
    if (keyStr.isEmpty()) return {};
    return parseLicenseKey(keyStr);
}

// -----------------------------------------------------------------------
// saveLicenseKey
// -----------------------------------------------------------------------
QJsonObject saveLicenseKey(const QString &keyStr) {
    QJsonObject info = parseLicenseKey(keyStr);
    if (info.isEmpty()) return {};

    QJsonObject settings = loadSettings();
    settings["license_key"] = keyStr.trimmed();
    saveSettings(settings);
    return info;
}

// -----------------------------------------------------------------------
// clearLicenseKey
// -----------------------------------------------------------------------
void clearLicenseKey() {
    QJsonObject settings = loadSettings();
    if (settings.contains("license_key")) {
        settings.remove("license_key");
        saveSettings(settings);
    }
}

// -----------------------------------------------------------------------
// Trial marker: Windows registry on Win32, JSON file elsewhere
// -----------------------------------------------------------------------
struct TrialMarker {
    QString trialStart;
    QString lastSeen;
};

static TrialMarker readTrialMarker() {
    TrialMarker m;
#ifdef _WIN32
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REGISTRY_KEY_PATH, 0, KEY_READ, &hKey)
            == ERROR_SUCCESS) {
        char buf[64] = {};
        DWORD len = sizeof(buf);
        DWORD type = REG_SZ;
        if (RegQueryValueExA(hKey, "TrialStart", nullptr, &type,
                             reinterpret_cast<LPBYTE>(buf), &len) == ERROR_SUCCESS) {
            m.trialStart = QString::fromUtf8(buf);
        }
        len = sizeof(buf); type = REG_SZ;
        memset(buf, 0, sizeof(buf));
        if (RegQueryValueExA(hKey, "LastSeen", nullptr, &type,
                             reinterpret_cast<LPBYTE>(buf), &len) == ERROR_SUCCESS) {
            m.lastSeen = QString::fromUtf8(buf);
        }
        RegCloseKey(hKey);
    }
#else
    QString path = getAppDataDir() + "/trial_marker.json";
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            m.trialStart = obj.value("trial_start").toString();
            m.lastSeen   = obj.value("last_seen").toString();
        }
        f.close();
    }
#endif
    return m;
}

static void writeTrialMarker(const TrialMarker &m) {
#ifdef _WIN32
    HKEY hKey = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REGISTRY_KEY_PATH, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                        &hKey, nullptr) == ERROR_SUCCESS) {
        if (!m.trialStart.isEmpty()) {
            QByteArray ts = m.trialStart.toUtf8();
            RegSetValueExA(hKey, "TrialStart", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(ts.constData()),
                           static_cast<DWORD>(ts.size() + 1));
        }
        if (!m.lastSeen.isEmpty()) {
            QByteArray ls = m.lastSeen.toUtf8();
            RegSetValueExA(hKey, "LastSeen", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(ls.constData()),
                           static_cast<DWORD>(ls.size() + 1));
        }
        RegCloseKey(hKey);
    }
#else
    QString path = getAppDataDir() + "/trial_marker.json";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonObject obj;
        obj["trial_start"] = m.trialStart;
        obj["last_seen"]   = m.lastSeen;
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();
    }
#endif
}

struct TrialStatus {
    int  daysRemaining;
    bool expired;
};

static TrialStatus getTrialStatus() {
    QDate today = QDate::currentDate();
    TrialMarker marker = readTrialMarker();

    QDate trialStart = marker.trialStart.isEmpty()
        ? today
        : QDate::fromString(marker.trialStart, Qt::ISODate);
    if (!trialStart.isValid()) trialStart = today;

    QDate lastSeen = marker.lastSeen.isEmpty()
        ? today
        : QDate::fromString(marker.lastSeen, Qt::ISODate);
    if (!lastSeen.isValid()) lastSeen = today;

    bool clockRolledBack = today < lastSeen;
    int  daysElapsed     = trialStart.daysTo(today);
    int  daysRemaining   = qMax(0, TRIAL_DAYS - daysElapsed);
    bool expired         = clockRolledBack || (daysRemaining <= 0);

    // Write updated marker
    TrialMarker updated;
    updated.trialStart = trialStart.toString(Qt::ISODate);
    updated.lastSeen   = (today > lastSeen ? today : lastSeen).toString(Qt::ISODate);
    writeTrialMarker(updated);

    return { clockRolledBack ? 0 : daysRemaining, expired };
}

// -----------------------------------------------------------------------
// getLicenseStatus
// -----------------------------------------------------------------------
LicenseStatus getLicenseStatus() {
    LicenseStatus s;
    s.licenseInfo  = getLicenseInfo();
    s.licensed     = !s.licenseInfo.isEmpty();
    TrialStatus ts = getTrialStatus();
    s.trialDaysRemaining = ts.daysRemaining;
    s.trialExpired       = ts.expired;
    return s;
}
