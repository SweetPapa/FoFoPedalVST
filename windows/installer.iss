; installer.iss - Sweet Papa Pedals Windows installer (Inno Setup 6).
; Compiled by windows\build.ps1, which passes:
;   /DAppVersion=<version>  /DStageDir=<staged, signed .vst3 bundles>

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef StageDir
  #error "Pass /DStageDir=<path to staged .vst3 bundles> (use windows\build.ps1)"
#endif

[Setup]
AppName=Sweet Papa Pedals
AppVersion={#AppVersion}
AppPublisher=Sweet Papa Technologies
AppPublisherURL=https://github.com/sweetpapa
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=SweetPapaPedals-Setup-{#AppVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
UninstallDisplayName=Sweet Papa Pedals

[Files]
; VST3 bundles are directories - install them whole into the system VST3 dir.
Source: "{#StageDir}\VROOM.vst3\*";     DestDir: "{commoncf64}\VST3\VROOM.vst3";     Flags: ignoreversion recursesubdirs
Source: "{#StageDir}\DAYDREAM.vst3\*";  DestDir: "{commoncf64}\VST3\DAYDREAM.vst3";  Flags: ignoreversion recursesubdirs
Source: "{#StageDir}\FOFOPEDAL.vst3\*"; DestDir: "{commoncf64}\VST3\FOFOPEDAL.vst3"; Flags: ignoreversion recursesubdirs
Source: "{#StageDir}\DOUBLE.vst3\*";    DestDir: "{commoncf64}\VST3\DOUBLE.vst3";    Flags: ignoreversion recursesubdirs
Source: "{#StageDir}\BACKPORCH.vst3\*"; DestDir: "{commoncf64}\VST3\BACKPORCH.vst3"; Flags: ignoreversion recursesubdirs
Source: "{#StageDir}\SWAY.vst3\*";      DestDir: "{commoncf64}\VST3\SWAY.vst3";      Flags: ignoreversion recursesubdirs
Source: "{#StageDir}\DREAMRIPPER.vst3\*"; DestDir: "{commoncf64}\VST3\DREAMRIPPER.vst3"; Flags: ignoreversion recursesubdirs

[Messages]
WelcomeLabel2=This will install the seven Sweet Papa pedals (VST3) on your computer:%n%nDOUBLE - every take you didn't record%nBACKPORCH - sounds produced, not wet%nSWAY - makes static tracks move like a band%nVROOM - the dirt pedal that lands in the mix%nDAYDREAM - one knob, warm tape to dream%nFOFOPEDAL - twelve characters, one MIX knob%nDREAMRIPPER - grunge and metal, amp in a box%n%nRescan plug-ins in your DAW after installing.
