# star_term (Windows)

SSH terminal client. VT100 emulation via `pyte`, GUI via PySide6, SSH via paramiko.

Defaults: Courier New font, Linux standard console colors, VT100 terminal type, port 22 (configurable).

## Run from source

```
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python -m star_term.main
```

## Building the installer

Requires [NSIS](https://nsis.sourceforge.io/) (`makensis`) on the build machine
(`sudo apt-get install nsis` on Debian/Ubuntu).

```
cd installer
makensis setup.nsi
```

The output installer is written to `installer/Output/star_term_setup.exe`.

Installer behavior:
- On launch, shows a splash screen (`splash.bmp`, a terminal-window mockup -
  regenerate with `python make_splash.py`) instantly on top of all windows for
  ~3 seconds (no fade), then continues automatically.
- Checks the registry for an existing star_term install; if found, prompts the
  user to install/update or cancel.
- Runs a dedicated Python check section before the application section (NSIS
  runs sections in declaration order, so this always completes first). It
  checks the registry for any existing Python 3.x install (HKLM and HKCU, any
  version under `SOFTWARE\Python\PythonCore`). A dialog reports the result
  (Python found at <path>, or Python not found and about to be
  downloaded/installed) - click OK to continue. If none is found, downloads
  Python 3.12.4 from python.org via a hidden PowerShell `Invoke-WebRequest`
  call (with TLS 1.2 forced, to avoid certificate-trust errors on older
  systems) and installs it silently, then reports success/failure with
  another OK dialog before continuing.
- Copies application files, creates a virtual environment under the install
  directory, and installs `requirements.txt` into it. The venv/pip commands run
  hidden via `nsExec::ExecToLog`, so no console window appears - all output is
  written to the installer's own log view.
- Registers the app in Windows "Apps & Features" (with an uninstaller), and
  creates Start Menu + desktop shortcuts that launch the app via
  `run_star_term.bat`. The shortcuts and "Apps & Features" entry use
  `app.ico` (regenerate with `python make_icon.py`), the same icon as the
  app's window/taskbar icon and the Sessions activity-bar button.
- The installer window is brought to the front on launch so it doesn't end
  up hidden behind other open windows.

## Toolbar and activity bar

A toolbar below the menu bar provides quick-access buttons for Connect,
Close Session, the Multi-Exec View on/off toggle, and Settings (gear icon),
which opens the Preferences dialog.

A vertical activity bar on the left edge switches the left-hand panel between:
- **Sessions** (terminal icon) - the session sidebar.
- **Remote Files** (folder icon) - the remote file browser.

## Sessions and settings

Saved sessions are listed in a sidebar on the left (Add/Edit/Remove/Connect),
grouped into folders and sorted alphabetically (folders, then sessions within
each folder). Right-click a session for an Edit/Remove context menu.
Right-click empty space or a folder header for a "New Folder..." option to
create an empty folder; right-click an empty, manually-created folder for a
"Remove Folder" option. Each session stores name, folder, host, port,
username, and whether to authenticate using an SSH key (password is prompted
at connect time and never stored). When SSH key authentication is enabled, a
session uses its own key path if one is set, otherwise falls back to the key
configured in the Preferences dialog's SSH Key tab. Leave the folder field
blank to place a session in "General".

## Remote file browser

The Remote Files panel (shown in the left-hand panel via the activity bar)
browses the current
SSH session's remote filesystem over SFTP:
- Double-click a folder to navigate into it; use the Up and Refresh buttons
  or type a path directly to navigate.
- Drag local files into the list to upload them to the current remote
  directory, or right-click and choose "Upload..." to pick files via a
  dialog.
- Drag files out of the list to download them to a local destination (e.g.
  a file manager window), or right-click one or more files and choose
  "Download..." to pick a destination via a dialog.
- The browser follows the remote shell's current directory as you `cd`
  around (inferred from typed `cd` commands, since the cwd isn't reported by
  the shell protocol). Click "Follow Current Directory" at the bottom of the
  panel to disable this and browse independently.

File menu > Export Sessions... / Import Sessions... lets you save the session
list to a JSON file or load one (import replaces the current list, after
confirmation).

The Settings button on the toolbar (gear icon, also available under the
Settings menu as "Preferences...") opens a tabbed Preferences dialog:
- **General** - switch the application's appearance between Dark (the
  default) and Light themes. Applies immediately and persists across
  restarts.
- **Terminal** - change the terminal font family/size and the cursor style
  (Underline, the default, or Block). These apply to all open sessions and
  persist across restarts.
- **SSH Key** - point at a private key file on disk to use as the default
  key for sessions with key authentication enabled (Browse... sets the path
  directly - no copy of the key is made; Remove just clears the setting and
  leaves the file in place). A session can override this with its own key
  path on its Edit dialog.
- **License** - shows the current trial/license status and lets you enter a
  perpetual license key.

## Trial and licensing

Star Term is fully functional for a 30-day trial period, starting from first
launch. The trial start date is recorded in the Windows registry
(`HKCU\Software\StarTerm`), so it survives deleting `%APPDATA%\star_term\`.

Once the trial ends, the app shows a "License Required" dialog at startup. To
continue using Star Term, enter a perpetual license key (purchased
separately) - this can be done from that startup dialog, or at any time
beforehand from Preferences > License. License keys are verified offline
(cryptographically signed) - no internet connection or license server is
required to activate.

Maintainers issuing license keys to customers should see
`windows/licensing_tool/`.

## Tabs and Multi-Exec

Each Connect (from the File menu or the sidebar) opens a new tab with its own
terminal and SSH session, so multiple sessions can be open concurrently.
Tabs are closable from the tab bar's close button or the "Close" button below
each terminal.

View > Multi-Exec View switches from the tabbed layout to a grid showing all
open sessions simultaneously. While Multi-Exec View is active, keyboard input
typed into any (non-excluded) terminal - including paste (Shift+Insert or
Ctrl+Shift+V) - is sent to every other non-excluded session at the same time.
Use Ctrl+Shift+C to copy a terminal's current screen contents to the
clipboard.

Each session has an "Exclude from Multi-Exec" checkbox below its terminal;
checking it keeps that session's input local even while Multi-Exec View is
active.

Each terminal keeps a scrollback buffer (2000 lines) with a scrollbar on the
right edge. Scroll with the mouse wheel, drag the scrollbar, or use
Shift+PageUp / Shift+PageDown to page through history. If you're scrolled
back, new output is appended below without moving your view; once you scroll
back to the bottom, new output keeps the view pinned there.

Clicking anywhere in a terminal gives it keyboard focus (kept until you click
another widget), and the Tab key is always sent to the shell - useful for
shell tab-completion - instead of moving focus to the next control.

If an SSH session disconnects (error or remote close), a "Reconnect" button
appears next to "Close" on that pane; clicking it re-establishes the
connection with the same parameters.

On first connection to a host, an unrecognized host key shows a confirmation
dialog with the key's SHA256 fingerprint - accept to continue and remember the
key, or reject to abort the connection. Accepted host keys are stored in
`known_hosts` (OpenSSH format) under `%APPDATA%\star_term\`.

Data is stored as JSON/text under `%APPDATA%\star_term\`:
- `sessions.json` - saved session list
- `settings.json` - app settings (appearance theme, default SSH key path, terminal font/size/cursor style)
- `folders.json` - manually-created session folder names
- `known_hosts` - accepted remote host keys (OpenSSH format)

## Status bar

The bottom status bar shows live CPU utilization, CPU load average, RAM
utilization, and swap utilization for the remote host of the currently
selected tab's SSH session, refreshed every 2 seconds. Stats start polling
once a session connects and show "N/A" until the first sample arrives. The
status bar is hidden while Multi-Exec View is active.

## Status

Connection dialog (host/port/username, password or private key + passphrase
auth) with host key fingerprint verification, VT100 terminal rendering with
configurable font/cursor, click-to-focus and Tab-for-completion behavior, a
padded layout, and a scrollback buffer that preserves your scroll position as
new output arrives, SSH sessions over paramiko with reconnect support, tabbed
multi-session support with a Multi-Exec grid view and input broadcast, a
polished Dark/Light-themed UI with a toolbar and activity bar of icon buttons,
session sidebar with folder grouping/sorting, persistent storage, right-click
edit/remove/new folder, and session import/export, an SFTP remote file browser
with drag-and-drop and right-click upload/download and cwd-following, a tabbed
Preferences dialog (General appearance switcher, SSH key path reference with
per-session override, and terminal appearance settings) opened from a toolbar
Settings button, a system status bar, a Help menu with About and Update
History, and an NSIS Windows installer that brings itself to the front on
launch, with a terminal-mockup splash screen, hidden Python bootstrap, and
Apps & Features registration using a shared app icon across the Start
Menu/Desktop shortcuts and the app's own window/taskbar icon.
