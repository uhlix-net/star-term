#include "secretstore.h"

#include <QByteArray>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#endif

namespace {

#ifdef Q_OS_WIN
// Shown by Windows in credential-audit tooling; not used as a key.
wchar_t kDescription[] = L"Star Term SSH key passphrase";

// Overwrite a buffer that held plaintext before it goes back to the allocator.
void wipe(QByteArray &buf) {
    if (!buf.isEmpty())
        SecureZeroMemory(buf.data(), static_cast<size_t>(buf.size()));
}
#endif

} // namespace

bool SecretStore::available() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QString SecretStore::protect(const QString &plain) {
    if (plain.isEmpty()) return QString();

#ifdef Q_OS_WIN
    QByteArray in = plain.toUtf8();
    DATA_BLOB blobIn;
    blobIn.cbData = static_cast<DWORD>(in.size());
    blobIn.pbData = reinterpret_cast<BYTE *>(in.data());

    DATA_BLOB blobOut = {0, nullptr};
    // CRYPTPROTECT_UI_FORBIDDEN: never block the UI thread on a prompt.
    const BOOL ok = CryptProtectData(&blobIn, kDescription, nullptr, nullptr,
                                     nullptr, CRYPTPROTECT_UI_FORBIDDEN, &blobOut);
    wipe(in);
    if (!ok) return QString();

    const QByteArray out(reinterpret_cast<const char *>(blobOut.pbData),
                         static_cast<int>(blobOut.cbData));
    LocalFree(blobOut.pbData);
    return QString::fromLatin1(out.toBase64());
#else
    // No DPAPI: store nothing rather than write the passphrase in the clear.
    return QString();
#endif
}

QString SecretStore::unprotect(const QString &stored) {
    if (stored.isEmpty()) return QString();

#ifdef Q_OS_WIN
    QByteArray raw = QByteArray::fromBase64(stored.toLatin1());
    if (raw.isEmpty()) return QString();

    DATA_BLOB blobIn;
    blobIn.cbData = static_cast<DWORD>(raw.size());
    blobIn.pbData = reinterpret_cast<BYTE *>(raw.data());

    DATA_BLOB blobOut = {0, nullptr};
    if (!CryptUnprotectData(&blobIn, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &blobOut))
        return QString();

    QByteArray plain(reinterpret_cast<const char *>(blobOut.pbData),
                     static_cast<int>(blobOut.cbData));
    SecureZeroMemory(blobOut.pbData, blobOut.cbData);
    LocalFree(blobOut.pbData);

    const QString result = QString::fromUtf8(plain);
    wipe(plain);
    return result;
#else
    return QString();
#endif
}
