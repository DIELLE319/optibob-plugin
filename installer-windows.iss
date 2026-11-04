; UN solo setup: OptiBob-Windows-Setup.exe. In installazione scegli i componenti
; (Desktop e/o Plugin), con disinstallazione in "App e funzionalita".
; ISCC installer-windows.iss  (accanto a OptiBob_artefacts\)
[Setup]
AppId={{B0B0B0B0-0B0B-4B0B-8B0B-0B0B0B0B0B01}
AppName=OptiBob Live
AppVersion=1.0.0
AppPublisher=OptiBob
AppPublisherURL=https://optibob.com
DefaultDirName={autopf}\OptiBob Live
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename=OptiBob-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayName=OptiBob Live
UninstallDisplayIcon={app}\OptiBob.exe
[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
[Types]
Name: "full"; Description: "OptiBob Live + Plugin"
Name: "custom"; Description: "Custom"; Flags: iscustom
[Components]
Name: "desktop"; Description: "OptiBob Live (app, for your radio)"; Types: full custom
Name: "plugin"; Description: "OptiBob Plugin (VST3, for music software)"; Types: full custom
[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: desktop; Flags: unchecked
[Files]
Source: "OptiBob_artefacts\Release\Standalone\OptiBob.exe"; DestDir: "{app}"; Components: desktop; Flags: ignoreversion
Source: "OptiBob_artefacts\Release\VST3\OptiBob.vst3\*"; DestDir: "{commoncf64}\VST3\OptiBob.vst3"; Components: plugin; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{autoprograms}\OptiBob Live"; Filename: "{app}\OptiBob.exe"; Components: desktop
Name: "{autodesktop}\OptiBob Live"; Filename: "{app}\OptiBob.exe"; Components: desktop; Tasks: desktopicon
[Run]
Filename: "{app}\OptiBob.exe"; Description: "Start OptiBob Live"; Components: desktop; Flags: nowait postinstall skipifsilent
[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\OptiBob.vst3"
