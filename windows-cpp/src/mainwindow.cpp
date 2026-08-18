#include "mainwindow.h"
#include "colors.h"
#include "config.h"
#include "connectiondialog.h"
#include "icons.h"
#include "licensedialog.h"
#include "licensing.h"
#include "macrospanel.h"
#include "preferencesdialog.h"
#include "remotebrowser.h"
#include "sessionpane.h"
#include "sidebar.h"
#include "sshsession.h"
#include "statusbar.h"
#include "terminalsession.h"
#include "wsldialog.h"
#include "wslsession.h"
#include "terminalwidget.h"
#include "theme.h"
#include "rdppane.h"
#include "updatechecker.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QSize>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <cmath>

#include <QMenuBar>
#include <QMenu>
#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QPushButton>
#include <QApplication>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

static const QString APP_VERSION = "0.7.1";

static const QString UPDATE_HISTORY = R"(Version 0.7.1

- Download and upload dialogs now open in your Downloads folder instead of the Star Term installation directory
- A file download can now be stopped while it is running, and any partially downloaded file is deleted
- Each file in a multi-file download now has its own progress bar instead of sharing one
- A remote host whose key no longer matches the one already trusted is now reported as a possible impersonation, instead of being presented like a first-time connection; accepting the change now replaces the stored key rather than adding a second one

Version 0.7.0

- Session tabs can be dragged into any order, and the order carries over to the Multi-Exec grid
- Installing an update from within the app now runs the installer silently and relaunches Star Term when it finishes
- Preferences shows the debug log path with Windows backslashes instead of forward slashes
- A refused RDP logon is now reported by Star Term, instead of raising the Windows credential dialog
- An RDP connection that fails before logon re-opens the sign-in dialog, with the reason shown above the username and password fields

Version 0.6.2

- RDP connections now require a username and password before connecting, instead of failing at logon
- RDP credential prompt starts in the password field when the session already supplies a username
- Fixed the main toolbar and the activity bar swapping places on launch
- Remote Files now works in WSL sessions, browsing the distribution through its Windows share
- Fixed the Multi-Exec opt-out checkbox never appearing on session tabs
- Fixed the Reconnect button never appearing after a session dropped
- Remote Files right-click menu now offers only what applies: Download on a file, Upload on empty space
- Re-enabling Follow Current Directory refreshes immediately instead of waiting for the next directory change

Version 0.6.1

- New File > Connect to WSL option for opening a shell in a WSL distribution
- Installed distributions are detected automatically, with the default one preselected and each shown as running or stopped
- A stopped distribution is started before its shell opens
- WSL tabs support session logging, macros and Multi-Exec just like SSH tabs
- New Connection dialog now offers a choice of SSH or RDP, with the port and remaining fields following the selected type
- Passwords are no longer typed into the connection form; SSH asks for one when the session opens
- Session tabs are no longer wrapped in extra controls — the terminal fills the tab, and the Multi-Exec opt-out appears only while Multi-Exec is on
- The window title now shows the active session
- Window size, position and panel layout are remembered between runs

Version 0.6.0

- Embedded RDP sessions now use the Remote Desktop ActiveX control instead of launching and reparenting mstsc.exe
- RDP windows resize in place — no reconnect when the window is resized
- RDP passwords are passed directly to the control and no longer staged in the Windows credential store
- Tab key is now delivered to the remote desktop instead of moving focus
- Company logo added to the About dialog
- Status bar shows license status, or days remaining in the trial

Version 0.5.1

- RDP tab status bar now shows processor queue length, RAM, and page file usage
- RDP tabs no longer disappear permanently when multi-exec mode is toggled

Version 0.5.0

- UI restyled to match uhlix.net site palette: deep navy backgrounds with electric cyan accent in dark mode; complementary blue-white with darker cyan in light mode

Version 0.4.2

- Remote files pane shows "No access" when a directory cannot be read, instead of appending the failed path
- Remote files pane now correctly follows tab-completed cd commands

Version 0.4.1

- Session folder delete now prompts for confirmation; sessions inside deleted folder are permanently removed
- Removed default "General" folder — no folders exist until user creates them; unfoldered sessions appear at root
- Updates now download and launch the installer automatically instead of opening a browser
- Installer version prompt shows both installed and new version
- Session logs written to AppData\star_term\session_logs\ (renamed from logs\)
- Session logs written live (flushed after each write) and ANSI escape codes stripped
- SSH known_hosts now saves correctly (key algorithm flag fix)
- Session logging defaults to disabled on startup

Version 0.4.0

- Session logging toggle in main toolbar — logs terminal output to AppData\star_term\logs\
- Terminal color themes: 10 presets (Default, Solarized, Dracula, Monokai, Nord, One Dark, Gruvbox, Campbell, PowerShell) selectable live in Preferences → Terminal
- Fixed SSH known_hosts not saving to AppData\Roaming\star_term\known_hosts

Version 0.3.3

- SSH terminal receives focus automatically when connection is established
- Startup update check prompts Yes/No and opens browser directly to release page
- Installer silently force-closes Star Term if graceful shutdown fails

Version 0.3.2

- New application icon
- Updates dialog: Check for Updates button, inline status, and startup check toggle (moved from Preferences)
- Update check now reports result whether or not a newer version exists

Version 0.3.1

- Collapsible session sidebar, with a toggle button in the main toolbar
- Session folders can now be removed (right-click menu or the Remove button), even with sessions still inside
- Close Session now disconnects RDP sessions, not just SSH

Version 0.3.0

- RDP sessions embedded in main window tab via Win32 window embedding
- Right-click copy/paste with mouse drag selection in terminal
- SSH and RDP session type icons in the session sidebar
- Check for updates on startup (configurable in Preferences → Updates)

Version 0.2.0

- VT100 terminal emulation with custom parser
- SSH connectivity via libssh2
- Ed25519 offline license verification via OpenSSL
- Tabbed multi-session support with Multi-Exec grid view
- Session sidebar with folder grouping and persistent storage
- SFTP remote file browser with drag-and-drop upload/download
- Macros panel for saved command sequences
- Scrollback buffer (2000 lines) with Shift+PageUp/Down
- Dark/Light themes, configurable font and cursor style
- Export/import sessions (JSON)
- 30-day trial with offline perpetual license key activation
- NSIS Windows installer
)";

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Star Term");
    setWindowIcon(Icons::appIcon());

    // --- Left panel stack ---
    m_sidebar       = new SessionSidebar;
    m_remoteBrowser = new RemoteFileBrowser;
    m_macrosPanel   = new MacrosPanel;

    m_leftStack = new QStackedWidget;
    m_leftStack->addWidget(m_sidebar);
    m_leftStack->addWidget(m_remoteBrowser);
    m_leftStack->addWidget(m_macrosPanel);

    // --- Tab view ---
    m_tabs = new QTabWidget;
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_tabs, &QTabWidget::currentChanged,    this, &MainWindow::onTabChanged);
    // A drag only moves the tab; m_panes is what Multi-Exec lays out and what
    // populateTabs() rebuilds from, so it has to follow or the next view switch
    // would undo the drag.
    connect(m_tabs->tabBar(), &QTabBar::tabMoved, this, &MainWindow::syncPaneOrder);

    // --- Multi-exec grid ---
    m_multiexecContainer = new QWidget;
    m_multiexecLayout    = new QGridLayout(m_multiexecContainer);
    m_multiexecLayout->setContentsMargins(4,4,4,4);
    m_multiexecLayout->setSpacing(4);

    m_viewStack = new QStackedWidget;
    m_viewStack->addWidget(m_tabs);
    m_viewStack->addWidget(m_multiexecContainer);

    // --- Splitter ---
    m_splitter = new QSplitter;
    m_splitter->setHandleWidth(6);
    m_splitter->setContentsMargins(4,4,4,4);
    m_splitter->addWidget(m_leftStack);
    m_splitter->addWidget(m_viewStack);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    setCentralWidget(m_splitter);

    // --- Status bar ---
    m_statusBar = new SystemStatusBar(this);
    setStatusBar(m_statusBar);
    m_statusBar->setLicenseStatus(getLicenseStatus());

    // --- Actions ---
    m_connectAction = new QAction("Connect...", this);
    m_connectAction->setIcon(Icons::connectIcon());
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::openConnectionDialog);

    m_wslConnectAction = new QAction("Connect to WSL...", this);
    connect(m_wslConnectAction, &QAction::triggered, this, &MainWindow::openWslDialog);

    m_closeSessionAction = new QAction("Close Session", this);
    m_closeSessionAction->setIcon(Icons::disconnectIcon());
    connect(m_closeSessionAction, &QAction::triggered, this, &MainWindow::disconnectSession);

    QAction *exportAction = new QAction("Export Sessions...", this);
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportSessions);

    QAction *importAction = new QAction("Import Sessions...", this);
    connect(importAction, &QAction::triggered, this, &MainWindow::importSessions);

    QAction *exitAction = new QAction("Exit", this);
    connect(exitAction, &QAction::triggered, this, &MainWindow::close);

    QMenu *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction(m_connectAction);
    fileMenu->addAction(m_wslConnectAction);
    fileMenu->addAction(m_closeSessionAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exportAction);
    fileMenu->addAction(importAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    m_multiExecAction = new QAction("Multi-Exec View", this);
    m_multiExecAction->setIcon(Icons::multiExecIcon());
    m_multiExecAction->setCheckable(true);
    connect(m_multiExecAction, &QAction::toggled, this, &MainWindow::toggleMultiExecView);

    QMenu *viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction(m_multiExecAction);

    m_preferencesAction = new QAction("Preferences...", this);
    m_preferencesAction->setIcon(Icons::settingsIcon());
    m_preferencesAction->setToolTip("Settings");
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::openPreferencesDialog);

    QMenu *settingsMenu = menuBar()->addMenu("Settings");
    settingsMenu->addAction(m_preferencesAction);

    QAction *aboutAction = new QAction("About Star Term", this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    QAction *updateHistAction = new QAction("Updates", this);
    connect(updateHistAction, &QAction::triggered, this, &MainWindow::showUpdatesDialog);

    QAction *licenseAction = new QAction("License...", this);
    connect(licenseAction, &QAction::triggered, this, &MainWindow::showLicenseDialog);

    QMenu *helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction(aboutAction);
    helpMenu->addAction(updateHistAction);
    helpMenu->addSeparator();
    helpMenu->addAction(licenseAction);

    m_toggleSidebarAction = new QAction("Toggle Sidebar", this);
    m_toggleSidebarAction->setIcon(Icons::sidebarToggleIcon());
    m_toggleSidebarAction->setToolTip("Show/Hide Sidebar");
    m_toggleSidebarAction->setCheckable(true);
    m_toggleSidebarAction->setChecked(true);
    connect(m_toggleSidebarAction, &QAction::toggled, this, [this](bool checked) {
        m_leftStack->setVisible(checked);
    });

    // --- Session logging action ---
    m_sessionLoggingAction = new QAction("Session Logging", this);
    m_sessionLoggingAction->setIcon(Icons::logIcon());
    m_sessionLoggingAction->setToolTip("Session Logging — log terminal output to file");
    m_sessionLoggingAction->setCheckable(true);
    m_sessionLoggingAction->setChecked(false);
    connect(m_sessionLoggingAction, &QAction::toggled,
            this, &MainWindow::toggleSessionLogging);

    // Apply saved color theme before showing
    {
        QJsonObject s = loadSettings();
        setColorTheme(s.value("color_theme").toString("Default"));
    }

    // --- Main toolbar ---
    QToolBar *toolbar = new QToolBar("Main Toolbar", this);
    // saveState()/restoreState() identify toolbars by object name. Without one,
    // Qt falls back to matching by position and hands this bar's slot to the
    // vertical activity bar, silently swapping the two on the next launch.
    toolbar->setObjectName("mainToolBar");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(24, 24));
    toolbar->addAction(m_connectAction);
    toolbar->addAction(m_closeSessionAction);
    toolbar->addSeparator();
    toolbar->addAction(m_toggleSidebarAction);
    toolbar->addAction(m_multiExecAction);
    toolbar->addAction(m_sessionLoggingAction);
    toolbar->addSeparator();
    toolbar->addAction(m_preferencesAction);
    addToolBar(toolbar);

    // --- Activity bar (left, vertical) ---
    m_sessionsAction = new QAction("Sessions", this);
    m_sessionsAction->setIcon(Icons::sessionsIcon());
    m_sessionsAction->setCheckable(true);
    m_sessionsAction->setChecked(true);
    connect(m_sessionsAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            m_leftStack->setCurrentWidget(m_sidebar);
            m_toggleSidebarAction->setChecked(true);
        }
    });

    m_remoteFilesAction = new QAction("Remote Files", this);
    m_remoteFilesAction->setIcon(Icons::directoryIcon());
    m_remoteFilesAction->setCheckable(true);
    connect(m_remoteFilesAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            m_leftStack->setCurrentWidget(m_remoteBrowser);
            m_toggleSidebarAction->setChecked(true);
        }
    });

    m_macrosAction = new QAction("Macros", this);
    m_macrosAction->setIcon(Icons::macrosIcon());
    m_macrosAction->setCheckable(true);
    connect(m_macrosAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            m_leftStack->setCurrentWidget(m_macrosPanel);
            m_toggleSidebarAction->setChecked(true);
        }
    });

    QActionGroup *panelGroup = new QActionGroup(this);
    panelGroup->setExclusive(true);
    panelGroup->addAction(m_sessionsAction);
    panelGroup->addAction(m_remoteFilesAction);
    panelGroup->addAction(m_macrosAction);

    QToolBar *activityBar = new QToolBar("Activity Bar", this);
    activityBar->setObjectName("activityBar");
    activityBar->setMovable(false);
    activityBar->setOrientation(Qt::Vertical);
    activityBar->setIconSize(QSize(24, 24));
    activityBar->addAction(m_sessionsAction);
    activityBar->addAction(m_remoteFilesAction);
    activityBar->addAction(m_macrosAction);
    addToolBar(Qt::LeftToolBarArea, activityBar);

    // --- Macros panel signal ---
    connect(m_macrosPanel, &MacrosPanel::runRequested, this, &MainWindow::runMacro);

    // --- Sidebar signal ---
    connect(m_sidebar, &SessionSidebar::connectRequested,
            this, &MainWindow::connectSavedSession);

    // --- Window geometry: default size, then whatever was saved last run ---
    resize(1200, 800);
    restoreWindowState();

    // --- Startup update check (deferred so the window is visible first) ---
    QTimer::singleShot(1500, this, &MainWindow::checkForUpdates);
}

// -----------------------------------------------------------------------
// Pane management
// -----------------------------------------------------------------------
void MainWindow::addPane(SessionPane *pane) {
    m_panes.append(pane);
    pane->setMultiExecControlsVisible(m_multiExecAction->isChecked());

    connect(pane, &SessionPane::dataToSend, this, [this, pane](const QByteArray &data) {
        onDataToSend(pane, data);
    });
    connect(pane, &SessionPane::sizeChanged, this, [this, pane](int cols, int rows) {
        onSizeChanged(pane, cols, rows);
    });
    connect(pane, &SessionPane::closeRequested, this, [this, pane]() {
        closePane(pane);
    });

    if (m_multiExecAction->isChecked()) {
        populateMultiExecGrid();
    } else {
        m_tabs->addTab(pane, pane->name);
        m_tabs->setCurrentWidget(pane);
    }
}

void MainWindow::closePane(SessionPane *pane) {
    stopPaneLogging(pane);
    pane->disconnectSession();
    if (m_panes.contains(pane)) m_panes.removeAll(pane);
    if (m_remoteBrowser) {
        // Disconnect remote browser if it's tracking this pane
        // (set_pane(nullptr) equivalent)
        m_remoteBrowser->setPane(nullptr);
    }

    if (m_multiExecAction->isChecked()) {
        populateMultiExecGrid();
    } else {
        int idx = m_tabs->indexOf(pane);
        if (idx >= 0) m_tabs->removeTab(idx);
    }
    pane->deleteLater();
}

void MainWindow::closeRdpPane(RdpPane *pane) {
    // Tear the RDP session and its ActiveX control down before removeTab so the
    // COM object is released while the pane is still in a valid widget tree.
    pane->disconnectRdp();
    int idx = m_tabs->indexOf(pane);
    if (idx >= 0) m_tabs->removeTab(idx);
    pane->deleteLater();
}

void MainWindow::onTabCloseRequested(int index) {
    SessionPane *pane = qobject_cast<SessionPane*>(m_tabs->widget(index));
    if (pane) { closePane(pane); return; }
    RdpPane *rdp = qobject_cast<RdpPane*>(m_tabs->widget(index));
    if (rdp) closeRdpPane(rdp);
}

// Reflects the active session in the title bar. NOTE: the NSIS installer no
// longer looks for a fixed window title when checking whether the app is
// running — it matches the process name instead.
void MainWindow::updateWindowTitle() {
    if (m_multiExecAction && m_multiExecAction->isChecked()) {
        setWindowTitle("Multi-Exec — Star Term");
        return;
    }

    QString session;
    if (QWidget *current = m_tabs->currentWidget()) {
        if (SessionPane *pane = qobject_cast<SessionPane*>(current))
            session = pane->name;
        else if (RdpPane *rdp = qobject_cast<RdpPane*>(current))
            session = rdp->name;
    }

    setWindowTitle(session.isEmpty() ? QStringLiteral("Star Term")
                                     : QString("%1 — Star Term").arg(session));
}

// Bumped whenever the toolbar layout changes in a way that makes an older
// saved state wrong. restoreState() rejects a state carrying a different
// version, so a stale layout is discarded instead of being applied.
// 1: toolbars gained object names, without which they were restored swapped.
static const int WINDOW_STATE_VERSION = 1;

void MainWindow::saveWindowState() {
    QJsonObject s = loadSettings();
    s["window_geometry"] = QString::fromLatin1(saveGeometry().toBase64());
    s["window_state"]    = QString::fromLatin1(saveState(WINDOW_STATE_VERSION).toBase64());
    if (m_splitter)
        s["splitter_state"] = QString::fromLatin1(m_splitter->saveState().toBase64());
    saveSettings(s);
}

void MainWindow::restoreWindowState() {
    const QJsonObject s = loadSettings();

    const QString geometry = s.value("window_geometry").toString();
    if (!geometry.isEmpty())
        restoreGeometry(QByteArray::fromBase64(geometry.toLatin1()));

    const QString state = s.value("window_state").toString();
    if (!state.isEmpty())
        restoreState(QByteArray::fromBase64(state.toLatin1()), WINDOW_STATE_VERSION);

    const QString splitter = s.value("splitter_state").toString();
    if (m_splitter && !splitter.isEmpty())
        m_splitter->restoreState(QByteArray::fromBase64(splitter.toLatin1()));
}

void MainWindow::toggleMultiExecView(bool checked) {
    for (SessionPane *pane : m_panes)
        pane->setMultiExecControlsVisible(checked);
    updateWindowTitle();

    if (checked) {
        populateMultiExecGrid();
        m_viewStack->setCurrentWidget(m_multiexecContainer);
        resizeForMultiExec();
    } else {
        populateTabs();
        m_viewStack->setCurrentWidget(m_tabs);
    }
    m_statusBar->setVisible(!checked);
}

void MainWindow::resizeForMultiExec() {
    if (m_panes.isEmpty()) return;
    int columns = std::max(1, (int)std::ceil(std::sqrt((double)m_panes.size())));
    int rows    = (int)std::ceil((double)m_panes.size() / columns);

    QSize paneHint = m_panes[0]->terminal->sizeHint();
    int desiredWidth  = columns * paneHint.width()  + m_sidebar->width() + 40;
    int desiredHeight = rows    * paneHint.height() + 80;

    QRect available = screen()->availableGeometry();
    int w = std::min(desiredWidth,  available.width());
    int h = std::min(desiredHeight, available.height());

    if (w > width() || h > height())
        resize(std::max(width(), w), std::max(height(), h));
}

void MainWindow::populateMultiExecGrid() {
    while (m_multiexecLayout->count())
        m_multiexecLayout->takeAt(0);
    // Remove only SSH session tabs; RDP tabs stay in m_tabs so they reappear on exit
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        if (qobject_cast<SessionPane*>(m_tabs->widget(i)))
            m_tabs->removeTab(i);
    }

    for (int i = 0; i < m_multiexecLayout->rowCount(); ++i)
        m_multiexecLayout->setRowStretch(i, 0);
    for (int i = 0; i < m_multiexecLayout->columnCount(); ++i)
        m_multiexecLayout->setColumnStretch(i, 0);

    if (m_panes.isEmpty()) return;

    int columns = std::max(1, (int)std::ceil(std::sqrt((double)m_panes.size())));
    int rows    = (int)std::ceil((double)m_panes.size() / columns);

    for (int i = 0; i < m_panes.size(); ++i) {
        int row = i / columns;
        int col = i % columns;
        m_multiexecLayout->addWidget(m_panes[i], row, col);
        m_panes[i]->show();
    }
    for (int c = 0; c < columns; ++c) m_multiexecLayout->setColumnStretch(c, 1);
    for (int r = 0; r < rows;    ++r) m_multiexecLayout->setRowStretch(r, 1);
}

void MainWindow::populateTabs() {
    while (m_multiexecLayout->count())
        m_multiexecLayout->takeAt(0);
    for (SessionPane *pane : m_panes)
        m_tabs->addTab(pane, pane->name);
}

// Rewrites m_panes in the order the tabs now sit in, so a dragged tab keeps its
// place everywhere else the list is used.
void MainWindow::syncPaneOrder() {
    QList<SessionPane*> ordered;
    for (int i = 0; i < m_tabs->count(); ++i)
        if (SessionPane *pane = qobject_cast<SessionPane*>(m_tabs->widget(i)))
            ordered.append(pane);
    // Anything not currently a tab keeps its relative order at the end rather
    // than being dropped from the list.
    for (SessionPane *pane : m_panes)
        if (!ordered.contains(pane)) ordered.append(pane);
    m_panes = ordered;
}

void MainWindow::onDataToSend(SessionPane *pane, const QByteArray &data) {
    if (m_multiExecAction->isChecked() && !pane->excludeCheckbox->isChecked()) {
        for (SessionPane *p : m_panes) {
            if (p->session && !p->excludeCheckbox->isChecked())
                p->session->send(data);
        }
    } else if (pane->session) {
        pane->session->send(data);
    }
}

void MainWindow::runMacro(const QString &commands, bool autoExecute) {
    QStringList lines = commands.split('\n');
    if (lines.isEmpty()) return;

    QString text;
    if (autoExecute) {
        for (const QString &line : lines) text += line + "\n";
    } else {
        for (int i = 0; i < lines.size() - 1; ++i) text += lines[i] + "\n";
        text += lines.last();
    }
    QByteArray data = text.toUtf8();

    if (m_multiExecAction->isChecked()) {
        for (SessionPane *p : m_panes)
            if (p->session && !p->excludeCheckbox->isChecked())
                p->session->send(data);
    } else {
        SessionPane *pane = qobject_cast<SessionPane*>(m_tabs->currentWidget());
        if (pane && pane->session) pane->session->send(data);
    }
}

void MainWindow::onSizeChanged(SessionPane *pane, int cols, int rows) {
    if (pane->session) pane->session->resize(cols, rows);
}

void MainWindow::onTabChanged(int index) {
    SessionPane *pane = qobject_cast<SessionPane*>(m_tabs->widget(index));
    m_remoteBrowser->setPane(pane);
    updateWindowTitle();
    if (!m_multiExecAction->isChecked()) {
        if (pane) {
            m_statusBar->updateStats(pane->lastStats);
        } else if (RdpPane *rdp = qobject_cast<RdpPane*>(m_tabs->widget(index))) {
            m_statusBar->updateStats(rdp->lastStats);
        } else {
            m_statusBar->updateStats({});
        }
    }
}

// -----------------------------------------------------------------------
// Session connection
// -----------------------------------------------------------------------
void MainWindow::openWslDialog() {
    WslConnectDialog dlg(this);
    if (!dlg.hasDistributions()) {
        QMessageBox::information(this, "No WSL Distributions",
            "No WSL distributions were found.\n\n"
            "Install one with \"wsl --install\" and try again.");
        return;
    }
    if (!dlg.exec()) return;

    const QString distro = dlg.selectedDistribution();
    if (distro.isEmpty()) return;

    // Boot the distribution first if it is not running. Opening the shell would
    // start it implicitly, but that can take several seconds during which the
    // terminal just looks hung.
    if (!wslRunningDistributions().contains(distro)) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString error;
        const bool started = wslStartDistribution(distro, &error);
        QApplication::restoreOverrideCursor();

        if (!started) {
            QMessageBox::warning(this, "WSL",
                QString("Could not start the %1 distribution.\n\n%2")
                    .arg(distro, error));
            return;
        }
    }

    QJsonObject params;
    params["type"]   = "wsl";
    params["distro"] = distro;

    QJsonObject settings = loadSettings();
    SessionPane *pane = new SessionPane(
        QString("WSL: %1").arg(distro),
        settings.value("font_family").toString("Courier New"),
        settings.value("font_size").toInt(10),
        settings.value("cursor_style").toString("underline")
    );
    addPane(pane);

    connect(pane, &SessionPane::reconnectRequested, this, [this, pane]() {
        reconnectPane(pane);
    });

    startSession(pane, params);
}

void MainWindow::openConnectionDialog() {
    ConnectionDialog dlg(this);
    if (!dlg.exec()) return;

    QJsonObject params = dlg.getConnectionParams();
    QString name = QString("%1@%2").arg(
        params["username"].toString(), params["host"].toString());

    if (params.value("type").toString() == "rdp") {
        params["name"] = name;
        connectSavedSession(params);
        return;
    }

    // The dialog no longer collects a password, so prompt for one here — the
    // same flow a saved session uses.  A private key supplies its own passphrase.
    if (params["key_path"].toString().isEmpty()) {
        bool ok = false;
        QString password = QInputDialog::getText(
            this, "Password",
            QString("Password for %1:").arg(name),
            QLineEdit::Password, "", &ok);
        if (!ok) return;
        params["password"] = password;
    }

    connectSession(params, name);
}

void MainWindow::connectSavedSession(const QJsonObject &session) {
    // RDP sessions open in an embedded tab hosting the Remote Desktop ActiveX control.
    if (session.value("type").toString("ssh") == "rdp") {
        RdpPane *rdp = new RdpPane(session, this);
        m_tabs->addTab(rdp, rdp->name);
        m_tabs->setCurrentWidget(rdp);
        connect(rdp, &RdpPane::closeRequested, this, [this, rdp]() {
            closeRdpPane(rdp);
        });
        connect(rdp, &RdpPane::statsReady, this, [this, rdp](const QJsonObject &stats) {
            if (!m_multiExecAction->isChecked() && rdp == m_tabs->currentWidget())
                m_statusBar->updateStats(stats);
        });
        return;
    }

    QString keyPath, keyPassphrase, password;
    QJsonObject settings = loadSettings();

    if (session.value("use_key").toBool()) {
        keyPath = session.value("key_path").toString();
        if (keyPath.isEmpty()) keyPath = settings.value("ssh_key_path").toString();
        if (keyPath.isEmpty()) {
            QMessageBox::warning(this, "No SSH Key",
                "No SSH key is configured. Set one in Settings > Preferences > SSH Key "
                "or override it on this session's profile.");
            return;
        }
        bool ok = false;
        keyPassphrase = QInputDialog::getText(
            this, "Key Passphrase",
            QString("Passphrase for %1 (leave blank if none):").arg(keyPath),
            QLineEdit::Password, "", &ok);
        if (!ok) return;
    } else {
        bool ok = false;
        password = QInputDialog::getText(
            this, "Password",
            QString("Password for %1@%2:").arg(
                session["username"].toString(), session["host"].toString()),
            QLineEdit::Password, "", &ok);
        if (!ok) return;
    }

    QJsonObject params;
    params["host"]            = session["host"];
    params["port"]            = session["port"];
    params["username"]        = session["username"];
    params["password"]        = password;
    params["key_path"]        = keyPath;
    params["key_passphrase"]  = keyPassphrase;

    connectSession(params, session.value("name").toString());
}

void MainWindow::connectSession(const QJsonObject &params, const QString &name) {
    QJsonObject settings = loadSettings();
    SessionPane *pane = new SessionPane(
        name.isEmpty() ? QString("%1@%2").arg(
            params["username"].toString(), params["host"].toString()) : name,
        settings.value("font_family").toString("Courier New"),
        settings.value("font_size").toInt(10),
        settings.value("cursor_style").toString("underline")
    );
    addPane(pane);

    connect(pane, &SessionPane::reconnectRequested, this, [this, pane]() {
        reconnectPane(pane);
    });

    startSession(pane, params);
}

// Signal wiring shared by every backend. Anything SSH-specific stays in
// startSession so a WSL pane behaves identically for logging, macros,
// multi-exec broadcast and reconnect.
void MainWindow::wireSession(SessionPane *pane, TerminalSession *session) {
    pane->session = session;

    connect(session, &TerminalSession::dataReceived, pane->terminal, &TerminalWidget::feed);
    connect(session, &TerminalSession::dataReceived, pane->cwdTracker, &CwdTracker::feedServerData);
    connect(session, &TerminalSession::dataReceived, this, [this, pane](const QByteArray &data) {
        if (QFile *f = m_sessionLogs.value(pane)) {
            static QRegularExpression s_ansi(
                R"(\x1B(?:[@-Z\-_]|\[[0-9;?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1B\\)))");
            QString text = QString::fromUtf8(data);
            text.remove(s_ansi);
            f->write(text.toUtf8());
            f->flush();
        }
    });
    connect(session, &TerminalSession::connectionError, this, &MainWindow::showError);
    connect(session, &TerminalSession::connectionError, this, [this, pane](const QString&) {
        onSessionEnded(pane);
    });
    connect(session, &TerminalSession::connectionClosed, this, [this, pane]() {
        onSessionEnded(pane);
    });
    connect(session, &TerminalSession::connected, this, [this, pane]() {
        onSessionConnected(pane);
    });
    connect(pane, &SessionPane::statsUpdated, this, [this, pane](const QJsonObject &stats) {
        if (!m_multiExecAction->isChecked() && pane == m_tabs->currentWidget())
            m_statusBar->updateStats(stats);
    });
}

void MainWindow::startSession(SessionPane *pane, const QJsonObject &params) {
    pane->connectionParams = params;
    pane->setReconnectVisible(false);

    // Local WSL shell in a pseudo-console rather than an SSH channel.
    if (params.value("type").toString("ssh") == "wsl") {
        WslSession *wsl = new WslSession(
            params["distro"].toString(),
            pane->terminal->screen().cols(),
            pane->terminal->screen().rows()
        );
        wireSession(pane, wsl);
        wsl->start();
        return;
    }

    SSHSession *session = new SSHSession(
        params["host"].toString(),
        params["port"].toInt(22),
        params["username"].toString(),
        params["password"].toString(),
        params["key_path"].toString(),
        params["key_passphrase"].toString(),
        "vt100",
        pane->terminal->screen().cols(),
        pane->terminal->screen().rows()
    );
    wireSession(pane, session);
    connect(session, &SSHSession::hostKeyUnknown,
            this, &MainWindow::onHostKeyUnknown);

    session->start();
}

void MainWindow::onSessionConnected(SessionPane *pane) {
    if (!m_multiExecAction->isChecked()) {
        m_tabs->setCurrentWidget(pane);
        pane->terminal->setFocus();
    }
    if (pane == m_tabs->currentWidget())
        m_remoteBrowser->setPane(pane);
    pane->startStatsWorker();
    if (m_sessionLoggingAction->isChecked())
        startPaneLogging(pane);
}

void MainWindow::onSessionEnded(SessionPane *pane) {
    pane->stopStatsWorker();
    if (!m_multiExecAction->isChecked() && pane == m_tabs->currentWidget())
        m_statusBar->updateStats({});
    if (m_panes.contains(pane))
        pane->setReconnectVisible(true);
}

// -----------------------------------------------------------------------
// Session logging
// -----------------------------------------------------------------------
void MainWindow::startPaneLogging(SessionPane *pane) {
    if (m_sessionLogs.contains(pane)) return;
    QString logsDir = getAppDataDir() + "/session_logs";
    QDir().mkpath(logsDir);
    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString safeName = pane->name;
    safeName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    QString path = logsDir + "/" + safeName + "_" + ts + ".log";
    QFile *f = new QFile(path, this);
    if (f->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        m_sessionLogs[pane] = f;
    else
        delete f;
}

void MainWindow::stopPaneLogging(SessionPane *pane) {
    if (QFile *f = m_sessionLogs.take(pane)) {
        f->close();
        f->deleteLater();
    }
}

void MainWindow::toggleSessionLogging(bool enabled) {
    QJsonObject s = loadSettings();
    s["session_logging"] = enabled;
    saveSettings(s);
    if (enabled) {
        for (SessionPane *pane : m_panes)
            if (pane->session) startPaneLogging(pane);
    } else {
        for (SessionPane *pane : m_panes)
            stopPaneLogging(pane);
    }
}

void MainWindow::reconnectPane(SessionPane *pane) {
    if (pane->connectionParams.isEmpty()) return;
    if (pane->session) {
        pane->session->stop();
        pane->session->wait();
        pane->session->deleteLater();
        pane->session = nullptr;
    }
    startSession(pane, pane->connectionParams);
}

void MainWindow::onHostKeyUnknown(const QString &host, const QString &keyType,
                                   const QString &fingerprint,
                                   const QString &storedFingerprint,
                                   bool mismatch) {
    SSHSession *session = qobject_cast<SSHSession*>(sender());
    bool accepted = false;

    if (mismatch) {
        // A key we already trust has changed. This is the MITM signal, so it gets
        // its own alarming dialog rather than the routine first-connect prompt.
        QMessageBox box(this);
        box.setIcon(QMessageBox::Critical);
        box.setWindowTitle("Remote Host Identification Has Changed");
        box.setText(QString("The %1 host key for '%2' does not match the key "
                            "already trusted for this host.").arg(keyType, host));
        box.setInformativeText(
            QString("Someone may be impersonating this host to intercept your "
                    "session — or the host's key was legitimately changed.\n\n"
                    "Previously trusted:\n%1\n\n"
                    "Offered now:\n%2\n\n"
                    "Continue only if you know why the key changed. Continuing "
                    "replaces the trusted key for this host.")
                .arg(storedFingerprint.isEmpty() ? QString("(unavailable)")
                                                 : storedFingerprint,
                     fingerprint));

        QPushButton *cancelBtn  = box.addButton("Cancel Connection",
                                                QMessageBox::RejectRole);
        QPushButton *replaceBtn = box.addButton("Replace Key and Connect",
                                                QMessageBox::DestructiveRole);
        box.setDefaultButton(cancelBtn);
        box.exec();
        accepted = (box.clickedButton() == replaceBtn);
    } else {
        int answer = QMessageBox::question(
            this, "Unknown Host Key",
            QString("The authenticity of host '%1' can't be established.\n"
                    "%2 key fingerprint:\n%3\n\n"
                    "Are you sure you want to continue connecting?")
                .arg(host, keyType, fingerprint),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No
        );
        accepted = (answer == QMessageBox::Yes);
    }

    if (session) {
        if (accepted) session->acceptHostKey();
        else          session->rejectHostKey();
    }
}

void MainWindow::disconnectSession() {
    if (m_multiExecAction->isChecked()) return;
    QWidget *current = m_tabs->currentWidget();
    if (SessionPane *pane = qobject_cast<SessionPane*>(current)) {
        closePane(pane);
        return;
    }
    if (RdpPane *rdp = qobject_cast<RdpPane*>(current)) {
        closeRdpPane(rdp);
    }
}

void MainWindow::showError(const QString &message) {
    QMessageBox::critical(this, "Connection Error", message);
}

// -----------------------------------------------------------------------
// Menu actions
// -----------------------------------------------------------------------
void MainWindow::openPreferencesDialog() {
    QJsonObject settings = loadSettings();
    QString prevColorTheme = settings.value("color_theme").toString("Default");

    PreferencesDialog dlg(
        this,
        settings.value("font_family").toString("Courier New"),
        settings.value("font_size").toInt(10),
        settings.value("cursor_style").toString("underline"),
        settings.value("theme").toString("dark"),
        prevColorTheme
    );

    // Live color theme preview while dialog is open
    connect(&dlg, &PreferencesDialog::colorThemePreviewRequested,
            this, [this](const QString &name) {
        setColorTheme(name);
        for (SessionPane *pane : m_panes) pane->terminal->update();
    });

    if (dlg.exec()) {
        QJsonObject newTermSettings = dlg.getTerminalSettings();
        setColorTheme(newTermSettings["color_theme"].toString("Default"));
        for (SessionPane *pane : m_panes) {
            pane->applySettings(
                newTermSettings["font_family"].toString(),
                newTermSettings["font_size"].toInt(),
                newTermSettings["cursor_style"].toString()
            );
            pane->terminal->update();
        }
        QJsonObject newGenSettings = dlg.getGeneralSettings();
        settings = loadSettings();
        // Merge
        for (auto it = newTermSettings.begin(); it != newTermSettings.end(); ++it)
            settings[it.key()] = it.value();
        for (auto it = newGenSettings.begin(); it != newGenSettings.end(); ++it)
            settings[it.key()] = it.value();
        saveSettings(settings);
        clearStylesheetCache();
        qApp->setStyleSheet(
            getStylesheet(settings.value("theme").toString("dark")));
    } else {
        // Cancelled — revert live color theme preview
        setColorTheme(prevColorTheme);
        for (SessionPane *pane : m_panes) pane->terminal->update();
    }
}

void MainWindow::showAboutDialog() {
    QDialog dlg(this);
    dlg.setWindowTitle("About Star Term");

    bool isDark = loadSettings().value("theme").toString("dark") != "light";
    QString logoResource = isDark ? ":/logo-dark.png" : ":/logo-light.png";
    QPixmap logoPm(logoResource);
    QLabel *logoLabel = new QLabel;
    logoLabel->setPixmap(logoPm.scaled(280, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);

    QLabel *iconLabel = new QLabel;
    iconLabel->setPixmap(Icons::appIcon().pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel *versionLabel = new QLabel(
        QString("Star Term v%1").arg(APP_VERSION));
    versionLabel->setAlignment(Qt::AlignCenter);
    QFont f = versionLabel->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    versionLabel->setFont(f);

    QLabel *infoLabel = new QLabel(
        "SSH Terminal with VT100 emulation\n"
        "Built with Qt6, libssh2, and OpenSSL");
    infoLabel->setAlignment(Qt::AlignCenter);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->addWidget(logoLabel);
    layout->addWidget(iconLabel);
    layout->addWidget(versionLabel);
    layout->addWidget(infoLabel);
    layout->addWidget(buttons);

    dlg.exec();
}

void MainWindow::showUpdatesDialog() {
    QDialog dlg(this);
    dlg.setWindowTitle("Updates");

    // --- Current version row ---
    QLabel *versionLabel = new QLabel(
        QString("Current version:  <b>%1</b>").arg(APP_VERSION));
    versionLabel->setTextFormat(Qt::RichText);

    QPushButton *checkBtn = new QPushButton("Check for Updates");
    checkBtn->setFixedWidth(160);

    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->addWidget(versionLabel);
    topRow->addStretch();
    topRow->addWidget(checkBtn);

    // --- Status line ---
    QLabel *statusLabel = new QLabel;
    statusLabel->setTextFormat(Qt::RichText);
    statusLabel->setOpenExternalLinks(true);
    statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusLabel->setWordWrap(true);

    // --- Startup check preference ---
    QJsonObject curSettings = loadSettings();
    QCheckBox *startupCheck = new QCheckBox("Check for updates automatically on startup");
    startupCheck->setChecked(curSettings.value("check_updates").toBool(true));
    connect(startupCheck, &QCheckBox::toggled, this, [](bool checked) {
        QJsonObject s = loadSettings();
        s["check_updates"] = checked;
        saveSettings(s);
    });

    // --- Release history ---
    QLabel *histHeader = new QLabel("Release History");
    QFont hf = histHeader->font();
    hf.setBold(true);
    histHeader->setFont(hf);

    QLabel *textLabel = new QLabel(UPDATE_HISTORY);
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    textLabel->setContentsMargins(8, 8, 8, 8);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(textLabel);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->addLayout(topRow);
    layout->addWidget(statusLabel);
    layout->addWidget(startupCheck);
    layout->addSpacing(8);
    layout->addWidget(histHeader);
    layout->addWidget(scrollArea);
    layout->addWidget(buttons);

    // --- Ensure checker exists ---
    if (!m_updateChecker)
        m_updateChecker = new UpdateChecker(APP_VERSION, this);

    // --- Install button (hidden until update found) ---
    QPushButton *installBtn = new QPushButton("Download && Install");
    installBtn->setVisible(false);
    QString pendingDlUrl;

    connect(installBtn, &QPushButton::clicked, &dlg, [this, &dlg, &pendingDlUrl]() {
        dlg.accept();
        downloadAndInstall(pendingDlUrl);
    });

    layout->insertWidget(layout->indexOf(statusLabel) + 1, installBtn);

    // --- Check button: fires a single-shot check, result shown inline ---
    connect(checkBtn, &QPushButton::clicked, &dlg,
            [this, checkBtn, statusLabel, installBtn, &pendingDlUrl]() {
        checkBtn->setEnabled(false);
        installBtn->setVisible(false);
        statusLabel->setText("Checking...");
        connect(m_updateChecker, &UpdateChecker::checkFinished,
                checkBtn, [this, checkBtn, statusLabel, installBtn, &pendingDlUrl]
                (bool hasUpdate, const QString &ver, const QString &url, const QString &dlUrl) {
            checkBtn->setEnabled(true);
            if (hasUpdate) {
                if (!dlUrl.isEmpty()) {
                    pendingDlUrl = dlUrl;
                    installBtn->setVisible(true);
                    statusLabel->setText(
                        QString("Version <b>%1</b> is available.").arg(ver));
                } else {
                    statusLabel->setText(
                        QString("Version <b>%1</b> is available &mdash; "
                                "<a href='%2'>download</a>").arg(ver, url));
                }
            } else if (!ver.isEmpty()) {
                statusLabel->setText("You are up to date.");
            } else {
                statusLabel->setText("Could not reach update server. Check your network connection.");
            }
        }, Qt::SingleShotConnection);
        m_updateChecker->checkAsync();
    });

    dlg.resize(520, 500);
    dlg.exec();
}

void MainWindow::showLicenseDialog() {
    LicenseDialog dlg(this);
    dlg.exec();
}

void MainWindow::checkForUpdates() {
    QJsonObject settings = loadSettings();
    if (!settings.value("check_updates").toBool(true)) return;
    if (!m_updateChecker)
        m_updateChecker = new UpdateChecker(APP_VERSION, this);

    connect(m_updateChecker, &UpdateChecker::checkFinished,
            this, [this](bool hasUpdate, const QString &ver, const QString &url, const QString &dlUrl) {
        if (!hasUpdate) return;
        int ret = QMessageBox::question(
            this, "Update Available",
            QString("Star Term %1 is available.\n\nWould you like to install it now?").arg(ver),
            QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            if (!dlUrl.isEmpty())
                downloadAndInstall(dlUrl);
            else
                QDesktopServices::openUrl(QUrl(url));
        }
    }, Qt::SingleShotConnection);

    m_updateChecker->checkAsync();
}

void MainWindow::downloadAndInstall(const QString &url) {
    auto *progress = new QProgressDialog("Downloading update...", "Cancel", 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setValue(0);
    progress->show();

    auto *nam = new QNetworkAccessManager(this);
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setRawHeader("User-Agent", "star-term-updater");
    QNetworkReply *reply = nam->get(req);

    connect(reply, &QNetworkReply::downloadProgress, progress,
            [progress](qint64 got, qint64 total) {
        if (total > 0) {
            progress->setMaximum(100);
            progress->setValue(int(got * 100 / total));
        }
    });
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, progress, nam]() {
        progress->close();
        progress->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            showError("Download failed: " + reply->errorString());
            reply->deleteLater();
            return;
        }
        QString path = QDir::tempPath() + "/star_term_setup.exe";
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(reply->readAll()) < 0) {
            showError("Could not save installer to temp directory.");
            reply->deleteLater();
            return;
        }
        f.close();
        reply->deleteLater();
        // /S installs without any of the wizard pages (UAC still prompts) and
        // makes the installer relaunch Star Term once it is done, so an update
        // started from here comes back up on its own.
        QProcess::startDetached(path, {"/S"});
        QApplication::quit();
    });
}

void MainWindow::exportSessions() {
    QString path = QFileDialog::getSaveFileName(
        this, "Export Sessions", "sessions.json", "JSON Files (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, "Export Failed", f.errorString());
        return;
    }
    f.write(QJsonDocument(loadSessions()).toJson(QJsonDocument::Indented));
    f.close();
}

void MainWindow::importSessions() {
    QString path = QFileDialog::getOpenFileName(
        this, "Import Sessions", "", "JSON Files (*.json)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Import Failed", f.errorString());
        return;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        QMessageBox::critical(this, "Import Failed",
                              "File is not a valid sessions JSON array.");
        return;
    }

    if (QMessageBox::question(
            this, "Import Sessions",
            "This will replace your current saved sessions. Continue?")
        != QMessageBox::Yes) return;

    saveSessions(doc.array());
    m_sidebar->reload();
}

// -----------------------------------------------------------------------
// Close
// -----------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent *event) {
    saveWindowState();
    for (SessionPane *pane : QList<SessionPane*>(m_panes))
        pane->disconnectSession();
    QMainWindow::closeEvent(event);
}
