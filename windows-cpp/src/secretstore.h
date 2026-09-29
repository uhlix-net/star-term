#pragma once
#include <QString>

// Reversible protection for the small secrets Star Term keeps on disk —
// currently only SSH key passphrases in sessions.json.
//
// Backed by Windows DPAPI (CryptProtectData), so the stored blob is readable
// only by the same Windows user on the same machine. It is not a substitute
// for a key manager: anything running as that user can undo it. It exists so
// that sessions.json, which is a plain file in %APPDATA% and is exportable,
// never contains a passphrase in clear text.
//
// Both calls fail closed: on any error they return an empty string rather than
// falling back to plaintext.
namespace SecretStore {

// Plaintext -> base64-encoded protected blob. Empty in, empty out.
QString protect(const QString &plain);

// Base64-encoded protected blob -> plaintext. Empty if it cannot be read,
// which is what happens to a session profile copied from another machine or
// another Windows account.
QString unprotect(const QString &stored);

// False on platforms with no DPAPI, where nothing is ever written.
bool available();

} // namespace SecretStore
