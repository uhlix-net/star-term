!define APPNAME "star_term"
!define DISPLAYNAME "Star Term"
!define VERSION "0.2.0"
!define PUBLISHER "uhlix.net"
!define PYTHON_URL "https://www.python.org/ftp/python/3.12.4/python-3.12.4-amd64.exe"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

!include "MUI2.nsh"
!include "LogicLib.nsh"

!define MUI_ICON "app.ico"

Name "${APPNAME}"
OutFile "Output\star_term_setup.exe"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKLM "${UNINSTKEY}" "InstallLocation"
RequestExecutionLevel admin

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  SetRegView 64

  ; Splash screen: appears instantly on top of all windows for 3 seconds
  ; (no fade), then continues automatically.
  InitPluginsDir
  File /oname=$PLUGINSDIR\splash.bmp "splash.bmp"
  Splash::show 3000 $PLUGINSDIR\splash

  ; Bring the installer window to the front - the splash screen can leave
  ; it behind other open windows.
  BringToFront

  ; If already installed, ask whether to install/update or cancel
  ReadRegStr $R0 HKLM "${UNINSTKEY}" "DisplayVersion"
  StrCmp $R0 "" not_installed
  MessageBox MB_YESNO|MB_ICONQUESTION "${APPNAME} version $R0 is already installed.$\n$\nDo you want to install/update it?" IDYES not_installed
  Abort
  not_installed:
FunctionEnd

; Looks for any installed Python 3.x under HKLM/HKCU "SOFTWARE\Python\PythonCore"
; and returns its install directory in $0, or "" if none was found.
Function FindPython
  Push $1
  Push $2
  Push $3

  StrCpy $0 ""
  StrCpy $2 0
  fp_hklm_loop:
    EnumRegKey $1 HKLM "SOFTWARE\Python\PythonCore" $2
    StrCmp $1 "" fp_hkcu
    ReadRegStr $3 HKLM "SOFTWARE\Python\PythonCore\$1\InstallPath" ""
    IfFileExists "$3\python.exe" fp_found fp_hklm_next
    fp_hklm_next:
    IntOp $2 $2 + 1
    Goto fp_hklm_loop

  fp_hkcu:
    StrCpy $2 0
  fp_hkcu_loop:
    EnumRegKey $1 HKCU "SOFTWARE\Python\PythonCore" $2
    StrCmp $1 "" fp_done
    ReadRegStr $3 HKCU "SOFTWARE\Python\PythonCore\$1\InstallPath" ""
    IfFileExists "$3\python.exe" fp_found fp_hkcu_next
    fp_hkcu_next:
    IntOp $2 $2 + 1
    Goto fp_hkcu_loop

  fp_found:
    StrCpy $0 $3
    Goto fp_done

  fp_done:
  Pop $3
  Pop $2
  Pop $1
FunctionEnd

; Runs before Section "Application" (sections execute in declaration order),
; so the Python dependency check always completes before any app files
; are copied.
Section "-Python Check"
  SetRegView 64

  DetailPrint "Checking for Python..."
  Call FindPython
  ${If} $0 != ""
    DetailPrint "Found Python at $0"
    MessageBox MB_OK|MB_ICONINFORMATION "Python found at:$\n$0$\n$\nClick OK to continue installing ${APPNAME}."
  ${Else}
    DetailPrint "Python not found."
    MessageBox MB_OK|MB_ICONINFORMATION "Python was not found on this computer.$\n$\n${APPNAME} will now download and install Python 3.12. This may take a few minutes.$\n$\nClick OK to continue."
    DetailPrint "Downloading Python 3.12 installer..."
    nsExec::ExecToLog `powershell -NoProfile -ExecutionPolicy Bypass -Command "$$ProgressPreference='SilentlyContinue'; [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '${PYTHON_URL}' -OutFile '$TEMP\python-installer.exe' -UseBasicParsing -ErrorAction Stop"`
    Pop $1
    ${If} $1 == "0"
      DetailPrint "Installing Python 3.12 (this may take a minute)..."
      ExecWait '"$TEMP\python-installer.exe" /quiet InstallAllUsers=1 PrependPath=1 Include_test=0'
      Delete "$TEMP\python-installer.exe"
      Call FindPython
      ${If} $0 != ""
        DetailPrint "Python 3.12 installed successfully at $0. Continuing with ${APPNAME} install..."
        MessageBox MB_OK|MB_ICONINFORMATION "Python 3.12 was installed successfully.$\n$\nClick OK to continue installing ${APPNAME}."
      ${Else}
        DetailPrint "Python could not be installed automatically."
        DetailPrint "${APPNAME} will still be installed, but you must install Python 3.10+"
        DetailPrint "manually, then run from $INSTDIR:"
        DetailPrint "  python -m venv venv"
        DetailPrint "  venv\Scripts\pip install -r requirements.txt"
        MessageBox MB_OK|MB_ICONEXCLAMATION "Python could not be installed automatically.$\n$\n${APPNAME} will still be installed, but you must install Python 3.10+ manually and then run, from the install folder:$\n$\n  python -m venv venv$\n  venv\Scripts\pip install -r requirements.txt$\n$\nClick OK to continue."
      ${EndIf}
    ${Else}
      DetailPrint "Failed to download Python installer (exit code $1)."
      DetailPrint "${APPNAME} will still be installed, but you must install Python 3.10+"
      DetailPrint "manually, then run from $INSTDIR:"
      DetailPrint "  python -m venv venv"
      DetailPrint "  venv\Scripts\pip install -r requirements.txt"
      MessageBox MB_OK|MB_ICONEXCLAMATION "Failed to download the Python installer (exit code $1).$\n$\n${APPNAME} will still be installed, but you must install Python 3.10+ manually and then run, from the install folder:$\n$\n  python -m venv venv$\n  venv\Scripts\pip install -r requirements.txt$\n$\nClick OK to continue."
    ${EndIf}
  ${EndIf}
SectionEnd

Section "Application" SecApp
  SetRegView 64

  ; --- Application installation ---
  SetOutPath "$INSTDIR\star_term"
  File "..\star_term\*.py"
  File "..\star_term\*.bmp"

  SetOutPath "$INSTDIR"
  File "..\requirements.txt"
  File "run_star_term.bat"
  File "app.ico"

  ${If} $0 != ""
    DetailPrint "Creating virtual environment..."
    nsExec::ExecToLog '"$0\python.exe" -m venv "$INSTDIR\venv"'
    Pop $1
    DetailPrint "Installing dependencies..."
    nsExec::ExecToLog '"$INSTDIR\venv\Scripts\python.exe" -m pip install -r "$INSTDIR\requirements.txt"'
    Pop $1
  ${EndIf}

  ; Shortcuts (use app.ico - the same icon as the app's Sessions
  ; activity-bar button and taskbar/window icon)
  CreateDirectory "$SMPROGRAMS\${DISPLAYNAME}"
  CreateShortcut "$SMPROGRAMS\${DISPLAYNAME}\${DISPLAYNAME}.lnk" "$INSTDIR\run_star_term.bat" "" "$INSTDIR\app.ico" 0
  CreateShortcut "$SMPROGRAMS\${DISPLAYNAME}\Uninstall ${DISPLAYNAME}.lnk" "$INSTDIR\Uninstall.exe" "" "" 0
  CreateShortcut "$DESKTOP\${DISPLAYNAME}.lnk" "$INSTDIR\run_star_term.bat" "" "$INSTDIR\app.ico" 0

  ; Uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Register with Windows "Apps & Features"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\app.ico"
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetRegView 64
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\${DISPLAYNAME}\${DISPLAYNAME}.lnk"
  Delete "$SMPROGRAMS\${DISPLAYNAME}\Uninstall ${DISPLAYNAME}.lnk"
  RMDir "$SMPROGRAMS\${DISPLAYNAME}"
  Delete "$DESKTOP\${DISPLAYNAME}.lnk"
  DeleteRegKey HKLM "${UNINSTKEY}"
SectionEnd
