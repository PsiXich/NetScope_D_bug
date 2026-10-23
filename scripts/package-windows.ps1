param(
    [string]$QtRoot = 'D:/Qt/6.8.3/mingw_64',
    [string]$QtSources = 'D:/Qt/6.8.3/Src',
    [string]$ToolsRoot = 'D:/Qt/Tools'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$cmake = Join-Path $ToolsRoot 'CMake_64/bin/cmake.exe'
$mingw = Join-Path $ToolsRoot 'mingw1310_64'
$build = Join-Path $repo 'build/release-v1.1'
$output = Join-Path $repo ('build/packages/' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$package = Join-Path $output 'NetScope-v1.1-windows-x64'
$modules = @('qtbase', 'qtsvg', 'qtimageformats', 'qtwebsockets', 'qtmqtt')
foreach ($path in @($cmake, "$QtRoot/bin/windeployqt.exe", "$mingw/bin/g++.exe") + ($modules | ForEach-Object { "$QtSources/$_/LICENSES" })) {
    if (!(Test-Path -LiteralPath $path)) { throw "Missing prerequisite: $path" }
}
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
$previousPath = $env:PATH
try {
    $env:PATH = "$mingw/bin;$ToolsRoot/Ninja;$QtRoot/bin;" + $env:PATH
    Invoke-Checked $cmake @('-S', $repo, '-B', $build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_PREFIX_PATH=$QtRoot")
    Invoke-Checked $cmake @('--build', $build, '--parallel', '6')
    New-Item -ItemType Directory -Path $package | Out-Null
    Copy-Item -LiteralPath "$build/bin/netscope.exe" -Destination $package
    Invoke-Checked "$QtRoot/bin/windeployqt.exe" @('--release', '--compiler-runtime', '--no-translations', '--no-opengl-sw', '--exclude-plugins', 'qopensslbackend', "$package/netscope.exe")
    # Schannel uses Windows TLS facilities; no separate OpenSSL install is needed.
    @('[Paths]', 'Prefix=.', 'Plugins=.') | Set-Content "$package/qt.conf" -Encoding ascii
    foreach ($name in @('README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md')) {
        Copy-Item -LiteralPath "$repo/$name" -Destination $package
    }
    Copy-Item -LiteralPath "$repo/packaging/README.txt" -Destination "$package/START-HERE.txt"
    foreach ($module in $modules) {
        New-Item -ItemType Directory -Path "$package/licenses/$module" -Force | Out-Null
        Copy-Item "$QtSources/$module/LICENSES/*" -Destination "$package/licenses/$module"
    }
    Copy-Item -LiteralPath "$mingw/licenses" -Destination "$package/licenses/mingw" -Recurse
    foreach ($required in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll','Qt6WebSockets.dll','Qt6Mqtt.dll','platforms/qwindows.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        if (!(Test-Path -LiteralPath "$package/$required")) { throw "Deployment incomplete: $required" }
    }
    $sourceZip = Join-Path $output 'NetScope-v1.1-sources.zip'
    Invoke-Checked 'tar.exe' (@('-a','-c','-f',$sourceZip,'-C',$repo,'CMakeLists.txt','README.md','LICENSE','THIRD_PARTY_NOTICES.md','src','tests','scripts','packaging','-C',$QtSources) + $modules)
    Compress-Archive -LiteralPath $package -DestinationPath "$output/NetScope-v1.1-windows-x64.zip"
    Get-ChildItem -LiteralPath $output -Filter *.zip | ForEach-Object {
        $hash = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
        "$($hash.Hash.ToLowerInvariant())  $($_.Name)"
    } | Set-Content "$output/SHA256SUMS.txt" -Encoding ascii
    Write-Output "Release artifacts: $output"
} finally {
    $env:PATH = $previousPath
}
