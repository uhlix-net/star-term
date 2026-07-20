#include "mainwindow.h"
#include "config.h"
#include "connectiondialog.h"
#include "icons.h"
#include "licensedialog.h"
#include "macrospanel.h"
#include "preferencesdialog.h"
#include "remotebrowser.h"
#include "sessionpane.h"
#include "sidebar.h"
#include "sshsession.h"
#include "statusbar.h"
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
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <cmath>

#include <QMenuBar>
#include <QMenu>
#include <QCheckBox>
#include <QDesktopServices>
#include <QPushButton>
#include <QApplication>
#include <QProcess>
#include <QTimer>
#include <QUrl>

static const QString APP_VERSION = "0.3.2";

static const QString UPDATE_HISTORY = R"(Version 0.3.2

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
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_tabs, &QTabWidget::currentChanged,    this, &MainWindow::onTabChanged);

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

    // --- Actions ---
    m_connectAction = new QAction("Connect...", this);
    m_connectAction->setIcon(Icons::connectIcon());
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::openConnectionDialog);

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

    // --- Main toolbar ---
    QToolBar *toolbar = new QToolBar("Main Toolbar", this);
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(24, 24));
    toolbar->addAction(m_connectAction);
    toolbar->addAction(m_closeSessionAction);
    toolbar->addSeparator();
    toolbar->addAction(m_toggleSidebarAction);
    toolbar->addAction(m_multiExecAction);
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

    // --- Startup update check (deferred so the window is visible first) ---
    QTimer::singleShot(1500, this, &MainWindow::checkForUpdates);
}

// -----------------------------------------------------------------------
// Pane management
// -----------------------------------------------------------------------
void MainWindow::addPane(SessionPane *pane) {
    m_panes.append(pane);

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
    // Un-parent the mstsc window BEFORE removeTab triggers Qt's native window
    // hierarchy update in Qt6Gui.dll — otherwise Qt walks our HWND children,
    // finds the foreign mstsc HWND, and crashes with an access violation.
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

void MainWindow::toggleMultiExecView(bool checked) {
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
    while (m_tabs->count())
        m_tabs->removeTab(0);

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
    if (!m_multiExecAction->isChecked())
        m_statusBar->updateStats(pane ? pane->lastStats : QJsonObject());
}

// -----------------------------------------------------------------------
// Session connection
// -----------------------------------------------------------------------
void MainWindow::openConnectionDialog() {
    ConnectionDialog dlg(this);
    if (dlg.exec()) {
        QJsonObject params = dlg.getConnectionParams();
        QString name = QString("%1@%2").arg(
            params["username"].toString(), params["host"].toString());
        connectSession(params, name);
    }
}

void MainWindow::connectSavedSession(const QJsonObject &session) {
    // RDP sessions open in an embedded tab using Win32 window embedding (mstsc.exe reparented).
    if (session.value("type").toString("ssh") == "rdp") {
        RdpPane *rdp = new RdpPane(session, this);
        m_tabs->addTab(rdp, rdp->name);
        m_tabs->setCurrentWidget(rdp);
        connect(rdp, &RdpPane::closeRequested, this, [this, rdp]() {
            closeRdpPane(rdp);
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

void MainWindow::startSession(SessionPane *pane, const QJsonObject &params) {
    pane->connectionParams = params;
    pane->reconnectBtn->setVisible(false);

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
    pane->session = session;

    connect(session, &SSHSession::dataReceived,    pane->terminal, &TerminalWidget::feed);
    connect(session, &SSHSession::connectionError, this, &MainWindow::showError);
    connect(session, &SSHSession::connectionError, this, [this, pane](const QString&) {
        onSessionEnded(pane);
    });
    connect(session, &SSHSession::connectionClosed, this, [this, pane]() {
        onSessionEnded(pane);
    });
    connect(session, &SSHSession::connected, this, [this, pane]() {
        onSessionConnected(pane);
    });
    connect(pane, &SessionPane::statsUpdated, this, [this, pane](const QJsonObject &stats) {
        if (!m_multiExecAction->isChecked() && pane == m_tabs->currentWidget())
            m_statusBar->updateStats(stats);
    });
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
}

void MainWindow::onSessionEnded(SessionPane *pane) {
    pane->stopStatsWorker();
    if (!m_multiExecAction->isChecked() && pane == m_tabs->currentWidget())
        m_statusBar->updateStats({});
    if (m_panes.contains(pane))
        pane->reconnectBtn->setVisible(true);
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
                                   const QString &fingerprint, const QString &/*hexHash*/) {
    SSHSession *session = qobject_cast<SSHSession*>(sender());
    int answer = QMessageBox::question(
        this, "Unknown Host Key",
        QString("The authenticity of host '%1' can't be established.\n"
                "%2 key fingerprint:\n%3\n\n"
                "Are you sure you want to continue connecting?")
            .arg(host, keyType, fingerprint),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );

    if (session) {
        if (answer == QMessageBox::Yes) session->acceptHostKey();
        else                            session->rejectHostKey();
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

    PreferencesDialog dlg(
        this,
        settings.value("font_family").toString("Courier New"),
        settings.value("font_size").toInt(10),
        settings.value("cursor_style").toString("underline"),
        settings.value("theme").toString("dark")
    );
    if (dlg.exec()) {
        QJsonObject newTermSettings = dlg.getTerminalSettings();
        for (SessionPane *pane : m_panes) {
            pane->applySettings(
                newTermSettings["font_family"].toString(),
                newTermSettings["font_size"].toInt(),
                newTermSettings["cursor_style"].toString()
            );
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
    }
}

void MainWindow::showAboutDialog() {
    QDialog dlg(this);
    dlg.setWindowTitle("About Star Term");

    QLabel *iconLabel = new QLabel;
    iconLabel->setPixmap(Icons::appIcon().pixmap(96, 96));
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

    // --- Check button: fires a single-shot check, result shown inline ---
    connect(checkBtn, &QPushButton::clicked, &dlg,
            [this, checkBtn, statusLabel]() {
        checkBtn->setEnabled(false);
        statusLabel->setText("Checking...");
        connect(m_updateChecker, &UpdateChecker::checkFinished,
                checkBtn, [checkBtn, statusLabel]
                (bool hasUpdate, const QString &ver, const QString &url) {
            checkBtn->setEnabled(true);
            if (hasUpdate)
                statusLabel->setText(
                    QString("Version <b>%1</b> is available &mdash; "
                            "<a href='%2'>download</a>").arg(ver, url));
            else if (!ver.isEmpty())
                statusLabel->setText("You are up to date.");
            else
                statusLabel->setText("Could not reach update server. Check your network connection.");
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
            this, [this](bool hasUpdate, const QString &ver, const QString &url) {
        if (!hasUpdate) return;
        int ret = QMessageBox::question(
            this, "Update Available",
            QString("Star Term %1 is available.\n\nWould you like to download it now?").arg(ver),
            QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl(url));
    }, Qt::SingleShotConnection);

    m_updateChecker->checkAsync();
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
    for (SessionPane *pane : QList<SessionPane*>(m_panes))
        pane->disconnectSession();
    QMainWindow::closeEvent(event);
}
