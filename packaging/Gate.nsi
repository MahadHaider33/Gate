Unicode True
!include "MUI2.nsh"
Name "Gate"
OutFile "..\dist\Gate-0.1.1-setup-x64.exe"
InstallDir "$LOCALAPPDATA\Programs\Gate"
RequestExecutionLevel user
SetCompressor /SOLID lzma
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Function .onInit
  FindWindow $0 "GateDesktopWindow"
  StrCmp $0 0 done
    MessageBox MB_OK "Exit Gate from its tray menu before installing." /SD IDOK
    Abort
  done:
FunctionEnd
Section "Gate"
  SetOutPath "$INSTDIR"
  File "..\dist\Gate\Gate.exe"
  File "..\dist\Gate\LICENSE"
  File "..\dist\Gate\README.md"
  File "..\dist\Gate\THIRD_PARTY_NOTICES.md"
  File "..\dist\Gate\dependencies.lock.json"
  File "..\dist\Gate\build-metadata.json"
  SetOutPath "$INSTDIR\docs"
  File "..\dist\Gate\docs\*.md"
  SetOutPath "$INSTDIR\licenses"
  File "..\dist\Gate\licenses\*.txt"
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortcut "$SMPROGRAMS\Gate.lnk" "$INSTDIR\Gate.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gate" "DisplayName" "Gate"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gate" "DisplayVersion" "0.1.1"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gate" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gate" "DisplayIcon" "$INSTDIR\Gate.exe"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gate" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gate" "NoRepair" 1
SectionEnd
Function un.onInit
  FindWindow $0 "GateDesktopWindow"
  StrCmp $0 0 done
    MessageBox MB_OK "Exit Gate from its tray menu before uninstalling." /SD IDOK
    Abort
  done:
FunctionEnd
Section "Uninstall"
  Delete "$SMPROGRAMS\Gate.lnk"
  Delete "$INSTDIR\Gate.exe"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\THIRD_PARTY_NOTICES.md"
  Delete "$INSTDIR\dependencies.lock.json"
  Delete "$INSTDIR\build-metadata.json"
  Delete "$INSTDIR\docs\ARCHITECTURE.md"
  Delete "$INSTDIR\docs\VERIFICATION.md"
  Delete "$INSTDIR\docs\BUILD_REPORT.md"
  RMDir "$INSTDIR\docs"
  Delete "$INSTDIR\licenses\RNNoise.txt"
  Delete "$INSTDIR\licenses\SpeexDSP.txt"
  Delete "$INSTDIR\licenses\Cycfi-Q.txt"
  Delete "$INSTDIR\licenses\Cycfi-infra.txt"
  RMDir "$INSTDIR\licenses"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gate"
  MessageBox MB_YESNO "Also remove Gate preferences? VB-CABLE will be left installed." /SD IDNO IDNO keep
    DeleteRegKey HKCU "Software\Gate"
  keep:
SectionEnd
