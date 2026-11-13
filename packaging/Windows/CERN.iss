; Inno Setup Script Template for C.E.R.N
; Anpassungen vor dem Build: AppVersion, Source-Pfade, SignTool (optional)

#define MyAppName "C.E.R.N"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "ScalyxSoft"
#define MyAppURL "https://scalyxsoft.com"
#define MyAppExeName "C.E.R.N Standalone.exe"

[Setup]
AppId={{8F3D4C9A-1B2E-4D5F-9A7C-0000CERNVST3}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; Lizenzdatei hier eintragen, falls vorhanden:
; LicenseFile=..\..\LICENSE
OutputDir=..\..\dist\windows
OutputBaseFilename=CERN-{#MyAppVersion}-Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Pfade relativ zu diesem .iss (packaging/Windows). Build-Artefakte vorher erzeugen.
Source: "..\..\build\Source\CERNStandalone_artefacts\Release\Standalone\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; VST3-Bundle als Ordner installieren:
Source: "..\..\build\Source\CERN_artefacts\Release\VST3\CERN.vst3\*"; DestDir: "{commoncf64}\VST3\CERN.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

; Optional: SignTool-Konfiguration in Inno Setup IDE/Compiler einstellen
; SignTool=mysigntool signtool.exe sign /fd sha256 /tr http://timestamp.digicert.com /td sha256 $f
; SignedUninstaller=yes
