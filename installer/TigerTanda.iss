; Tiger Tanda — Inno Setup installer script
; Builds a single-exe installer that places TigerTanda.dll + metadata.csv
; into {localappdata}\VirtualDJ\Plugins64\SoundEffect\TigerTanda by default.

#define MyAppName      "Tiger Tanda"
#define MyAppVersion   "1.0.4"
#define MyAppPublisher "Sean Ericson"
#define MyAppURL       "https://github.com/sericson0/tigertanda-vdj"

[Setup]
AppId={{E7A3F1B2-4C5D-4E6F-8A9B-0C1D2E3F4A5B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppSupportURL={#MyAppURL}
DefaultDirName={localappdata}\VirtualDJ\Plugins64\SoundEffect\TigerTanda
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=..\build
OutputBaseFilename=TigerTanda-Windows-Installer
Compression=lzma
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\TigerTanda.dll

[Files]
Source: "..\build\Release\TigerTanda.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\metadata.csv";   DestDir: "{app}"; Flags: ignoreversion

[Messages]
SelectDirLabel3=Tiger Tanda will be installed into the following folder.%nThis should be inside your VirtualDJ Plugins64\SoundEffect directory.
