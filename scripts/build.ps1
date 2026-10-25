# Builds openFences (x64), runs the unit tests and packages the MSI.
#   powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Preset x64-release|x64-debug] [-SkipTests] [-SkipMsi]
# Output: out\openFences-<version>-x64.msi
param(
    [ValidateSet('x64-release', 'x64-debug')][string]$Preset = 'x64-release',
    [switch]$SkipTests,
    [switch]$SkipMsi
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

# Locate the Visual Studio C++ toolchain.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'Visual Studio Build Tools not found. Run scripts\install-toolchain.ps1 first.' }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'The MSVC x64 toolset is not installed. Run scripts\install-toolchain.ps1 first.' }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
# vcvars64.bat looks for vswhere on PATH.
$env:PATH = "$(Split-Path $vswhere);$env:PATH"

function Invoke-InVsShell([string]$command) {
    cmd /c "`"$vcvars`" >nul && $command"
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $command" }
}

Write-Host "==> Configure ($Preset)"
Invoke-InVsShell "cmake --preset $Preset"
Write-Host '==> Build'
Invoke-InVsShell "cmake --build --preset $Preset"
if (-not $SkipTests) {
    Write-Host '==> Unit tests'
    Invoke-InVsShell "ctest --preset $Preset"
}
if ($SkipMsi) { return }

Write-Host '==> MSI'
$env:PATH = "$env:ProgramFiles\dotnet;$env:USERPROFILE\.dotnet\tools;$env:PATH"
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) { throw 'WiX is not installed. Run scripts\install-toolchain.ps1 first.' }
$version = (Select-String -Path CMakeLists.txt -Pattern 'project\(openFences VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$binDir = Join-Path $root "build\$Preset"
New-Item -ItemType Directory -Force -Path (Join-Path $root 'out') | Out-Null
$msi = Join-Path $root "out\openFences-$version-x64.msi"
Push-Location (Join-Path $root 'installer')
try {
    wix build Package.wxs -arch x64 -ext WixToolset.UI.wixext -ext WixToolset.Util.wixext -d "Version=$version" -d "BinDir=$binDir" -o $msi
    if ($LASTEXITCODE -ne 0) { throw "wix build failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
Write-Host "Built $msi"
