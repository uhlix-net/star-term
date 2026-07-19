!define APPNAME "star_term"
!define DISPLAYNAME "Star Term"
!define VERSION "0.3.2"
!define PUBLISHER "uhlix.net"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinMessages.nsh"

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

Function .onInit
  SetRegView 64

  ; Bring the installer window to the front so it isn't left behind
  ; other open windows.
  BringToFront

  ; Check if the application is currently running and offer to close it.
    FindWindow $0 "" "Star Term"
    IntCmp $0 0 not_running app_running app_running

  app_running:
    MessageBox MB_YESNO|MB_ICONQUESTION \
      "Star Term is currently running.$\n$\nThe installer must close it before continuing.$\n$\nClose Star Term now?" \
      IDYES close_app IDNO abort_install

  close_app:
    SendMessage $0 ${WM_CLOSE} 0 0
    Sleep 2000
    ; Verify it actually closed; if not, warn and abort.
    FindWindow $0 "" "Star Term"
    IntCmp $0 0 not_running still_running still_running

  still_running:
    MessageBox MB_OK|MB_ICONEXCLAMATION \
      "Star Term could not be closed automatically.$\n$\nPlease close it manually and run the installer again."
    Abort

  abort_install:
    Abort

  not_running:

  ; If already installed, ask whether to install/update or cancel
  ReadRegStr $R0 HKLM "${UNINSTKEY}" "DisplayVersion"
  StrCmp $R0 "" not_installed
  MessageBox MB_YESNO|MB_ICONQUESTION "${DISPLAYNAME} version $R0 is already installed.$\n$\nDo you want to install/update it?" IDYES not_installed
  Abort
  not_installed:
FunctionEnd

Section "Application" SecApp
  SetRegView 64

  ; Remove old "Star Term C++ Edition" Start menu entries left by previous installs.
  Delete "$SMPROGRAMS\Star Term C++ Edition\Star Term C++ Edition.lnk"
  Delete "$SMPROGRAMS\Star Term C++ Edition\Uninstall Star Term C++ Edition.lnk"
  RMDir  "$SMPROGRAMS\Star Term C++ Edition"
  Delete "$DESKTOP\Star Term C++ Edition.lnk"

; --- Application binary + all runtime files (Qt, plugins, vcpkg DLLs) ---
  ; Packages everything windeployqt + vcpkg staged in build\Release.
  SetOutPath "$INSTDIR"
  File /r /x *.pdb /x *.lib /x *.exp "..\build\Release\*.*"
  File "app.ico"
  File "run_star_term.bat"

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

  ; Force a synchronous shell icon cache flush (SHCNE_ASSOCCHANGED | SHCNF_FLUSH)
  System::Call "Shell32::SHChangeNotify(l 0x8000000, l 0x1000, p 0, p 0)"

  ; Uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Register with Windows "Apps & Features"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName"      "${DISPLAYNAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString"  '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation"  "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher"        "${PUBLISHER}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion"   "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon"      "$INSTDIR\app.ico"
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
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
