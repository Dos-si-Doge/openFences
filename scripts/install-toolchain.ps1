# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# FIRST RUN ON A NEW MACHINE: "running scripts is disabled on this system"
# ------------------------------------------------------------------------
# Windows 11 blocks every PowerShell script until you allow them (execution policy "Restricted"). In a normal,
# non-administrator PowerShell window:
#
#   1. Allow scripts for your account, permanently (not only for this window):
#          Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned -Force
#      RemoteSigned runs scripts made on this PC and blocks unsigned ones downloaded from the internet.
#
#   2. If this file was downloaded or copied from another computer, Windows may have marked it as coming from the
#      internet, which RemoteSigned still blocks. Unblock this one file:
#          Unblock-File .\install-toolchain.ps1
#
#   3. Run it, from the folder that holds it:
#          .\install-toolchain.ps1 -CloneTo C:\dev\repo\openFences -Build
#
# Check the result with "Get-ExecutionPolicy -List". If MachinePolicy or UserPolicy is set, your organization manages the
# policy (Group Policy) and step 1 has no effect; ask your administrator. To run the script once without changing any
# setting, use:  powershell -ExecutionPolicy Bypass -File .\install-toolchain.ps1
# The script itself also sets RemoteSigned for your account when the policy is stricter, so the repository's scripts
# (scripts\build.ps1, the integration tests) run directly afterwards.

<#
.SYNOPSIS
Installs everything needed to develop, build and package openFences on a clean Windows 10/11 machine, using winget.

.DESCRIPTION
Works on a fresh Windows 11 install with nothing else added, and is safe to run again: tools that are already present are
skipped. It doesn't need the repository, so it can be copied onto a new machine on its own.

Machine-wide installs run in one elevated child process (a single UAC prompt):
  - Git for Windows (includes Git Credential Manager, for cloning the private repository)
  - Visual Studio 2022 Build Tools with the C++ workload: MSVC v143 x64, Windows 11 SDK (10.0.26100), CMake, Ninja
  - .NET 8 SDK (runs the WiX toolset)
  - Python 3.13 (generates the user guide and icon; standard library only)
  - Unless -Minimal: Visual Studio Code and GitHub CLI
Per-user steps then run unelevated, so the files belong to you:
  - WiX Toolset 5.0.2 (.NET global tool) with the UI and Util extensions
  - Unless -Minimal: the VS Code C++ and CMake extensions
  - Optionally: clone the repository (-CloneTo) and run the first build (-Build)

Microsoft Edge (used headless to render the Welcome window images) ships with Windows 11; it's installed if missing.

.PARAMETER Minimal
Install only what building needs (skip VS Code and GitHub CLI).

.PARAMETER CloneTo
Clone the repository into this folder after installing. Git Credential Manager asks you to sign in to GitHub.

.PARAMETER RepoUrl
Repository to clone. Default: https://github.com/Dos-si-Doge/openFences.git

.PARAMETER Build
After installing (and cloning), run scripts\build.ps1: user guide, release build, unit tests, MSI and portable zip.

.PARAMETER DryRun
Show what would be installed without changing anything.

.EXAMPLE
powershell -ExecutionPolicy Bypass -File install-toolchain.ps1 -CloneTo C:\dev\repo\openFences -Build
#>
param(
    [switch]$Minimal,
    [string]$CloneTo = '',
    [string]$RepoUrl = 'https://github.com/Dos-si-Doge/openFences.git',
    [switch]$Build,
    [switch]$DryRun,
    # Internal: set when the script relaunches itself elevated for the machine-wide installs.
    [switch]$MachinePhase,
    [string]$ResultFile = ''
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$WixVersion = '5.0.2'
$NuGetSource = 'https://api.nuget.org/v3/index.json'
$VsComponents = @(
    'Microsoft.VisualStudio.Workload.VCTools',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    'Microsoft.VisualStudio.Component.Windows11SDK.26100',
    'Microsoft.VisualStudio.Component.VC.CMake.Project'   # CMake and Ninja
)
$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$VsInstaller = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe"

# winget exit codes that mean "nothing to do".
$WingetOk = @(0, -1978335189, -1978335135, -1978334963)   # 0x8A15002B no applicable update, 0x8A150061 already installed, 0x8A15010D

function Write-Step($text) { Write-Host "==> $text" -ForegroundColor Cyan }
function Test-Admin { ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) }

# Re-reads PATH from the registry so tools installed by this script are usable straight away.
function Update-SessionPath {
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user = [Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = (@($machine, $user, "$env:ProgramFiles\dotnet", "$env:USERPROFILE\.dotnet\tools") | Where-Object { $_ }) -join ';'
}

# --------------------------------------------------------------------------------------------- machine-wide (elevated)

function Initialize-Winget {
    if (Get-Command winget -ErrorAction SilentlyContinue) { return }
    # On a brand-new account, App Installer (winget) can be present but not yet registered for the user.
    Write-Step 'Registering winget (App Installer)'
    try { Add-AppxPackage -RegisterByFamilyName -MainPackage Microsoft.DesktopAppInstaller_8wekyb3d8bbwe } catch { Write-Verbose "Registering App Installer failed: $_" }   # checked right below
    $env:Path += ";$env:LOCALAPPDATA\Microsoft\WindowsApps"
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw 'winget is not available. Update "App Installer" from the Microsoft Store (or see https://aka.ms/getwinget), then run this script again.'
    }
}

function Install-Package([string]$Id, [string]$Name, [string[]]$Extra = @()) {
    $null = winget list --exact --id $Id --source winget --accept-source-agreements 2>$null
    if ($LASTEXITCODE -eq 0) { Write-Host "    $Name is already installed."; return $true }
    if ($DryRun) { Write-Host "    would install $Name ($Id)"; return $true }
    Write-Host "    installing $Name ($Id)..."
    # Start-Process -NoNewWindow lets winget write straight to the console. Capturing or piping its output would turn each
    # spinner frame into a new line and garble the progress bar (PowerShell decodes it with the OEM code page).
    $argList = @('install', '--exact', '--id', $Id, '--source', 'winget', '--silent', '--accept-package-agreements',
        '--accept-source-agreements', '--disable-interactivity') + $Extra
    $argString = ($argList | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join ' '
    $wingetExe = (Get-Command winget).Source
    $proc = Start-Process -FilePath $wingetExe -ArgumentList $argString -NoNewWindow -PassThru
    $null = $proc.Handle   # keeps the handle open so ExitCode is available after the process ends
    $proc.WaitForExit()
    $code = $proc.ExitCode
    if ($WingetOk -contains $code) { return $true }
    if ($code -eq 3010 -or $code -eq -1978335215) { Write-Warning "$Name needs a restart to finish installing."; return $true }
    Write-Warning "$Name failed to install (winget exit code $code)."
    return $false
}

# Visual Studio Build Tools: installs them, or adds the C++ components to an existing Build Tools install that lacks them
# (winget alone would skip it as "already installed").
function Install-BuildTools {
    $hasCpp = $false
    if (Test-Path $VsWhere) {
        $path = & $VsWhere -products * -version '[17.0,18.0)' -requires $VsComponents -latest -property installationPath
        $hasCpp = [bool]$path
    }
    if ($hasCpp) { Write-Host '    Visual Studio 2022 Build Tools with the C++ workload are already installed.'; return $true }
    $adds = ($VsComponents | ForEach-Object { "--add $_" }) -join ' '
    $existing = if (Test-Path $VsWhere) { & $VsWhere -products Microsoft.VisualStudio.Product.BuildTools -version '[17.0,18.0)' -latest -property installationPath } else { $null }
    if ($existing) {
        if ($DryRun) { Write-Host "    would add the C++ components to $existing"; return $true }
        Write-Host "    adding the C++ components to $existing (this takes a while)..."
        $p = Start-Process -FilePath $VsInstaller -ArgumentList "modify --installPath `"$existing`" $adds --includeRecommended --quiet --norestart --wait" -Wait -PassThru
        if ($p.ExitCode -in 0, 3010) { return $true }
        Write-Warning "Visual Studio Installer failed (exit code $($p.ExitCode))."
        return $false
    }
    Write-Host '    (about 6-8 GB; this takes a while)'
    return Install-Package 'Microsoft.VisualStudio.2022.BuildTools' 'Visual Studio 2022 Build Tools' @('--override', "--wait --quiet --norestart --nocache $adds --includeRecommended")
}

function Invoke-MachinePhase {
    Initialize-Winget
    if (-not $DryRun) { winget source update --disable-interactivity | Out-Null }
    $ok = @{}
    Write-Step 'Git for Windows'
    $ok['Git'] = Install-Package 'Git.Git' 'Git'
    Write-Step 'Visual Studio 2022 Build Tools (MSVC v143, Windows 11 SDK, CMake, Ninja)'
    $ok['Build Tools'] = Install-BuildTools
    Write-Step '.NET 8 SDK (runs WiX)'
    $ok['.NET 8 SDK'] = Install-Package 'Microsoft.DotNet.SDK.8' '.NET 8 SDK'
    Write-Step 'Python 3.13 (user guide and icon scripts)'
    # Any real Python 3 on PATH will do; the WindowsApps "python" is only the Microsoft Store installer stub.
    $py = Get-Command python -ErrorAction SilentlyContinue
    if ($py -and $py.Source -notmatch 'WindowsApps' -and ((& $py.Source --version 2>&1) -match 'Python 3\.')) {
        Write-Host "    $(& $py.Source --version 2>&1) is already installed."
        $ok['Python'] = $true
    } else {
        # Machine scope puts python.exe on the system PATH, ahead of the Microsoft Store "python" alias.
        $ok['Python'] = Install-Package 'Python.Python.3.13' 'Python 3.13' @('--scope', 'machine', '--override', '/quiet InstallAllUsers=1 PrependPath=1 Include_test=0')
    }
    $edge = @("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe", "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe") | Where-Object { Test-Path $_ }
    if (-not $edge) {
        Write-Step 'Microsoft Edge (renders the Welcome window images)'
        $ok['Edge'] = Install-Package 'Microsoft.Edge' 'Microsoft Edge'
    }
    if (-not $Minimal) {
        Write-Step 'Visual Studio Code'
        $ok['VS Code'] = Install-Package 'Microsoft.VisualStudioCode' 'Visual Studio Code' @('--scope', 'machine')
        Write-Step 'GitHub CLI'
        $ok['GitHub CLI'] = Install-Package 'GitHub.cli' 'GitHub CLI'
    }
    return $ok
}

if ($MachinePhase) {
    # Elevated child: do the installs, hand the results back to the parent, and leave.
    Start-Transcript -Path (Join-Path $env:TEMP 'openFences-toolchain-machine.log') -Force | Out-Null
    try {
        $ok = Invoke-MachinePhase
        # Written to a temporary name and then renamed, so the waiting parent never reads a half-written file.
        if ($ResultFile) { $ok | ConvertTo-Json | Set-Content -Path "$ResultFile.tmp" -Encoding UTF8; Move-Item "$ResultFile.tmp" $ResultFile -Force }
    } catch {
        Write-Host $_ -ForegroundColor Red
        if ($ResultFile) { @{ error = "$_" } | ConvertTo-Json | Set-Content -Path "$ResultFile.tmp" -Encoding UTF8; Move-Item "$ResultFile.tmp" $ResultFile -Force }
    } finally {
        Stop-Transcript | Out-Null
    }
    exit 0
}

# --------------------------------------------------------------------------------------------- main (unelevated)

Start-Transcript -Path (Join-Path $env:TEMP 'openFences-toolchain.log') -Force | Out-Null
try {
    $os = Get-CimInstance Win32_OperatingSystem
    Write-Host "openFences toolchain setup on $($os.Caption) $($os.Version) ($env:PROCESSOR_ARCHITECTURE)"
    if ($env:PROCESSOR_ARCHITECTURE -ne 'AMD64') { Write-Warning 'openFences is built for x64; this machine is not x64.' }
    if ($DryRun) { Write-Host '(dry run: nothing will be changed)' -ForegroundColor Yellow }

    # 0. Let PowerShell scripts run for this user from now on (Windows 11 starts out "Restricted"). The Process scope is
    #    ignored here: "-ExecutionPolicy Bypass" on the command line only lasts for this run.
    Write-Step 'PowerShell execution policy (current user)'
    $persistent = 'Restricted'
    $source = 'Windows default'
    foreach ($scope in 'MachinePolicy', 'UserPolicy', 'CurrentUser', 'LocalMachine') {
        $value = Get-ExecutionPolicy -Scope $scope
        if ($value -ne 'Undefined') { $persistent = "$value"; $source = $scope; break }
    }
    if ($persistent -in 'Restricted', 'AllSigned') {
        if ($DryRun) { Write-Host "    would set RemoteSigned for the current user (now $persistent)" }
        elseif ($source -in 'MachinePolicy', 'UserPolicy') { Write-Warning "Scripts are restricted by Group Policy ($source = $persistent); run them with -ExecutionPolicy Bypass or ask your administrator." }
        else {
            try {
                Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned -Force
                Write-Host "    set to RemoteSigned for $env:USERNAME (was $persistent): scripts made on this PC now run without -ExecutionPolicy Bypass."
            } catch { Write-Warning "Couldn't change the execution policy: $_" }
        }
    } else {
        Write-Host "    $persistent ($source); scripts can run."
    }

    # 1. Machine-wide installs, elevated once.
    if ((Test-Admin) -or $DryRun) {   # a dry run only reads, so it needs no elevation
        $machine = Invoke-MachinePhase
    } else {
        Write-Step 'Machine-wide installs (approve the UAC prompt)'
        $resultFile = Join-Path $env:TEMP "openFences-toolchain-$PID.json"
        $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", '-MachinePhase', '-ResultFile', "`"$resultFile`"")
        if ($Minimal) { $argList += '-Minimal' }
        if ($DryRun) { $argList += '-DryRun' }
        try {
            $p = Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList $argList -Verb RunAs -PassThru
        } catch {
            throw "The machine-wide installs need administrator rights; the UAC prompt was declined or failed ($($_.Exception.Message))."
        }
        # Don't rely on Start-Process -Wait: in Windows PowerShell it can return while an elevated process is still running.
        # Wait until the elevated part writes its result file, or its process is gone.
        Write-Host '    waiting for the elevated window to finish (Visual Studio Build Tools can take 10-30 minutes)...'
        while (-not (Test-Path $resultFile) -and (Get-Process -Id $p.Id -ErrorAction SilentlyContinue)) { Start-Sleep -Seconds 3 }
        if (-not (Test-Path $resultFile)) { throw "The elevated installer stopped before finishing. See $env:TEMP\openFences-toolchain-machine.log." }
        $machine = Get-Content -Raw $resultFile | ConvertFrom-Json
        Remove-Item $resultFile -ErrorAction SilentlyContinue
        if ($machine.error) { throw "Machine-wide installs failed: $($machine.error)" }
        Get-Content (Join-Path $env:TEMP 'openFences-toolchain-machine.log') -ErrorAction SilentlyContinue |
            Where-Object { $_ -match '^(==>|    |WARNING)' } | ForEach-Object { Write-Host $_ }
    }
    Update-SessionPath

    # 2. WiX Toolset (per-user .NET global tool) and its extensions.
    Write-Step "WiX Toolset $WixVersion"
    if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) {
        if ($DryRun) { Write-Host "    would install the wix $WixVersion global tool and the UI and Util extensions" }
        else { Write-Warning '.NET SDK not found; skipping WiX. Open a new terminal and run this script again.' }
    } else {
        $tools = dotnet tool list --global | Out-String
        if ($tools -match "(?m)^wix\s+$([regex]::Escape($WixVersion))\b") { Write-Host "    wix $WixVersion is already installed." }
        elseif ($DryRun) { Write-Host "    would install the wix $WixVersion global tool" }
        else {
            if ($tools -match '(?m)^wix\s') { dotnet tool uninstall --global wix | Out-Null }
            dotnet tool install --global wix --version $WixVersion --add-source $NuGetSource
            if ($LASTEXITCODE -ne 0) { throw 'Installing WiX failed.' }
        }
        Update-SessionPath
        # Extensions restore from NuGet; a private nuget.config supplies the source in case the user has none configured.
        $work = Join-Path $env:TEMP "openFences-wix-$PID"
        New-Item -ItemType Directory -Force -Path $work | Out-Null
        "<?xml version=`"1.0`" encoding=`"utf-8`"?><configuration><packageSources><add key=`"nuget.org`" value=`"$NuGetSource`" /></packageSources></configuration>" |
            Set-Content -Path (Join-Path $work 'nuget.config') -Encoding UTF8
        Push-Location $work
        try {
            $installed = if (Get-Command wix -ErrorAction SilentlyContinue) { wix extension list -g | Out-String } else { '' }
            foreach ($ext in 'WixToolset.UI.wixext', 'WixToolset.Util.wixext') {
                if ($installed -match "$([regex]::Escape($ext))\s+$([regex]::Escape($WixVersion))") { Write-Host "    $ext is already installed."; continue }
                if ($DryRun) { Write-Host "    would add $ext $WixVersion"; continue }
                wix extension add -g "$ext/$WixVersion"
                if ($LASTEXITCODE -ne 0) { throw "Adding $ext failed." }
            }
        } finally {
            Pop-Location
            Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
        }
    }

    # 3. VS Code extensions.
    if (-not $Minimal) {
        Write-Step 'VS Code extensions (C/C++, CMake Tools)'
        $code = Get-Command code -ErrorAction SilentlyContinue
        if ($DryRun) { Write-Host '    would install ms-vscode.cpptools and ms-vscode.cmake-tools' }
        elseif ($code) { foreach ($ext in 'ms-vscode.cpptools', 'ms-vscode.cmake-tools') { & $code.Source --install-extension $ext --force | Out-Null; Write-Host "    $ext" } }
        else { Write-Warning 'VS Code not found on PATH; open a new terminal and run "code --install-extension ms-vscode.cpptools".' }
    }

    # 4. Repository.
    $repo = $null
    if ($CloneTo) {
        Write-Step "Repository -> $CloneTo"
        if (Test-Path (Join-Path $CloneTo '.git')) { Write-Host '    already cloned.'; $repo = $CloneTo }
        elseif ($DryRun) { Write-Host "    would clone $RepoUrl" }
        else {
            New-Item -ItemType Directory -Force -Path (Split-Path $CloneTo -Parent) | Out-Null
            git clone $RepoUrl $CloneTo   # Git Credential Manager opens a GitHub sign-in for the private repository.
            if ($LASTEXITCODE -ne 0) { throw 'git clone failed.' }
            $repo = $CloneTo
            if (Test-Admin) { & icacls $CloneTo /setowner $env:USERNAME /T /C /Q | Out-Null }   # avoid git's "dubious ownership" later
        }
    } elseif (Test-Path (Join-Path (Split-Path $PSScriptRoot -Parent) 'CMakeLists.txt')) {
        $repo = Split-Path $PSScriptRoot -Parent
    }

    # 5. Check what's there.
    Write-Step 'Summary'
    $vsPath = if (Test-Path $VsWhere) { & $VsWhere -products * -version '[17.0,18.0)' -requires $VsComponents -latest -property installationPath } else { $null }
    $vsVer = if ($vsPath) { & $VsWhere -products * -version '[17.0,18.0)' -requires $VsComponents -latest -property catalog_productDisplayVersion } else { $null }
    function Get-ToolVersion($cmd, $args2) { $c = Get-Command $cmd -ErrorAction SilentlyContinue; if ($c) { ((& $c.Source @args2 2>$null) | Select-Object -First 1) -replace '\s+$', '' } else { $null } }
    $python = Get-Command python -ErrorAction SilentlyContinue
    $rows = @(
        [pscustomobject]@{ Tool = 'Git'; Status = Get-ToolVersion git @('--version') },
        [pscustomobject]@{ Tool = 'VS 2022 Build Tools + C++'; Status = $(if ($vsPath) { "$vsVer ($vsPath)" }) },
        [pscustomobject]@{ Tool = '.NET SDK'; Status = Get-ToolVersion dotnet @('--version') },
        [pscustomobject]@{ Tool = 'WiX'; Status = Get-ToolVersion wix @('--version') },
        [pscustomobject]@{ Tool = 'Python'; Status = $(if ($python -and $python.Source -notmatch 'WindowsApps') { Get-ToolVersion python @('--version') }) },
        [pscustomobject]@{ Tool = 'Microsoft Edge'; Status = $(if (@("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe", "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe") | Where-Object { Test-Path $_ }) { 'installed' }) }
    )
    if (-not $Minimal) {
        $rows += [pscustomobject]@{ Tool = 'VS Code'; Status = Get-ToolVersion code @('--version') }
        $rows += [pscustomobject]@{ Tool = 'GitHub CLI'; Status = Get-ToolVersion gh @('--version') }
    }
    foreach ($r in $rows) { if (-not $r.Status) { $r.Status = $(if ($DryRun) { '(not installed yet)' } else { 'MISSING' }) } }
    $rows | Format-Table -AutoSize | Out-String -Width 200 | Write-Host
    $missing = @($rows | Where-Object Status -eq 'MISSING')
    if ($missing.Count) { Write-Warning "Missing: $(($missing.Tool) -join ', '). Open a new terminal and run this script again; see $env:TEMP\openFences-toolchain*.log." }

    # 6. First build.
    if ($Build -and -not $DryRun) {
        if (-not $repo) { throw '-Build needs the repository: pass -CloneTo, or run this script from scripts\ inside a clone.' }
        if ($missing.Count) { throw 'Not building: some tools are missing.' }
        Write-Step 'First build (scripts\build.ps1)'
        & (Get-Process -Id $PID).Path -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repo 'scripts\build.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'The build failed.' }
    }

    Write-Host ''
    Write-Host 'Toolchain ready. Open a new terminal so PATH changes apply everywhere, then build with:' -ForegroundColor Green
    Write-Host '    powershell -ExecutionPolicy Bypass -File scripts\build.ps1'
} finally {
    Stop-Transcript | Out-Null
}
