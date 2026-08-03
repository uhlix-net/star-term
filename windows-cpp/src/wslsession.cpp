#include "wslsession.h"
#include "debug.h"

#include <QDir>
#include <QMutexLocker>
#include <QProcess>
#include <QSettings>

#include <vector>

#include <windows.h>

// ---------------------------------------------------------------------------
// ConPTY entry points are resolved at runtime.  They exist only on Windows 10
// 1809 and later, and binding them dynamically keeps the build independent of
// the SDK's _WIN32_WINNT level while letting us fail with a clear message on
// older systems instead of refusing to launch at all.
// ---------------------------------------------------------------------------
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#  define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

using HPCON_T = void *;
typedef HRESULT (WINAPI *CreatePseudoConsoleFn)(COORD, HANDLE, HANDLE, DWORD, HPCON_T *);
typedef HRESULT (WINAPI *ResizePseudoConsoleFn)(HPCON_T, COORD);
typedef void    (WINAPI *ClosePseudoConsoleFn)(HPCON_T);

struct ConPtyApi {
    CreatePseudoConsoleFn create = nullptr;
    ResizePseudoConsoleFn resize = nullptr;
    ClosePseudoConsoleFn  close  = nullptr;
    bool available() const { return create && resize && close; }
};

static const ConPtyApi &conPty() {
    static const ConPtyApi api = [] {
        ConPtyApi a;
        if (HMODULE k32 = GetModuleHandleW(L"kernel32.dll")) {
            a.create = reinterpret_cast<CreatePseudoConsoleFn>(
                GetProcAddress(k32, "CreatePseudoConsole"));
            a.resize = reinterpret_cast<ResizePseudoConsoleFn>(
                GetProcAddress(k32, "ResizePseudoConsole"));
            a.close  = reinterpret_cast<ClosePseudoConsoleFn>(
                GetProcAddress(k32, "ClosePseudoConsole"));
        }
        return a;
    }();
    return api;
}

// ---------------------------------------------------------------------------
// WSL helpers
// ---------------------------------------------------------------------------

static void hideConsole(QProcess &proc) {
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= CREATE_NO_WINDOW;
    });
}

// wsl.exe writes UTF-16LE.
static QString decodeWslOutput(const QByteArray &raw) {
    if (raw.isEmpty()) return {};
    QString s = QString::fromUtf16(
        reinterpret_cast<const char16_t *>(raw.constData()),
        static_cast<int>(raw.size() / 2));
    s.remove(QChar(0xFEFF));   // byte-order mark, if wsl.exe emitted one
    return s;
}

static QStringList runWslList(const QStringList &args) {
    QProcess proc;
    hideConsole(proc);
    proc.start("wsl.exe", args);
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        debugLog("WSL: listing timed out");
        return {};
    }
    if (proc.exitCode() != 0) return {};

    QStringList names;
    const QStringList lines = decodeWslOutput(proc.readAllStandardOutput()).split('\n');
    for (const QString &line : lines) {
        const QString name = line.trimmed();
        if (!name.isEmpty()) names << name;
    }
    return names;
}

QStringList wslDistributions() {
    return runWslList({"--list", "--quiet"});
}

QStringList wslRunningDistributions() {
    return runWslList({"--list", "--running", "--quiet"});
}

QString wslDefaultDistribution() {
    QSettings lxss(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Lxss",
        QSettings::NativeFormat);
    const QString guid = lxss.value("DefaultDistribution").toString();
    if (guid.isEmpty()) return {};
    return lxss.value(guid + "/DistributionName").toString();
}

bool wslStartDistribution(const QString &distro, QString *error) {
    QProcess proc;
    hideConsole(proc);
    // --exec skips the login shell, so this boots the distribution and exits
    // immediately rather than leaving an interactive shell behind.
    proc.start("wsl.exe", {"--distribution", distro, "--exec", "/bin/true"});

    if (!proc.waitForFinished(60000)) {
        proc.kill();
        if (error) *error = QString("Timed out waiting for %1 to start.").arg(distro);
        return false;
    }
    if (proc.exitCode() != 0) {
        if (error) {
            const QString msg = decodeWslOutput(proc.readAllStandardError()).trimmed();
            *error = msg.isEmpty()
                ? QString("wsl.exe exited with code %1.").arg(proc.exitCode())
                : msg;
        }
        return false;
    }
    return true;
}

QString wslFilesystemRoot(const QString &distro) {
    if (distro.isEmpty()) return {};
    // wsl.localhost is the current share; wsl$ is the older name and is still
    // served, so it covers builds that predate the rename.
    for (const QString &prefix : {QStringLiteral("//wsl.localhost/"),
                                  QStringLiteral("//wsl$/")}) {
        const QString root = prefix + distro;
        if (QDir(root).exists()) return root;
    }
    debugLog(QString("WSL: no reachable filesystem share for %1").arg(distro));
    return {};
}

QString wslHomeDirectory(const QString &distro) {
    QProcess proc;
    hideConsole(proc);
    // --exec runs printenv directly, so the distribution name and the arguments
    // are separate tokens and nothing has to survive a shell's quoting rules.
    proc.start("wsl.exe", {"--distribution", distro, "--exec", "printenv", "HOME"});

    if (!proc.waitForFinished(10000)) {
        proc.kill();
        return {};
    }
    if (proc.exitCode() != 0) return {};

    // Unlike wsl.exe's own listings, this is the child process's stdout, which
    // passes through as raw UTF-8 rather than UTF-16.
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

// ---------------------------------------------------------------------------
// WslSession
// ---------------------------------------------------------------------------

WslSession::WslSession(const QString &distro, int cols, int rows, QObject *parent)
    : TerminalSession(parent)
    , m_distro(distro)
    , m_cols(cols > 0 ? cols : 80)
    , m_rows(rows > 0 ? rows : 24)
{}

WslSession::~WslSession() {
    stop();
    if (!wait(5000)) {
        terminate();
        wait(1000);
    }
    // Safe now: the read loop has exited, so nothing else touches the handles.
    closeHandles();
}

bool WslSession::openPty(QString *error) {
    const ConPtyApi &api = conPty();
    if (!api.available()) {
        if (error)
            *error = "This version of Windows has no pseudo-console support "
                     "(ConPTY requires Windows 10 1809 or later).";
        return false;
    }

    HANDLE inRead = nullptr, inWrite = nullptr;
    HANDLE outRead = nullptr, outWrite = nullptr;

    if (!CreatePipe(&inRead, &inWrite, nullptr, 0)) {
        if (error) *error = "Failed to create the pseudo-console input pipe.";
        return false;
    }
    if (!CreatePipe(&outRead, &outWrite, nullptr, 0)) {
        CloseHandle(inRead);
        CloseHandle(inWrite);
        if (error) *error = "Failed to create the pseudo-console output pipe.";
        return false;
    }

    COORD size;
    size.X = static_cast<SHORT>(m_cols);
    size.Y = static_cast<SHORT>(m_rows);

    HPCON_T hpc = nullptr;
    const HRESULT hr = api.create(size, inRead, outWrite, 0, &hpc);

    // The pseudo-console keeps its own duplicates of the ends it was given.
    CloseHandle(inRead);
    CloseHandle(outWrite);

    if (FAILED(hr) || !hpc) {
        CloseHandle(inWrite);
        CloseHandle(outRead);
        if (error)
            *error = QString("CreatePseudoConsole failed (0x%1).")
                         .arg(static_cast<quint32>(hr), 8, 16, QChar('0'));
        return false;
    }

    // Attribute list carrying the pseudo-console through to the child.
    STARTUPINFOEXW si {};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attrSize));

    bool attrOk = si.lpAttributeList
        && InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrSize)
        && UpdateProcThreadAttribute(si.lpAttributeList, 0,
                                     PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                     hpc, sizeof(hpc), nullptr, nullptr);
    if (!attrOk) {
        if (si.lpAttributeList) HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        api.close(hpc);
        CloseHandle(inWrite);
        CloseHandle(outRead);
        if (error) *error = "Failed to prepare the pseudo-console process attributes.";
        return false;
    }

    // The distribution name must NOT be quoted. wsl.exe parses its own command
    // line and keeps surrounding quotes as part of the name, so
    //     --distribution "Ubuntu"   fails with WSL_E_DISTRO_NOT_FOUND
    //     --distribution Ubuntu     succeeds
    // (verified against wsl.exe directly).  A name containing spaces therefore
    // has no quoting form that works here.
    //
    // CreateProcessW may write to the command line buffer, so it must be mutable.
    const std::wstring cmd =
        QString("wsl.exe --distribution %1").arg(m_distro).toStdWString();
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    PROCESS_INFORMATION pi {};
    const BOOL launched = CreateProcessW(
        nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        nullptr, nullptr, &si.StartupInfo, &pi);

    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);

    if (!launched) {
        api.close(hpc);
        CloseHandle(inWrite);
        CloseHandle(outRead);
        if (error)
            *error = QString("Failed to launch wsl.exe for %1 (error %2).")
                         .arg(m_distro).arg(GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);

    QMutexLocker lock(&m_ioMutex);
    m_hPC     = hpc;
    m_inWrite = inWrite;
    m_outRead = outRead;
    m_process = pi.hProcess;
    return true;
}

void WslSession::run() {
    QString error;
    if (!openPty(&error)) {
        debugLog(QString("WSL: %1").arg(error));
        emit connectionError(error);
        return;
    }

    m_running = true;
    emit connected();

    // Cached outside the loop deliberately: stop() never closes this handle, it
    // closes the pseudo-console instead, which ends the read with EOF.  The
    // handle itself is only released in the destructor, after the thread joins.
    HANDLE out = nullptr;
    {
        QMutexLocker lock(&m_ioMutex);
        out = static_cast<HANDLE>(m_outRead);
    }

    char  buf[4096];
    DWORD n = 0;
    while (out && ReadFile(out, buf, sizeof(buf), &n, nullptr) && n > 0)
        emit dataReceived(QByteArray(buf, static_cast<int>(n)));

    m_running = false;
    emit connectionClosed();
}

void WslSession::send(const QByteArray &data) {
    QMutexLocker lock(&m_ioMutex);
    if (!m_inWrite || data.isEmpty()) return;
    DWORD written = 0;
    WriteFile(static_cast<HANDLE>(m_inWrite), data.constData(),
              static_cast<DWORD>(data.size()), &written, nullptr);
}

void WslSession::resize(int cols, int rows) {
    if (cols <= 0 || rows <= 0) return;
    QMutexLocker lock(&m_ioMutex);
    m_cols = cols;
    m_rows = rows;
    if (!m_hPC) return;
    COORD size;
    size.X = static_cast<SHORT>(cols);
    size.Y = static_cast<SHORT>(rows);
    conPty().resize(m_hPC, size);
}

void WslSession::stop() {
    m_running = false;

    HPCON_T hpc = nullptr;
    HANDLE  proc = nullptr;
    {
        QMutexLocker lock(&m_ioMutex);
        hpc  = m_hPC;      m_hPC     = nullptr;
        proc = static_cast<HANDLE>(m_process);
    }

    // Closing the pseudo-console tears down the shell and ends the pending
    // ReadFile in run(); it is the only safe way to unblock that thread.
    if (hpc) conPty().close(hpc);

    if (proc && WaitForSingleObject(proc, 2000) == WAIT_TIMEOUT)
        TerminateProcess(proc, 0);
}

void WslSession::closeHandles() {
    QMutexLocker lock(&m_ioMutex);
    if (m_inWrite) { CloseHandle(static_cast<HANDLE>(m_inWrite)); m_inWrite = nullptr; }
    if (m_outRead) { CloseHandle(static_cast<HANDLE>(m_outRead)); m_outRead = nullptr; }
    if (m_process) { CloseHandle(static_cast<HANDLE>(m_process)); m_process = nullptr; }
}
