; Per-user setup for the LED Matrix Controller desktop app (no administrator rights).
;
; It only unpacks the release package to a temp folder and runs
; `led-matrix-desktop.exe --install --launch`, which copies it to
; %LOCALAPPDATA%\led-matrix-desktop, creates the shortcuts and the
; "Installed apps" entry (see shared/desktop/src/shared/desktop/SelfUpdate.cpp).
; The installed app updates itself from then on.
;
; Built by packaging/windows/build_setup.cmake (a CPack post-build script):
;   makensis /DPKG_DIR=<unzipped package> /DOUT_FILE=<exe> /DVERSION=<x.y.z> /DICON=<ico> setup.nsi

Unicode true
SetCompressor /SOLID lzma
RequestExecutionLevel user
ManifestDPIAware true

Name "LED Matrix Controller"
Caption "LED Matrix Controller ${VERSION} Setup"
OutFile "${OUT_FILE}"
Icon "${ICON}"
BrandingText "LED Matrix Controller ${VERSION}"
ShowInstDetails nevershow
AutoCloseWindow true

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "LED Matrix Controller"
VIAddVersionKey "FileDescription" "LED Matrix Controller Setup"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "CompanyName" "sshcrack"
VIAddVersionKey "LegalCopyright" "sshcrack"

Page instfiles

Section "Install"
  DetailPrint "Unpacking LED Matrix Controller ${VERSION}..."
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\pkg"
  File /r "${PKG_DIR}\*.*"
  DetailPrint "Installing for this user..."
  ExecWait '"$PLUGINSDIR\pkg\bin\led-matrix-desktop.exe" --install --launch' $0
  SetOutPath "$TEMP" ; release $PLUGINSDIR so it can be deleted
  StrCmp $0 "0" done
    ; The app already showed the reason in a message box.
    SetErrorLevel $0
    Abort "Installation failed (exit code $0)."
  done:
SectionEnd
