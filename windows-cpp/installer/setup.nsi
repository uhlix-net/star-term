!define APPNAME "star_term_cpp"
!define DISPLAYNAME "Star Term C++ Edition"
!define VERSION "0.3.0"
!define PUBLISHER "uhlix.net"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

!include "MUI2.nsh"
!include "LogicLib.nsh"

!define MUI_ICON "app.ico"

Name "${APPNAME}"
OutFile "Output\star_term_cpp_setup.exe"
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

  ; If already installed, ask whether to install/update or cancel
  ReadRegStr $R0 HKLM "${UNINSTKEY}" "DisplayVersion"
  StrCmp $R0 "" not_installed
  MessageBox MB_YESNO|MB_ICONQUESTION "${APPNAME} version $R0 is already installed.$\n$\nDo you want to install/update it?" IDYES not_installed
  Abort
  not_installed:
FunctionEnd

Section "Application" SecApp
  SetRegView 64

; --- Application binary + all runtime files (Qt, plugins, vcpkg DLLs) ---
  ; Packages everything windeployqt + vcpkg staged in build\Release.
  SetOutPath "$INSTDIR"
  File /r /x *.pdb /x *.lib /x *.exp "..\build\Release\*.*"
  File "app.ico"
  File "run_star_term_cpp.bat"

  ; Shortcuts
  CreateDirectory "$SMPROGRAMS\${DISPLAYNAME}"
  CreateShortcut "$SMPROGRAMS\${DISPLAYNAME}\${DISPLAYNAME}.lnk" \
    "$INSTDIR\star_term_cpp.exe" "" "$INSTDIR\app.ico" 0
  CreateShortcut "$SMPROGRAMS\${DISPLAYNAME}\Uninstall ${DISPLAYNAME}.lnk" \
    "$INSTDIR\Uninstall.exe" "" "" 0
  CreateShortcut "$DESKTOP\${DISPLAYNAME}.lnk" \
    "$INSTDIR\star_term_cpp.exe" "" "$INSTDIR\app.ico" 0

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
