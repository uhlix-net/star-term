#include "rdppane.h"
#include "config.h"
#include "debug.h"

#include <QAxObject>
#include <QAxWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

// ---------------------------------------------------------------------------
// The RDP ActiveX control ships with Windows in mstscax.dll.  Its coclasses are
// named MsRdpClient<N>[NotSafeForScripting] in the MSTSCLib type library, but
// those names are NOT registered as ProgIDs — the registered ProgIDs are
// MsTscAx.MsTscAx.<N>, whose version index is unrelated to the coclass number.
//
// Those ProgIDs resolve to the NotSafeForScripting coclasses, which is what we
// need: the script-safe variants refuse to accept a password through
// ClearTextPassword.  (Verified on Windows: MsTscAx.MsTscAx.13 ->
// {3F859AA3-C2D4-4FAA-B0E4-FD0C9C4E5E3A} "Microsoft RDP Client Control -
// version 13", with no safe-for-scripting implemented category.)
//
// Walk newest-first so the machine's most capable control wins; the upper bound
// is deliberately ahead of what ships today so future releases bind without a
// code change.
// ---------------------------------------------------------------------------
static const int RDP_PROGID_NEWEST = 16;
static const int RDP_PROGID_OLDEST = 2;

// Minimum remote desktop geometry the control will accept.
static const int MIN_SESSION_W = 640;
static const int MIN_SESSION_H = 480;

// ---------------------------------------------------------------------------

RdpPane::RdpPane(const QJsonObject &session, QWidget *parent)
    : QWidget(parent)
{
    name   = session.value("name").toString();
    m_host = session.value("host").toString();
    m_port = session.value("port").toInt(3389);
    m_user = session.value("username").toString();

    // "DOMAIN\user" is split out; the control wants the domain separately.
    int sep = m_user.indexOf('\\');
    if (sep > 0) {
        m_domain = m_user.left(sep);
        m_user   = m_user.mid(sep + 1);
    }

    m_status = new QLabel(QString("Connecting to %1...").arg(m_host));
    m_status->setAlignment(Qt::AlignCenter);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_status);
}

RdpPane::~RdpPane()
{
    m_userClosing = true;
    stopStatsPolling();
    destroyControl();
}

void RdpPane::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_initialized) {
        m_initialized = true;
        QTimer::singleShot(0, this, &RdpPane::connectToHost);
    }
}

void RdpPane::showStatus(const QString &text)
{
    m_status->setText(text);
    m_status->show();
}

// ---------------------------------------------------------------------------
// Credentials
// ---------------------------------------------------------------------------
bool RdpPane::promptForCredentials()
{
    QDialog credDlg(this);
    credDlg.setWindowTitle(QString("Connect to %1").arg(m_host));
    auto *form = new QFormLayout(&credDlg);

    QString shownUser = m_domain.isEmpty() ? m_user
                                           : QString("%1\\%2").arg(m_domain, m_user);
    auto *userEdit = new QLineEdit(shownUser, &credDlg);
    auto *passEdit = new QLineEdit(&credDlg);
    passEdit->setEchoMode(QLineEdit::Password);
    form->addRow("Username:", userEdit);
    form->addRow("Password:", passEdit);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &credDlg);
    form->addRow(btns);
    connect(btns, &QDialogButtonBox::accepted, &credDlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &credDlg, &QDialog::reject);

    // The control reports a missing credential only after a failed logon round
    // trip, so require both up front rather than connecting to be refused.
    // A bare "DOMAIN\" leaves no user name and is rejected the same way.
    QPushButton *okBtn = btns->button(QDialogButtonBox::Ok);
    auto updateOkState = [okBtn, userEdit, passEdit]() {
        const QString user = userEdit->text().trimmed();
        const bool haveUser = !user.isEmpty() && !user.endsWith('\\');
        okBtn->setEnabled(haveUser && !passEdit->text().isEmpty());
    };
    connect(userEdit, &QLineEdit::textChanged, &credDlg, updateOkState);
    connect(passEdit, &QLineEdit::textChanged, &credDlg, updateOkState);
    updateOkState();

    if (credDlg.exec() != QDialog::Accepted) return false;

    QString entered = userEdit->text();
    int sep = entered.indexOf('\\');
    if (sep > 0) {
        m_domain = entered.left(sep);
        m_user   = entered.mid(sep + 1);
    } else {
        m_domain.clear();
        m_user = entered;
    }
    m_cachedPass = passEdit->text();
    return true;
}

// ---------------------------------------------------------------------------
// Control lifecycle
// ---------------------------------------------------------------------------
QSize RdpPane::sessionPixelSize() const
{
    const qreal dpr = devicePixelRatioF();
    int w = qRound(width()  * dpr);
    int h = qRound(height() * dpr);
    return QSize(qMax(w, MIN_SESSION_W), qMax(h, MIN_SESSION_H));
}

QAxObject *RdpPane::advancedSettings()
{
    if (m_advanced) return m_advanced;
    if (!m_ax)      return nullptr;

    // Each AdvancedSettings<N> revision is a superset of the previous one, so
    // binding the highest available exposes the widest set of properties.
    for (int v = 9; v >= 2; --v) {
        QByteArray prop = QString("AdvancedSettings%1").arg(v).toLatin1();
        if (QAxObject *o = m_ax->querySubObject(prop.constData())) {
            m_advanced = o;
            return m_advanced;
        }
    }
    m_advanced = m_ax->querySubObject("AdvancedSettings");
    return m_advanced;
}

bool RdpPane::createControl()
{
    destroyControl();

    m_ax = new QAxWidget(this);
    // Accept click and tab focus so keystrokes reach the remote session.
    m_ax->setFocusPolicy(Qt::StrongFocus);

    bool bound = false;
    for (int v = RDP_PROGID_NEWEST; v >= RDP_PROGID_OLDEST && !bound; --v) {
        QString progId = QString("MsTscAx.MsTscAx.%1").arg(v);
        if (m_ax->setControl(progId)) {
            debugLog(QString("RDP: bound ActiveX control %1").arg(progId));
            bound = true;
        }
    }
    // Version-independent ProgID as a last resort.
    if (!bound && m_ax->setControl(QStringLiteral("MsTscAx.MsTscAx"))) {
        debugLog("RDP: bound ActiveX control MsTscAx.MsTscAx");
        bound = true;
    }
    if (!bound) {
        debugLog("RDP: no MsRdpClient ActiveX control could be instantiated");
        m_ax->deleteLater();
        m_ax = nullptr;
        return false;
    }

    m_layout->addWidget(m_ax);

    // Event wiring.  ActiveQt builds the metaobject from the type library, so
    // these are matched by signature at runtime; log any that fail to bind so a
    // control-version mismatch is diagnosable from debug.log.
    struct { const char *sig; const char *slot; } events[] = {
        { SIGNAL(OnConnected()),          SLOT(onAxConnected())        },
        { SIGNAL(OnLoginComplete()),      SLOT(onAxLoginComplete())    },
        { SIGNAL(OnDisconnected(int)),    SLOT(onAxDisconnected(int))  },
        { SIGNAL(OnLogonError(int)),      SLOT(onAxLogonError(int))    },
        { SIGNAL(OnFatalError(int)),      SLOT(onAxFatalError(int))    },
    };
    for (const auto &e : events) {
        if (!connect(m_ax, e.sig, this, e.slot))
            debugLog(QString("RDP: failed to connect event %1").arg(e.sig));
    }
    return true;
}

void RdpPane::destroyControl()
{
    if (!m_ax) return;

    m_ax->disconnect(this);

    // Connected: 0 = disconnected, 1 = connected, 2 = connecting.
    if (m_ax->property("Connected").toInt() != 0)
        m_ax->dynamicCall("Disconnect()");

    m_advanced = nullptr;          // parented to m_ax, released with it
    m_ax->clear();                 // release the COM object
    m_layout->removeWidget(m_ax);
    m_ax->deleteLater();
    m_ax = nullptr;
}

// ---------------------------------------------------------------------------
// Connect
// ---------------------------------------------------------------------------
void RdpPane::connectToHost()
{
    m_userClosing = false;

    if (m_cachedPass.isEmpty() && !promptForCredentials()) {
        showStatus("Connection cancelled.");
        return;
    }

    // Covers credentials that arrived with a saved session rather than through
    // the prompt: fail here instead of handing the control an incomplete logon.
    if (m_user.isEmpty() || m_cachedPass.isEmpty()) {
        m_cachedPass.clear();
        showStatus("A username and password are both required for an RDP connection.");
        return;
    }

    showStatus(QString("Connecting to %1...").arg(m_host));

    if (!createControl()) {
        showStatus("The Remote Desktop ActiveX control (mstscax.dll) "
                   "is unavailable on this system.");
        return;
    }

    const QSize px = sessionPixelSize();

    m_ax->setProperty("Server",        m_host);
    m_ax->setProperty("UserName",      m_user);
    m_ax->setProperty("DesktopWidth",  px.width());
    m_ax->setProperty("DesktopHeight", px.height());
    if (!m_domain.isEmpty())
        m_ax->setProperty("Domain", m_domain);

    if (QAxObject *adv = advancedSettings()) {
        adv->setProperty("RDPPort",              m_port);
        adv->setProperty("ClearTextPassword",    m_cachedPass);
        // Scale the remote image to the tab while a resize is in flight; the
        // debounced UpdateSessionDisplaySettings then restores a 1:1 mapping.
        adv->setProperty("SmartSizing",          true);
        // NLA — required by default on current Windows Server builds.
        adv->setProperty("EnableCredSspSupport", true);
        // 2 = warn but allow the user to continue if server identity cannot be
        // verified, matching mstsc.exe's default behaviour.
        adv->setProperty("AuthenticationLevel",  2);
        adv->setProperty("DisplayConnectionBar", false);
        adv->setProperty("GrabFocusOnConnect",   true);
    } else {
        debugLog("RDP: AdvancedSettings unavailable; connecting with defaults");
    }

    m_ax->dynamicCall("Connect()");
}

void RdpPane::reconnect()
{
    destroyControl();
    QTimer::singleShot(0, this, &RdpPane::connectToHost);
}

// ---------------------------------------------------------------------------
// Control events
// ---------------------------------------------------------------------------
void RdpPane::onAxConnected()
{
    m_status->hide();
}

void RdpPane::onAxLoginComplete()
{
    m_status->hide();

    // Stats polling reuses the session credentials for remote WMI.
    m_statsHost = m_host;
    m_statsUser = m_domain.isEmpty() ? m_user
                                     : QString("%1\\%2").arg(m_domain, m_user);
    m_statsPass = m_cachedPass;
    startStatsPolling();
}

void RdpPane::onAxLogonError(int lError)
{
    // -1 bad password, -3 other logon failure.  Drop the cached password so the
    // next connect attempt re-prompts instead of silently failing again.
    if (lError == -1 || lError == -3)
        m_cachedPass.clear();
    debugLog(QString("RDP: logon error %1").arg(lError));
}

QString RdpPane::disconnectText(int discReason)
{
    QString detail;
    if (m_ax) {
        uint extended = m_ax->property("ExtendedDisconnectReason").toUInt();
        QVariant desc = m_ax->dynamicCall(
            "GetErrorDescription(uint,uint)", uint(discReason), extended);
        detail = desc.toString().trimmed();
    }
    if (detail.isEmpty())
        return QString("Disconnected from %1.").arg(m_host);
    return QString("Disconnected from %1: %2").arg(m_host, detail);
}

void RdpPane::onAxDisconnected(int discReason)
{
    stopStatsPolling();
    if (m_userClosing) return;
    // Hide rather than destroy: we are inside the control's own event dispatch,
    // and releasing the COM object here would re-enter it.  destroyControl()
    // runs later from reconnect() or disconnectRdp().
    if (m_ax) m_ax->hide();
    showStatus(disconnectText(discReason));
}

void RdpPane::onAxFatalError(int errorCode)
{
    stopStatsPolling();
    debugLog(QString("RDP: fatal error %1").arg(errorCode));
    if (m_userClosing) return;
    if (m_ax) m_ax->hide();
    showStatus(QString("Remote Desktop error %1 on %2.")
                   .arg(errorCode).arg(m_host));
}

bool RdpPane::focusNextPrevChild(bool)
{
    return false;
}

void RdpPane::disconnectRdp()
{
    m_userClosing = true;
    if (m_resizeTimer) m_resizeTimer->stop();
    stopStatsPolling();
    destroyControl();
}

// ---------------------------------------------------------------------------
// Resize — the control scales the image immediately (SmartSizing); once the
// drag settles we ask the server for a native-resolution desktop so text stays
// crisp.  UpdateSessionDisplaySettings needs RDP 8.1+ on both ends; when it is
// unavailable the call is a no-op and SmartSizing keeps the session usable.
// ---------------------------------------------------------------------------
void RdpPane::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_ax) return;
    if (event->size().width() <= 0 || event->size().height() <= 0) return;

    if (!m_resizeTimer) {
        m_resizeTimer = new QTimer(this);
        m_resizeTimer->setSingleShot(true);
        connect(m_resizeTimer, &QTimer::timeout, this, &RdpPane::applyPendingResize);
    }
    m_resizeTimer->start(500);
}

void RdpPane::applyPendingResize()
{
    if (!m_ax) return;
    if (m_ax->property("Connected").toInt() != 1) return;

    const QSize px = sessionPixelSize();
    // (width, height, physicalWidth, physicalHeight, orientation,
    //  desktopScaleFactor, deviceScaleFactor) — 0 physical size means
    //  "unspecified"; scale factors of 100 keep the server at 1:1.
    m_ax->dynamicCall("UpdateSessionDisplaySettings(uint,uint,uint,uint,uint,uint,uint)",
                      uint(px.width()), uint(px.height()),
                      uint(0), uint(0), uint(0),
                      uint(100), uint(100));
}

// ---------------------------------------------------------------------------
// Remote stats polling via WMI (processor queue length, RAM, page file)
// ---------------------------------------------------------------------------

void RdpPane::startStatsPolling() {
    if (m_statsTimer) return;
    m_statsTimer = new QTimer(this);
    connect(m_statsTimer, &QTimer::timeout, this, &RdpPane::pollStats);
    m_statsTimer->start(2000);
    pollStats();
}

void RdpPane::stopStatsPolling() {
    if (m_statsTimer) { m_statsTimer->stop(); m_statsTimer->deleteLater(); m_statsTimer = nullptr; }
    if (m_statsProcess) {
        m_statsProcess->disconnect(this);
        m_statsProcess->kill();
        m_statsProcess->deleteLater();
        m_statsProcess = nullptr;
    }
    lastStats = {};
}

void RdpPane::pollStats() {
    if (m_statsProcess && m_statsProcess->state() != QProcess::NotRunning) return;
    if (m_statsHost.isEmpty() || m_statsPass.isEmpty()) return;

    QString pass = m_statsPass;
    pass.replace("'", "''");  // escape single quotes for PowerShell string literal

    QString cmd = QString(
        "$pass = ConvertTo-SecureString '%1' -AsPlainText -Force; "
        "$cred = New-Object PSCredential('%2', $pass); "
        "$cpu = Get-WmiObject -ComputerName '%3' -Credential $cred "
            "-Class Win32_PerfFormattedData_PerfOS_Processor "
            "-Filter \"Name='_Total'\" -ErrorAction Stop; "
        "$sys = Get-WmiObject -ComputerName '%3' -Credential $cred "
            "-Class Win32_PerfFormattedData_PerfOS_System -ErrorAction Stop; "
        "$os = Get-WmiObject -ComputerName '%3' -Credential $cred "
            "-Class Win32_OperatingSystem -ErrorAction Stop; "
        "Write-Output \"$($cpu.PercentProcessorTime) $($sys.ProcessorQueueLength) "
            "$($os.FreePhysicalMemory) $($os.TotalVisibleMemorySize) "
            "$($os.FreeSpaceInPagingFiles) $($os.SizeStoredInPagingFiles)\""
    ).arg(pass, m_statsUser, m_statsHost);

    QProcess *proc = new QProcess(this);
#ifdef Q_OS_WIN
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= CREATE_NO_WINDOW;
    });
#endif
    connect(proc, &QProcess::finished, this,
            [this, proc](int exitCode, QProcess::ExitStatus) {
        if (m_statsProcess == proc) m_statsProcess = nullptr;
        if (exitCode != 0) { proc->deleteLater(); return; }

        QString output = proc->readAllStandardOutput().trimmed();
        proc->deleteLater();

        QStringList parts = output.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 6) return;

        bool ok;
        int       cpuPct   = parts[0].toInt(&ok);       if (!ok) return;
        int       procQ    = parts[1].toInt(&ok);        if (!ok) return;
        long long freeMem  = parts[2].toLongLong(&ok);  if (!ok) return;
        long long totalMem = parts[3].toLongLong(&ok);  if (!ok) return;
        long long freePF   = parts[4].toLongLong(&ok);  if (!ok) return;
        long long totalPF  = parts[5].toLongLong(&ok);  if (!ok) return;

        double ramPct = (totalMem > 0) ? (totalMem - freeMem) * 100.0 / totalMem : 0.0;
        double pfPct  = (totalPF  > 0) ? (totalPF  - freePF)  * 100.0 / totalPF  : 0.0;

        QJsonObject stats;
        stats["rdp"]   = true;
        stats["cpu"]   = cpuPct;
        stats["procq"] = procQ;
        stats["ram"]   = ramPct;
        stats["pf"]    = pfPct;
        lastStats = stats;
        emit statsReady(stats);
    });
    m_statsProcess = proc;
    proc->start("powershell.exe", {"-NonInteractive", "-NoProfile", "-Command", cmd});
}
