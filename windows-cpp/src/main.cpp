#include <QApplication>
#include <QDialog>

#include "config.h"
#include "debug.h"
#include "icons.h"
#include "licensedialog.h"
#include "licensing.h"
#include "mainwindow.h"
#include "theme.h"

#ifdef _WIN32
#  include <windows.h>
#endif

// -----------------------------------------------------------------------
// Crash / unhandled exception handler (writes to debug.log)
// -----------------------------------------------------------------------
#include <exception>
#include <stdexcept>

static std::terminate_handler g_prevTerminate = nullptr;

static void terminateHandler() {
    try {
        std::rethrow_exception(std::current_exception());
    } catch (const std::exception &e) {
        debugLog(QString("Unhandled exception (terminate): %1").arg(e.what()));
    } catch (...) {
        debugLog("Unhandled unknown exception (terminate)");
    }
    if (g_prevTerminate) g_prevTerminate();
    std::abort();
}

// -----------------------------------------------------------------------
// main
// -----------------------------------------------------------------------
int main(int argc, char *argv[]) {
    // Windows: set AppUserModelID so Windows shows our icon on the taskbar
    // and groups the window separately from other processes.
#ifdef _WIN32
    typedef HRESULT (WINAPI *SetAppUserModelIDFn)(PCWSTR);
    HMODULE shell32 = LoadLibraryA("shell32.dll");
    if (shell32) {
        SetAppUserModelIDFn fn = reinterpret_cast<SetAppUserModelIDFn>(
            GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID"));
        if (fn) fn(L"uhlix.net.StarTermCpp");
        FreeLibrary(shell32);
    }
#endif

    // Install terminate handler for crash logging
    g_prevTerminate = std::set_terminate(terminateHandler);

    QApplication app(argc, argv);
    app.setApplicationName("Star Term");
    app.setApplicationVersion("0.3.0");
    app.setOrganizationName("uhlix.net");

    // Apply stylesheet (must happen after QApplication, before any pixmaps)
    QJsonObject settings = loadSettings();
    app.setStyleSheet(getStylesheet(settings.value("theme").toString("dark")));
    app.setWindowIcon(Icons::appIcon());

    // License gate
    LicenseStatus status = getLicenseStatus();
    if (!status.licensed && status.trialExpired) {
        LicenseGateDialog gate;
        if (gate.exec() != QDialog::Accepted) {
            return 0;
        }
    }

    MainWindow window;
    window.resize(1200, 800);
    window.show();

    return app.exec();
}
