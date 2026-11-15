; The Windows installer, made with Inno Setup by tools/build-windows.sh:
;   ISCC /DSourceDir=<the finished program folder> /DOutDir=<where the installer goes> nachtzug.iss
; It installs for the person installing (no administrator needed), adds a Start menu entry, optionally a desktop
; icon, and opens .nachtzug songs with Nachtzug.

#ifndef SourceDir
  #define SourceDir "..\..\build-windows\package\Nachtzug"
#endif
#ifndef OutDir
  #define OutDir "..\..\build-windows"
#endif

[Setup]
AppId={{6E1C3F4A-8B2D-4E5F-9A10-4E61C47A2711}
AppName=Nachtzug
AppVersion=0.1
AppPublisher=Nachtzug
DefaultDirName={autopf}\Nachtzug
DefaultGroupName=Nachtzug
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#OutDir}
OutputBaseFilename=Nachtzug-Setup
SetupIconFile=nachtzug.ico
UninstallDisplayIcon={app}\Nachtzug.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Nachtzug"; Filename: "{app}\Nachtzug.exe"
Name: "{autodesktop}\Nachtzug"; Filename: "{app}\Nachtzug.exe"; Tasks: desktopicon

[Registry]
; .nachtzug songs open in Nachtzug
Root: HKA; Subkey: "Software\Classes\.nachtzug"; ValueType: string; ValueName: ""; ValueData: "Nachtzug.Song"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Nachtzug.Song"; ValueType: string; ValueName: ""; ValueData: "Nachtzug song"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Nachtzug.Song\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Nachtzug.exe,0"
Root: HKA; Subkey: "Software\Classes\Nachtzug.Song\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Nachtzug.exe"" ""%1"""

[Run]
Filename: "{app}\Nachtzug.exe"; Description: "{cm:LaunchProgram,Nachtzug}"; Flags: nowait postinstall skipifsilent
