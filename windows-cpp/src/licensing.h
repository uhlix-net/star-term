#pragma once
#include <QString>
#include <QJsonObject>

// Offline Ed25519 license verification + 30-day trial.
// Matches licensing.py exactly.

static const char* PRODUCT    = "star_term";
static const int   TRIAL_DAYS = 30;
static const char* PUBLIC_KEY_HEX =
    "4d055cd85dfd9c1849759c3c596b65ad99b1aee31103c6c43ea5bc98537697e6";

struct LicenseStatus {
    bool       licensed          = false;
    QJsonObject licenseInfo;       // empty if !licensed
    int        trialDaysRemaining = 0;
    bool       trialExpired       = false;
};

// Returns parsed JSON payload if valid, empty object if invalid/wrong product.
QJsonObject parseLicenseKey(const QString &keyStr);

// Verifies + stores key; returns payload on success, empty on failure.
QJsonObject saveLicenseKey(const QString &keyStr);

void clearLicenseKey();

// Returns the stored license info if valid, or empty object.
QJsonObject getLicenseInfo();

LicenseStatus getLicenseStatus();

// Restart the 30-day trial from today.
//
// TEMPORARY — pre-1.0 only. This exists so the trial can be re-armed during
// development and demos; it makes the trial unlimited in practice, since it is
// reachable from the expiry dialog with no gating. Remove this, its button in
// LicenseGateDialog, and TRIAL_REARM_NOTICE before the 1.0 release.
void resetTrial();

// Shown wherever the re-arm control appears, so the temporary nature of the
// feature is visible in the product and not only in the source.
static const char* TRIAL_REARM_NOTICE =
    "Resetting the trial is a pre-release convenience and is expected to be "
    "removed in the 1.0 release.";
