!define APPNAME "star_term"
!define DISPLAYNAME "Star Term"
!define VERSION "0.8.0"
!define PUBLISHER "uhlix.net"
; Use a C++-specific key so we don't collide with the Python edition's "star_term" entry
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\StarTermCpp"

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinMessages.nsh"
!include "FileFunc.nsh"

; "1" when Star Term was running as this installer started, so it should be
; brought back up afterwards. Set in .onInit, read in .onInstSuccess.
Var WasRunning

!define MUI_ICON "app.ico"

Name "${DISPLAYNAME}"
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

; Pushes "1" when star_term.exe is running, "0" otherwise.
; find returns 0 when it matches a line, 1 when it does not.
Function IsAppRunning
  nsExec::ExecToStack 'cmd.exe /c tasklist /FI "IMAGENAME eq star_term.exe" /NH | find /I "star_term.exe"'
  Pop $R0   ; exit code
  Pop $R1   ; captured output (unused)
  StrCmp $R0 "0" running notrunning
  running:
    Push "1"
    Return
  notrunning:
    Push "0"
FunctionEnd

Function .onInit
  SetRegView 64

  StrCpy $WasRunning "0"

  ; The in-app updater passes /RELAUNCH. It quits the running copy immediately
  ; before starting us, so the process check below can race with that exit and
  ; come back "not running" — the flag settles it.
  ${GetParameters} $R2
  ClearErrors
  ${GetOptions} $R2 "/RELAUNCH" $R3
  ${IfNot} ${Errors}
    StrCpy $WasRunning "1"
  ${EndIf}

  ; Bring the installer window to the front so it isn't left behind
  ; other open windows.
  BringToFront

  ; Check if the application is currently running and offer to close it.
  ; The main window title now tracks the active session ("user@host - Star
  ; Term"), so FindWindow against a fixed title no longer matches — detect the
  ; process by image name instead.
    Call IsAppRunning
    Pop $0
    StrCmp $0 "0" not_running

  app_running:
    ; Running now, so put it back afterwards — however this installer was started.
    StrCpy $WasRunning "1"

    ; /SD IDYES: a silent install is the in-app updater, which already asked the
    ; user and is itself the running copy — prompting there would hang unseen.
    MessageBox MB_YESNO|MB_ICONQUESTION \
      "Star Term is currently running.$\n$\nThe installer must close it before continuing.$\n$\nClose Star Term now?" \
      /SD IDYES IDYES close_app IDNO abort_install

  close_app:
    ; taskkill without /F posts WM_CLOSE, so the app shuts down cleanly and
    ; still gets to save its window state.
    nsExec::Exec 'taskkill.exe /IM star_term.exe'
    Pop $0
    Sleep 2000
    Call IsAppRunning
    Pop $0
    StrCmp $0 "0" not_running

  still_running:
    ; Graceful close failed — force terminate and clean up silently
    nsExec::Exec 'taskkill.exe /F /IM star_term.exe'
    Pop $0
    Sleep 500
    RMDir /r "$TEMP\star_term_*"
    Goto not_running

  abort_install:
    Abort

  not_running:

  ; If already installed, ask whether to install/update or cancel
  ReadRegStr $R0 HKLM "${UNINSTKEY}" "DisplayVersion"
  StrCmp $R0 "" not_installed
  MessageBox MB_YESNO|MB_ICONQUESTION "${DISPLAYNAME} version $R0 is already installed.$\n$\nInstall version ${VERSION} now?" /SD IDYES IDYES not_installed
  Abort
  not_installed:
FunctionEnd

; Relaunch only if Star Term was open when this installer started — whether that
; was the in-app updater (which passes /RELAUNCH) or the user running the
; downloaded installer over a running copy. Installing while it is closed leaves
; it closed. Launching through explorer.exe hands the process to the shell, which
; runs it at the user's own integrity level instead of inheriting the installer's
; admin token.
Function .onInstSuccess
  ${If} $WasRunning == "1"
    Exec '"$WINDIR\explorer.exe" "$INSTDIR\star_term.exe"'
  ${EndIf}
FunctionEnd

Section "Application" SecApp
  SetRegView 64

  ; Remove old "Star Term C++ Edition" Start menu entries left by previous installs.
  Delete "$SMPROGRAMS\Star Term C++ Edition\Star Term C++ Edition.lnk"
  Delete "$SMPROGRAMS\Star Term C++ Edition\Uninstall Star Term C++ Edition.lnk"
  RMDir  "$SMPROGRAMS\Star Term C++ Edition"
  Delete "$DESKTOP\Star Term C++ Edition.lnk"

  ; Remove old "star_term" uninstall registry key left by earlier C++ installers.
  ; (That key name collides with the Python edition; we now use StarTermCpp.)
  ReadRegStr $R1 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\star_term" "InstallLocation"
  StrCmp $R1 "$INSTDIR" 0 skip_old_key_cleanup
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\star_term"
  skip_old_key_cleanup:

; --- Application binary + all runtime files (Qt, plugins, vcpkg DLLs) ---
  ; Packages everything windeployqt + vcpkg staged in build\Release.
  SetOutPath "$INSTDIR"
  File /r /x *.pdb /x *.lib /x *.exp "..\build\Release\*.*"
  File "app.ico"
  File "run_star_term.bat"

  ; Uninstaller and registration come first. They used to sit after the shell
  ; notify below, which meant a failure there took them down with it — see the
  ; comment on SHChangeNotify.
  ClearErrors
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  ${If} ${Errors}
    DetailPrint "ERROR: could not write $INSTDIR\Uninstall.exe"
    ${IfNot} ${Silent}
      MessageBox MB_OK|MB_ICONEXCLAMATION \
        "Could not write the uninstaller to:$\n$INSTDIR\Uninstall.exe$\n$\nThe application is installed, but will not appear in Apps & Features."
    ${EndIf}
  ${EndIf}

  ; Register with Windows "Apps & Features"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName"      "${DISPLAYNAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString"  '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation"  "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher"        "${PUBLISHER}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion"   "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon"      "$INSTDIR\app.ico"
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1

  ; Delete existing shortcuts before recreating so the .lnk icon cache is flushed
  Delete "$SMPROGRAMS\${DISPLAYNAME}\${DISPLAYNAME}.lnk"
  Delete "$SMPROGRAMS\${DISPLAYNAME}\Uninstall ${DISPLAYNAME}.lnk"
  Delete "$DESKTOP\${DISPLAYNAME}.lnk"

  ; Shortcuts — use exe as icon source so the embedded resource is read directly
  CreateDirectory "$SMPROGRAMS\${DISPLAYNAME}"
  CreateShortcut "$SMPROGRAMS\${DISPLAYNAME}\${DISPLAYNAME}.lnk" \
    "$INSTDIR\star_term.exe" "" "$INSTDIR\star_term.exe" 0
  CreateShortcut "$SMPROGRAMS\${DISPLAYNAME}\Uninstall ${DISPLAYNAME}.lnk" \
    "$INSTDIR\Uninstall.exe" "" "" 0
  CreateShortcut "$DESKTOP\${DISPLAYNAME}.lnk" \
    "$INSTDIR\star_term.exe" "" "$INSTDIR\star_term.exe" 0

  ; Shell icon cache refresh (SHCNE_ASSOCCHANGED | SHCNF_FLUSHNOWAIT).
  ;
  ; Both fixes here matter. SHChangeNotify takes LONG + UINT, which are 4 bytes
  ; each; the System plugin's "l" is int64, so passing "l" pushed 8 bytes for
  ; each on a 32-bit installer and left the stack unbalanced after this stdcall
  ; returned. And SHCNF_FLUSH blocks until the shell has processed the event.
  ; Between them this call took the installer down mid-section: files and
  ; shortcuts landed, everything after it did not, and .onInstSuccess — which
  ; is what relaunches Star Term — never ran.
  System::Call "Shell32::SHChangeNotify(i 0x8000000, i 0x2000, p 0, p 0)"
SectionEnd

Section "Uninstall"
  SetRegView 64
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\${DISPLAYNAME}\${DISPLAYNAME}.lnk"
  Delete "$SMPROGRAMS\${DISPLAYNAME}\Uninstall ${DISPLAYNAME}.lnk"
  RMDir  "$SMPROGRAMS\${DISPLAYNAME}"
  Delete "$DESKTOP\${DISPLAYNAME}.lnk"
  DeleteRegKey HKLM "${UNINSTKEY}"
SectionEnd
