#define MyAppName "MicMute-S"
#define MyAppVersion "1.2.2"
#define MyAppPublisher "Suvojeet Sengupta"
#define MyAppExeName "MicMute-S.exe"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
AppId={{A3FE2123-4567-89AB-CDEF-0123456789AB}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
; Remove the following line to run in administrative install mode (install for all users.)
PrivilegesRequired=lowest
OutputDir=installer
OutputBaseFilename=MicMute-S-Setup
; SetupIconFile removed - using default Inno Setup icon
Compression=lzma
SolidCompression=yes
WizardStyle=modern
WizardSizePercent=120
CloseApplications=force
RestartApplications=yes

LicenseFile=TERMS.txt
InfoBeforeFile=TERMS.txt
SetupIconFile=resources\app.ico

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nDeveloped by Suvojeet Sengupta.%n%nIt is recommended that you close all other applications before continuing.
ClickFinish=Click Finish to exit Setup. Thank you for choosing MicMute-S by Suvojeet Sengupta.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: "Automatically start MicMute-S on Windows startup"; GroupDescription: "Additional Options:"; Flags: unchecked

[Files]
; IMPORTANT: The build pipeline must place the exe in the root or build folder.
Source: "build\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "MicMute-S"; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletevalue; Tasks: startup
