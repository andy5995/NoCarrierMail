#define AppName    "NoCarrierMail"
#define AppExe     "ncmail.exe"
#define Publisher  "NoCarrierMail"
#define AppURL     "https://github.com/andy5995/NoCarrierMail"
#define IssuesURL  "https://github.com/andy5995/NoCarrierMail/issues"

#ifndef VERSION
  #define VERSION "0.0.0"
#endif
#ifndef ARCH
  #define ARCH "x86_64"
#endif

[Setup]
AppName={#AppName}
AppVersion={#VERSION}
AppPublisher={#Publisher}
AppPublisherURL={#AppURL}
AppSupportURL={#IssuesURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
; Output goes to the repo root (script is in packaging/windows/)
OutputDir=..\..\
OutputBaseFilename=NoCarrierMail-{#VERSION}-windows-{#ARCH}-setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile=ncmail.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Executable
Source: "..\..\_staging\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
; Runtime DLLs collected by collect-dlls.sh
Source: "..\..\_staging\*.dll"; DestDir: "{app}"; Flags: ignoreversion
; Color schemes (selected at run time with the "ColorFile" rc keyword)
Source: "..\..\_staging\colors\*"; DestDir: "{app}\colors"; \
  Flags: ignoreversion recursesubdirs createallsubdirs
; Icon
Source: "ncmail.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\{#AppExe}"; IconFilename: "{app}\ncmail.ico"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#AppExe}"; \
  Description: "Launch {#AppName}"; \
  Flags: nowait postinstall skipifsilent
