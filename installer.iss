[Setup]
AppId={{7C3B1E52-94A6-4D0F-8E21-5B6A0C9D3F47}
AppName=INJUSTICE DAW
AppVersion=0.9.37
AppPublisher=Injustice Audio
DefaultDirName={autopf}\INJUSTICE DAW
DefaultGroupName=INJUSTICE DAW
OutputDir=installer_out
OutputBaseFilename=INJUSTICE-DAW-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\INJUSTICE DAW.exe
WizardStyle=modern
SetupIconFile=assets\icon.ico

[Files]
Source: "build\src\InjusticeDAW_artefacts\Release\INJUSTICE DAW.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\INJUSTICE DAW"; Filename: "{app}\INJUSTICE DAW.exe"
Name: "{autodesktop}\INJUSTICE DAW"; Filename: "{app}\INJUSTICE DAW.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; Flags: unchecked

[Run]
Filename: "{app}\INJUSTICE DAW.exe"; Description: "Abrir INJUSTICE DAW"; Flags: nowait postinstall skipifsilent
