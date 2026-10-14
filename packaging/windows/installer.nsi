; Engine Simulator installer for Windows (NSIS 3)
;
;   makensis /DVERSION=0.3.0 /DSOURCE_DIR=<staged install> /DOUTFILE=<setup.exe> installer.nsi
;
; Installs for the current user (no administrator rights needed) into
; %LOCALAPPDATA%\Programs\Engine Simulator, which also lets the application
; update itself in place.

Unicode true
SetCompressor /SOLID lzma

!ifndef VERSION
  !error "VERSION is not defined"
!endif
!ifndef SOURCE_DIR
  !error "SOURCE_DIR is not defined"
!endif
!ifndef NUMERIC_VERSION
  !define NUMERIC_VERSION "${VERSION}"     ; must be X.Y.Z
!endif
!ifndef OUTFILE
  !define OUTFILE "engine-sim-${VERSION}-windows-x64-setup.exe"
!endif

!define APP_NAME "Engine Simulator"
!define APP_EXE "bin\engine-sim-app.exe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\EngineSimulator"
!define PROJECT_URL "https://github.com/kyan0s-git/engine-sim"

Name "${APP_NAME}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\${APP_NAME}"
InstallDirRegKey HKCU "${UNINSTALL_KEY}" "InstallLocation"
RequestExecutionLevel user
ShowInstDetails nevershow
ShowUninstDetails nevershow
BrandingText "${APP_NAME} ${VERSION}"

VIProductVersion "${NUMERIC_VERSION}.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${NUMERIC_VERSION}"
VIAddVersionKey "FileDescription" "${APP_NAME} installer"
VIAddVersionKey "LegalCopyright" "MIT license"

!include "MUI2.nsh"

!define MUI_ICON "../icons/engine-sim.ico"
!define MUI_UNICON "../icons/engine-sim.ico"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Start ${APP_NAME}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE_DIR}/LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "!${APP_NAME}" SecMain
  SectionIn RO

  ; Replace the previous version's program files (user data lives elsewhere)
  RMDir /r "$INSTDIR\bin"
  RMDir /r "$INSTDIR\assets"
  RMDir /r "$INSTDIR\es"
  RMDir /r "$INSTDIR\delta"
  RMDir /r "$INSTDIR\docs"

  SetOutPath "$INSTDIR"
  File /r "${SOURCE_DIR}/*"

  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk" "$INSTDIR\uninstall.exe"

  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "Engine Simulator contributors"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegStr HKCU "${UNINSTALL_KEY}" "URLInfoAbout" "${PROJECT_URL}"
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "The simulator, engine library and documentation."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Adds a shortcut to the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  ; Only the files the installer created; settings and custom engines in
  ; %APPDATA%\EngineSimulator are kept
  RMDir /r "$INSTDIR\bin"
  RMDir /r "$INSTDIR\assets"
  RMDir /r "$INSTDIR\es"
  RMDir /r "$INSTDIR\delta"
  RMDir /r "$INSTDIR\docs"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\CHANGELOG.md"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"
  Delete "$DESKTOP\${APP_NAME}.lnk"

  DeleteRegKey HKCU "${UNINSTALL_KEY}"
SectionEnd
