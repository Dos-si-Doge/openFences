# Builds openFences (x64), runs the unit tests and packages the release: MSI installer and portable zip.
#   powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Preset x64-release|x64-debug] [-SkipTests] [-SkipPackages]
# Output: out\openFences-<version>-x64.msi and out\openFences-<version>-x64-portable.zip
param(
    [ValidateSet('x64-release', 'x64-debug')][string]$Preset = 'x64-release',
    [switch]$SkipTests,
    [Alias('SkipMsi')][switch]$SkipPackages
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

# The user guide ships inside both packages, so regenerate it from its source first.
$python = Get-Command python -ErrorAction SilentlyContinue
if ($python) {
    Write-Host '==> User guide'
    & $python.Source -I (Join-Path $root 'scripts\build_user_guide.py')
    if ($LASTEXITCODE -ne 0) { throw 'Building the user guide failed.' }
} else {
    Write-Warning 'Python not found; packaging the existing docs\user-guide\openFences-User-Guide.html.'
}

Write-Host "==> Configure ($Preset)"
Invoke-InVsShell "cmake --preset $Preset"
Write-Host '==> Build'
Invoke-InVsShell "cmake --build --preset $Preset"
if (-not $SkipTests) {
    Write-Host '==> Unit tests'
    Invoke-InVsShell "ctest --preset $Preset"
}
if ($SkipPackages) { return }

$version = (Select-String -Path CMakeLists.txt -Pattern 'project\(openFences VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$binDir = Join-Path $root "build\$Preset"
$outDir = Join-Path $root 'out'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Write-Host '==> MSI'
$env:PATH = "$env:ProgramFiles\dotnet;$env:USERPROFILE\.dotnet\tools;$env:PATH"
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) { throw 'WiX is not installed. Run scripts\install-toolchain.ps1 first.' }
$msi = Join-Path $outDir "openFences-$version-x64.msi"
Push-Location (Join-Path $root 'installer')
try {
    wix build Package.wxs -arch x64 -ext WixToolset.UI.wixext -ext WixToolset.Util.wixext -d "Version=$version" -d "BinDir=$binDir" -o $msi
    if ($LASTEXITCODE -ne 0) { throw "wix build failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
Write-Host "Built $msi"

Write-Host '==> Portable zip'
# Same files as the MSI, plus the marker that keeps settings in a Data folder next to the exe.
$stage = Join-Path $outDir 'portable\openFences'
Remove-Item -Recurse -Force (Join-Path $outDir 'portable') -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'templates') | Out-Null
Copy-Item (Join-Path $binDir 'openFences.exe') $stage
Copy-Item (Join-Path $binDir 'openFences-User-Guide.html') $stage
Copy-Item (Join-Path $root 'res\templates\*.json') (Join-Path $stage 'templates')
@(
    'openFences portable mode',
    '',
    'While this file is next to openFences.exe, openFences keeps its settings, backups and logs in the',
    '"Data" folder beside it instead of in your user profile. Delete this file to use %APPDATA%\openFences.',
    '',
    'Start openFences by running openFences.exe. "Start openFences when I sign in" (Settings > Welcome)',
    'points Windows at this copy, so keep the folder where it is or turn the option off before moving it.'
) | Set-Content -Path (Join-Path $stage 'openFences.portable') -Encoding UTF8
$zip = Join-Path $outDir "openFences-$version-x64-portable.zip"
Remove-Item -Force $zip -ErrorAction SilentlyContinue
# Name entries explicitly with "/" separators: Windows PowerShell's zip helpers write "\", which other unzip tools mishandle.
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    $base = Split-Path $stage -Parent
    Get-ChildItem -Path $stage -Recurse -File | ForEach-Object {
        $entry = $_.FullName.Substring($base.Length + 1).Replace('\', '/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $_.FullName, $entry, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally {
    $archive.Dispose()
}
Remove-Item -Recurse -Force (Join-Path $outDir 'portable')
Write-Host "Built $zip"
