; SPDX-License-Identifier: GPL-3.0-or-later
;
; installer/myabc.iss --- myabc (智能ABC) Windows 安装包，Inno Setup 脚本
;
; 依据：docs/plan/roadmap.md（v0.1 首个可分发版本）
;
; 用法（在仓库根目录先跑过 scripts/build.ps1 + scripts/build-engine.sh 之后）：
;   "C:\Users\<you>\AppData\Local\Programs\Inno Setup 6\ISCC.exe" installer\myabc.iss
; 产物：installer\dist\myabc-setup-<version>.exe
;
; 源文件目录约定：直接打包 out\Release\install-x64\ 的内容（跟这套仓库里"真实
; 安装测试"用的那份布局一致，不是 scripts\stage.ps1 的分 Config/arch 子目录布局——
; 见 docs/decisions/_debt-log.md 关于两套布局差异的记录）。

#define MyAppName "myabc"
#define MyAppNameFull "智能ABC (myabc)"
#define MyAppVersion "0.1.3"
#define MyAppPublisher "myabc project"
#define MyAppURL "https://github.com/realgz/myabc"
#define SrcDir "..\out\Release\install-x64"

[Setup]
AppId={{B7B6C9C0-6B0C-4F5B-9C7B-3B7B9C0F2E11}
AppName={#MyAppNameFull}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppNameFull}
DisableProgramGroupPage=yes
; 只做 x64（M6 前尚未落地 x86 双 InprocServer32，见 docs/decisions/_debt-log.md）。
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; 注册 TSF profile 要写 HKCR，必须管理员权限。
PrivilegesRequired=admin
; 覆盖安装时，用 Windows Restart Manager 检测并请求关闭正占用 myabc-tip.dll 的进程
; （实测会被 TextInputHost.exe/ctfmon.exe 占住，见 docs/decisions/_debt-log.md
; 2026-09-11 真机替换那次的排查记录）——不用每次都手动 taskkill。
CloseApplications=yes
RestartApplications=yes
CloseApplicationsFilter=*.exe,*.dll
LicenseFile=..\LICENSE
OutputDir=dist
OutputBaseFilename=myabc-setup-{#MyAppVersion}
SetupIconFile=..\assets\icons\myabc.ico
UninstallDisplayIcon={app}\myabc-tip.dll
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
; 简体中文语言包在这台机器上的 Inno Setup 安装里不是默认自带的（需要额外下载
; ChineseSimplified.isl），先只用英文向导 chrome——应用本身的 UI/候选窗全是中文，
; 不影响实际使用；后续要给安装向导也做中文可以再补语言包。
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; 整个 install-x64 目录内容原样打包，除了调试符号（.pdb 只在开发机排障用，
; 不该塞进给用户的安装包）。
Source: "{#SrcDir}\*"; Excludes: "*.pdb"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\卸载 {#MyAppNameFull}"; Filename: "{uninstallexe}"

[Run]
; 安装完成后自动注册 TSF 语言配置（--register 幂等，见 deploy_flow.cpp）。
Filename: "{app}\myabc-deployer.exe"; Parameters: "--register"; \
    StatusMsg: "正在注册输入法..."; Flags: runhidden waituntilterminated

[UninstallRun]
; 卸载前先反注册；先尽量结束可能还在跑的引擎/候选窗进程，避免文件占用导致残留
; （同一类教训见 docs/decisions/_debt-log.md「僵尸引擎进程」条目）。
Filename: "{cmd}"; Parameters: "/c taskkill /F /IM myabc-engine.exe & taskkill /F /IM myabc-ui.exe & exit 0"; \
    Flags: runhidden waituntilterminated; RunOnceId: "KillMyabcProcesses"
Filename: "{app}\myabc-deployer.exe"; Parameters: "--unregister"; \
    StatusMsg: "正在反注册输入法..."; Flags: runhidden waituntilterminated; RunOnceId: "UnregisterMyabc"

[UninstallDelete]
; 用户词库/学习数据（%APPDATA%\myabc）默认保留，不随卸载删除——避免用户重装后
; 丢失自学习结果；如需彻底清空，提示走 myabc-deployer --clear-userdict。
Type: filesandordirs; Name: "{app}"
