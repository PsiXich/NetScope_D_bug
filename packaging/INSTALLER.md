# Windows installer (Inno Setup 7)

`netscope.iss` packages the complete, previously tested portable directory.
It preserves the v1.0 AppId and Program Files installation scope for upgrades.
The installer includes English/Russian UI, GPLv3, optional desktop shortcut,
all deployment DLLs/plugins/licenses and normal uninstall support.
It no longer registers the unused `.myp` file association; an upgrade removes
only the association entries belonging to NetScope v1.0.

Compile from PowerShell (adjust PackageDir to the desired package):

```powershell
& 'C:\Program Files\Inno Setup 7\ISCC.exe' `
  '/DPackageDir=D:\Progects\NetScope\NetScopeDeBug\build\packages\20261022-235100\NetScope-v1.1-windows-x64' `
  '/DInstallerOutputDir=D:\Installer' `
  ./packaging/netscope.iss
```

The defaults also permit opening the script and selecting Compile on the current
development machine. Output: `NetScope_v1.1_Setup.exe`.

Distribute the matching `NetScope-v1.1-sources.zip` alongside the installer.
Installer creation does not sign the application or publish a GitHub release.
Before publication, test installation, upgrade from v1.0 and uninstall in a
clean Windows VM. Compilation alone does not verify these workflows.

## Release validation (2026-10-23)

The v1.1 installer passed a real installation cycle on the development Windows 11
machine, in a temporary directory with normal machine-wide registration:

- Fresh v1.1 install: all deployed file hashes, version, uninstall registration,
  Start Menu shortcut and application startup checked.
- Fresh v1.1 uninstall: installed files, registration and shortcut removed.
- Original published v1.0 installer installed and launched, then upgraded to v1.1.
- Upgrade: v1.1 file hashes, one uninstall entry, startup and removal of the obsolete
  NetScope file association checked; subsequent uninstall passed.
- Startup used a Windows-only PATH; no modules were loaded from the Qt SDK.

The eight Release CTest targets also passed. This is not a clean Windows VM test,
and it does not cover every Windows version, policy or interactive installer choice.
User profile settings/logs are intentionally outside installer cleanup.

To repeat on a disposable Windows machine, run `scripts/test-installer.ps1` from
an Administrator PowerShell, supplying `-OldInstaller`, `-NewInstaller` and
`-PackageDir`. The script refuses to replace an existing NetScope installation.
Reports and setup logs are saved under `build/installer-validation`.
