# Installs the toolchain needed to build openFences and its MSI installer.
# Requires winget. The Visual Studio Build Tools step prompts for elevation (UAC).
$ErrorActionPreference = 'Stop'

function Test-Command($name) { [bool](Get-Command $name -ErrorAction SilentlyContinue) }

Write-Host '==> Visual Studio 2022 Build Tools (MSVC v143, Windows 11 SDK, CMake, Ninja)'
winget install -e --id Microsoft.VisualStudio.2022.BuildTools --accept-package-agreements --accept-source-agreements `
    --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

Write-Host '==> .NET 8 SDK (used only to run WiX)'
winget install -e --id Microsoft.DotNet.SDK.8 --accept-package-agreements --accept-source-agreements

# Make dotnet visible in this session if it was just installed.
$env:PATH = "$env:ProgramFiles\dotnet;$env:USERPROFILE\.dotnet\tools;$env:PATH"

Write-Host '==> WiX Toolset v5'
$nugetSource = "https://api.nuget.org/v3/index.json"
if (-not (Test-Command wix)) {
    dotnet tool install --global wix --version 5.0.2 --add-source $nugetSource
}
# Run from the repo root so the repo-local nuget.config supplies the package source.
Push-Location (Split-Path $PSScriptRoot -Parent)
wix extension add -g WixToolset.UI.wixext/5.0.2
wix extension add -g WixToolset.Util.wixext/5.0.2
Pop-Location

Write-Host 'Toolchain ready.'
