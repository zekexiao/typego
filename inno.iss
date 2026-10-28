; TypeGo Windows 安装包

#define MyAppName "TypeGo"
#define MyAppVersion "1.8"
#define MyAppPublisher "Linger Me, Inc."
#define MyAppURL "https://typego.app"
#define MyAppExeName "TypeGo.exe"
#define ProjectDir SourcePath
#define BuildBinDir AddBackslash(ProjectDir) + "build\bin"
#define AppExePath AddBackslash(BuildBinDir) + MyAppExeName
#define QtBinDir "C:\Qt\6.10.3\msvc2022_64\bin"
#define WinDeployQt AddBackslash(QtBinDir) + "windeployqt.exe"

#if !FileExists(AppExePath)
  #error "未找到 build\bin\TypeGo.exe，请先运行 build.bat。"
#endif

#if !FileExists(WinDeployQt)
  #error "未找到 windeployqt.exe，请检查 inno.iss 中的 QtBinDir。"
#endif

; 打包前部署 Qt 运行库和插件，确保安装后可独立运行。
#if Exec(WinDeployQt, "--release --no-translations --compiler-runtime """ + AppExePath + """", BuildBinDir, 1, SW_HIDE) != 0
  #error "windeployqt 执行失败。"
#endif

[Setup]
AppId={{6D89C8A8-0F94-446A-8E20-999BAA089BA4}
AppName={#MyAppName}
AppMutex=TypeGo.Installation
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename={#MyAppName}_{#MyAppVersion}_x64
SetupIconFile=assets\icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern dynamic

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
#if FileExists(AddBackslash(CompilerPath) + "Languages\ChineseSimplified.isl")
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
#endif

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#BuildBinDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "third_party\everything-sdk\dll\Everything64.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
