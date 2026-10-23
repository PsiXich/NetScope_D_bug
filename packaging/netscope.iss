; NetScope v1.1 installer. Compile with Inno Setup 7.
; PackageDir can be overridden with ISCC /DPackageDir="...".
; Keep AppId unchanged to upgrade installations of v1.0.
#define MyAppName "NetScope"
#define MyAppVersion "1.1"
#define MyAppExeName "netscope.exe"
#ifndef PackageDir
  #define PackageDir "D:\Progects\NetScope\NetScopeDeBug\build\packages\20261022-235100\NetScope-v1.1-windows-x64"
#endif
#ifndef InstallerOutputDir
  #define InstallerOutputDir "D:\Installer"
#endif
#if !FileExists(PackageDir + "\netscope.exe")
  #error Missing netscope.exe: build the portable package first or set PackageDir.
#endif
#if !FileExists(PackageDir + "\Qt6Mqtt.dll")
  #error Missing Qt6Mqtt.dll: use the complete v1.1 deployment directory.
#endif
#if !FileExists(PackageDir + "\platforms\qwindows.dll")
  #error Missing platforms/qwindows.dll.
#endif

[Setup]
AppId={{5BAAA525-5F70-47B7-9BD6-A9C95217C5C4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
VersionInfoVersion=1.1.0.0
AppPublisher=Artyom (PsiXich)
AppPublisherURL=https://github.com/PsiXich/NetScope_D_bug
AppSupportURL=https://github.com/PsiXich/NetScope_D_bug/issues
DefaultDirName={autopf}\{#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
DisableProgramGroupPage=yes
LicenseFile={#PackageDir}\LICENSE
OutputDir={#InstallerOutputDir}
OutputBaseFilename=NetScope_v1.1_Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern dark
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Deploy the whole tested runtime, including qt.conf, DLLs, plugins and licenses.
; These are application-local files, never shared system DLLs.
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
; Remove only the unused file association created by the v1.0 wizard script.
Root: HKA; Subkey: "Software\Classes\.myp\OpenWithProgids"; ValueName: "NetScopeFile.myp"; Flags: deletevalue
Root: HKA; Subkey: "Software\Classes\NetScopeFile.myp"; Flags: deletekey

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent runasoriginaluser
