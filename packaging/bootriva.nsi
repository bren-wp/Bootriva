Unicode true
!ifndef APP_EXE
  !error "Specify /DAPP_EXE=full-path-to-Bootriva.exe"
!endif
!ifndef OUT_DIR
  !error "Specify /DOUT_DIR=output-folder"
!endif
Name "BOOTRIVA 0.2.3"
OutFile "${OUT_DIR}\BOOTRIVA-Setup.exe"
InstallDir "$PROGRAMFILES64\BOOTRIVA"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles
Section "Install"
  SetOutPath "$INSTDIR"
  File /oname=Bootriva.exe "${APP_EXE}"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\BOOTRIVA"
  CreateShortcut "$SMPROGRAMS\BOOTRIVA\BOOTRIVA.lnk" "$INSTDIR\Bootriva.exe"
  CreateShortcut "$DESKTOP\BOOTRIVA.lnk" "$INSTDIR\Bootriva.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BOOTRIVA" "DisplayName" "BOOTRIVA 0.2.3"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BOOTRIVA" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BOOTRIVA" "Publisher" "BRENDIGO"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BOOTRIVA" "DisplayVersion" "0.2.3"
SectionEnd
Section "Uninstall"
  Delete "$INSTDIR\Bootriva.exe"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$DESKTOP\BOOTRIVA.lnk"
  Delete "$SMPROGRAMS\BOOTRIVA\BOOTRIVA.lnk"
  RMDir "$SMPROGRAMS\BOOTRIVA"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BOOTRIVA"
SectionEnd
