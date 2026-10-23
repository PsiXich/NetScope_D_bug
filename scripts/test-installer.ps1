param(
    [string]$OldInstaller,
    [string]$NewInstaller,
    [string]$PackageDir
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (!$OldInstaller) { $OldInstaller = "$repo/build/installer-validation/NetScope_v1.0_Setup.exe" }
if (!$NewInstaller) { $NewInstaller = "$repo/build/installer-v1.1/NetScope_v1.1_Setup.exe" }
if (!$PackageDir) { $PackageDir = "$repo/build/packages/20261022-235100/NetScope-v1.1-windows-x64" }
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an Administrator PowerShell window.'
}
$id = '{5BAAA525-5F70-47B7-9BD6-A9C95217C5C4}_is1'
$keys = @("HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$id", "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\$id", "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$id")
foreach ($key in $keys) {
    if (Test-Path -LiteralPath $key) { throw "Existing NetScope installation found; refusing to modify it: $key" }
}
$shortcuts = @(
    (Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'NetScope.lnk'),
    (Join-Path ([Environment]::GetFolderPath('Programs')) 'NetScope.lnk')
)
foreach ($shortcut in $shortcuts) {
    if (Test-Path -LiteralPath $shortcut) { throw "Existing shortcut found: $shortcut" }
}
foreach ($file in @($OldInstaller, $NewInstaller, "$PackageDir/netscope.exe")) {
    if (!(Test-Path -LiteralPath $file)) { throw "Missing test input: $file" }
}
$run = Join-Path $repo ('build/installer-validation/run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$install = Join-Path $run 'installed'
New-Item -ItemType Directory -Path $run | Out-Null
$steps = New-Object 'System.Collections.Generic.List[string]'
$result = [ordered]@{ status = 'RUNNING'; run = $run; steps = $steps; error = $null }
$newHash = (Get-FileHash -LiteralPath $NewInstaller -Algorithm SHA256).Hash
$oldHash = (Get-FileHash -LiteralPath $OldInstaller -Algorithm SHA256).Hash
$result.newInstallerSHA256 = $newHash
$result.oldInstallerSHA256 = $oldHash
function Assert-True($Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function Run-Setup([string]$File, [string]$Log, [switch]$Uninstall) {
    $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', ('/LOG="' + $Log + '"'))
    if (!$Uninstall) { $arguments += @('/LANG=english', '/TASKS=""', ('/DIR="' + $install + '"')) }
    $process = Start-Process -FilePath $File -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(180000)) { throw "Installer timed out; inspect process $($process.Id) and $Log" }
    Assert-True ($process.ExitCode -eq 0) "Installer exit code $($process.ExitCode): $Log"
}
function Assert-Registration([string]$Version) {
    $found = @($keys | Where-Object { Test-Path -LiteralPath $_ })
    Assert-True ($found.Count -eq 1) 'Expected exactly one uninstall registration.'
    $entry = Get-ItemProperty -LiteralPath $found[0]
    Assert-True ($entry.DisplayVersion -eq $Version) "Unexpected registered version: $($entry.DisplayVersion)"
    Assert-True ($entry.InstallLocation.TrimEnd('\') -eq $install) 'Installer used an unexpected directory.'
    Assert-True (@($shortcuts | Where-Object { Test-Path -LiteralPath $_ }).Count -eq 1) 'Missing or duplicate Start Menu shortcut.'
}
function Assert-NewFiles {
    Assert-True ((Get-Item -LiteralPath "$install/netscope.exe").VersionInfo.ProductVersion -eq '1.1.0') 'Incorrect application version.'
    $sourceRoot = (Resolve-Path -LiteralPath $PackageDir).Path.TrimEnd('\')
    foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File) {
        $relative = $file.FullName.Substring($sourceRoot.Length + 1)
        $destination = Join-Path $install $relative
        Assert-True (Test-Path -LiteralPath $destination) "Missing installed file: $relative"
        Assert-True ((Get-FileHash -LiteralPath $file.FullName).Hash -eq (Get-FileHash -LiteralPath $destination).Hash) "Installed file differs: $relative"
    }
}
function Smoke-Test([string]$Name) {
    $savedPath = $env:PATH
    $savedPluginPath = $env:QT_PLUGIN_PATH
    $savedPlatformPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
    $process = $null
    try {
        $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
        $env:QT_PLUGIN_PATH = $null
        $env:QT_QPA_PLATFORM_PLUGIN_PATH = $null
        $process = Start-Process -FilePath "$install/netscope.exe" -WorkingDirectory $install -WindowStyle Hidden -PassThru
        Start-Sleep -Seconds 3
        $process.Refresh()
        Assert-True (!$process.HasExited) 'Installed application exited during startup.'
        $modules = @($process.Modules | ForEach-Object { $_.FileName })
        $modules | Set-Content -LiteralPath "$run/$Name-modules.txt"
        Assert-True (@($modules | Where-Object { $_ -like 'D:\Qt\*' }).Count -eq 0) 'Application loaded DLLs from the development SDK.'
    } finally {
        # This hidden startup probe is deliberately stopped without saving UI settings.
        if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
        $env:PATH = $savedPath
        $env:QT_PLUGIN_PATH = $savedPluginPath
        $env:QT_QPA_PLATFORM_PLUGIN_PATH = $savedPlatformPath
    }
}
function Remove-TestInstallation([string]$Name) {
    Run-Setup "$install/unins000.exe" "$run/$Name.log" -Uninstall
    Assert-True (@($keys | Where-Object { Test-Path -LiteralPath $_ }).Count -eq 0) 'Uninstall registration remains.'
    Assert-True (@($shortcuts | Where-Object { Test-Path -LiteralPath $_ }).Count -eq 0) 'Start Menu shortcut remains.'
    Assert-True (!(Test-Path -LiteralPath "$install/netscope.exe")) 'Application remains after uninstall.'
    # The uninstaller helper removes its own temporary files after the launcher exits.
    $deadline = (Get-Date).AddSeconds(15)
    while ((Test-Path -LiteralPath $install) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    Assert-True (@(Get-ChildItem -LiteralPath $install -Recurse -File -ErrorAction SilentlyContinue).Count -eq 0) 'Files remain after uninstall.'
}
try {
    Run-Setup $NewInstaller "$run/fresh-install.log"
    Assert-Registration '1.1'; Assert-NewFiles; Smoke-Test 'fresh'
    $steps.Add('Fresh v1.1 install, file hashes, registration, shortcut and startup: PASS')
    Remove-TestInstallation 'fresh-uninstall'
    $steps.Add('Fresh v1.1 uninstall: PASS')
    Run-Setup $OldInstaller "$run/v1.0-install.log"
    Assert-Registration '1.0'; Smoke-Test 'v1.0'
    $steps.Add('Original GitHub v1.0 install and startup: PASS')
    Run-Setup $NewInstaller "$run/upgrade.log"
    Assert-Registration '1.1'; Assert-NewFiles; Smoke-Test 'upgrade'
    Assert-True (!(Test-Path 'HKLM:\SOFTWARE\Classes\NetScopeFile.myp')) 'Obsolete v1.0 file association remains.'
    $steps.Add('Upgrade v1.0 to v1.1, matching files, one registration and startup: PASS')
    Remove-TestInstallation 'upgrade-uninstall'
    $steps.Add('Upgraded installation uninstall: PASS')
    $result.status = 'PASS'
} catch {
    $result.status = 'FAIL'
    $result.error = $_.Exception.Message
    # Keep logs and the failed installation for diagnosis; never delete paths blindly.
} finally {
    $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$run/result.json" -Encoding UTF8
    Write-Output "Report: $run/result.json"
    $result | ConvertTo-Json -Depth 6
}
if ($result.status -ne 'PASS') { exit 1 }

