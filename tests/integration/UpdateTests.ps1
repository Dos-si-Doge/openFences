# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# End-to-end test of automatic updates on a real machine. It CHANGES THE INSTALLED openFences: it installs a throwaway
# older baseline MSI, then lets the update service upgrade it to the MSI under test through a local feed.
#   1. Elevated (UAC prompt): install the baseline; set the UpdateFeedUrl policy to http://127.0.0.1:<port>/latest.json.
#   2. Checks the installer's results: service permissions, install folder ACL, the Users-writable update key.
#   3. Serves feeds with Python's http.server and runs checks through "openFences.exe --check-updates":
#      a release younger than 3 days is not installed; a wrong checksum and a version mismatch are refused; then a valid
#      release is installed silently while openFences runs; openFences closes itself for it and is restarted on the new
#      version by its relaunch helper.
#   4. Elevated (UAC prompt): removes the policy.
# Build the baseline from the same sources with an older version number (see CLAUDE.md, "UpdateTests.ps1").
#   powershell -ExecutionPolicy Bypass -File tests\integration\UpdateTests.ps1 -BaselineMsi <old.msi> -TargetMsi out\openFences-<ver>-x64.msi -WorkDir <empty folder>
param([string]$BaselineMsi, [string]$TargetMsi, [string]$WorkDir, [int]$Port = 8899, [string]$ElevatedStep = '')
$ErrorActionPreference = 'Stop'
$policyKey = 'HKLM:\SOFTWARE\Policies\openFences'
$updateKey = 'HKLM:\SOFTWARE\openFences\Update'
$installDir = Join-Path $env:ProgramFiles 'openFences'
$exe = Join-Path $installDir 'openFences.exe'

# ---- elevated helper steps (the script relaunches itself with -ElevatedStep)
if ($ElevatedStep -eq 'install') {
    # A newer installed version would block the baseline (no downgrades), so start clean: remove every installed openFences.
    $installer = New-Object -ComObject WindowsInstaller.Installer
    $related = $installer.GetType().InvokeMember('RelatedProducts', 'GetProperty', $null, $installer, @('{7B3E9A41-2C5D-4F86-A1E3-9D04C6B8F512}'))
    foreach ($code in @($related)) {
        if ($code) { Start-Process -FilePath "$env:SystemRoot\System32\msiexec.exe" -ArgumentList "/x $code /qn /norestart" -Wait | Out-Null }
    }
    $log = Join-Path $WorkDir 'baseline-install.log'
    $p = Start-Process -FilePath "$env:SystemRoot\System32\msiexec.exe" -ArgumentList "/i `"$BaselineMsi`" /qn /norestart /l*v `"$log`"" -Wait -PassThru
    New-Item -Path $policyKey -Force | Out-Null
    Set-ItemProperty -Path $policyKey -Name UpdateFeedUrl -Value "http://127.0.0.1:$Port/latest.json"
    Set-Content -Path (Join-Path $WorkDir 'elevated-install.txt') -Value $p.ExitCode
    exit 0
}
if ($ElevatedStep -eq 'cleanup') {
    Remove-ItemProperty -Path $policyKey -Name UpdateFeedUrl -ErrorAction SilentlyContinue
    if ((Get-Item $policyKey -ErrorAction SilentlyContinue) -and -not (Get-Item $policyKey).Property) { Remove-Item $policyKey -ErrorAction SilentlyContinue }
    Set-Content -Path (Join-Path $WorkDir 'elevated-cleanup.txt') -Value 'done'
    exit 0
}

$results = @()
function Check($name, $cond, $detail = '') { $script:results += [pscustomobject]@{ Test = $name; Result = $(if ($cond) { 'PASS' } else { 'FAIL' }); Detail = $detail } }
function Elevated($step) {
    $args2 = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", '-ElevatedStep', $step, '-WorkDir', "`"$WorkDir`"",
        '-BaselineMsi', "`"$BaselineMsi`"", '-Port', $Port)
    Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList $args2 -Verb RunAs -Wait | Out-Null
}
function UpdateValue($name) { (Get-ItemProperty -Path $updateKey -Name $name -ErrorAction SilentlyContinue).$name }
function InstalledVersion { if (Test-Path $exe) { (Get-Item $exe).VersionInfo.ProductVersion } }
function AppProcess { Get-CimInstance Win32_Process -Filter "Name='openFences.exe'" | Where-Object { $_.CommandLine -notmatch '--watchdog|--update-service' } }
function MsiVersion($path) {
    $installer = New-Object -ComObject WindowsInstaller.Installer
    $db = $installer.GetType().InvokeMember('OpenDatabase', 'InvokeMethod', $null, $installer, @($path, 0))
    $view = $db.GetType().InvokeMember('OpenView', 'InvokeMethod', $null, $db, @("SELECT Value FROM Property WHERE Property='ProductVersion'"))
    $view.GetType().InvokeMember('Execute', 'InvokeMethod', $null, $view, $null) | Out-Null
    $rec = $view.GetType().InvokeMember('Fetch', 'InvokeMethod', $null, $view, $null)
    $rec.GetType().InvokeMember('StringData', 'GetProperty', $null, $rec, 1)
}
# Writes latest.json in GitHub's format. $assetVersion names the asset; $digest may be wrong on purpose.
function WriteFeed($tag, $assetVersion, $publishedUtc, $digest) {
    $name = "openFences-$assetVersion-x64.msi"
    Copy-Item $TargetMsi (Join-Path $feedDir $name) -Force
    $json = [ordered]@{ tag_name = $tag; draft = $false; prerelease = $false; published_at = $publishedUtc.ToString('yyyy-MM-ddTHH:mm:ssZ')
        assets = @([ordered]@{ name = $name; browser_download_url = "http://127.0.0.1:$Port/$name"; digest = "sha256:$digest" }) }
    $json | ConvertTo-Json -Depth 5 | Set-Content -Path (Join-Path $feedDir 'latest.json') -Encoding ascii
}
# Runs a check through the running openFences (--check-updates starts the service with "check-now") and waits for it.
function RunCheck {
    $before = UpdateValue LastCheck
    & $exe --check-updates
    $deadline = (Get-Date).AddSeconds(150)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        $svc = Get-Service openFencesUpdate -ErrorAction SilentlyContinue
        if ((UpdateValue LastCheck) -ne $before -and $svc -and $svc.Status -eq 'Stopped') { return $true }
    }
    return $false
}

# ---- preparation
New-Item -ItemType Directory -Force $WorkDir | Out-Null
$feedDir = Join-Path $WorkDir 'feed'
New-Item -ItemType Directory -Force $feedDir | Out-Null
$targetVersion = MsiVersion $TargetMsi
$baselineVersion = MsiVersion $BaselineMsi
$digest = (Get-FileHash $TargetMsi -Algorithm SHA256).Hash.ToLower()
$savedRun = (Get-ItemProperty HKCU:\Software\Microsoft\Windows\CurrentVersion\Run -Name openFences -ErrorAction SilentlyContinue).openFences
foreach ($p in @(AppProcess)) { & $p.ExecutablePath --shutdown; Start-Sleep -Seconds 3 }

# ---- 1. install the baseline (elevated)
Elevated 'install'
Check 'Baseline installed' ((InstalledVersion) -eq $baselineVersion) "installed $(InstalledVersion), baseline $baselineVersion"
$sd = (& sc.exe sdshow openFencesUpdate) -join ''
Check 'Update service installed, demand start' ((Get-Service openFencesUpdate -ErrorAction SilentlyContinue).StartType -eq 'Manual')
Check 'Users may start the service' ($sd -match '\(A;;CCLCSWRPLOCRRC;;;BU\)') $sd
$acl = Get-Acl $installDir
$usersAce = @($acl.Access | Where-Object { $_.IdentityReference -match 'BUILTIN\\Users' })
Check 'Install folder: protected ACL, Users read and execute only' ($acl.AreAccessRulesProtected -and $usersAce.Count -ge 1 -and
    -not ($usersAce | Where-Object { $_.FileSystemRights -match 'Write|Modify|FullControl' })) (($usersAce | ForEach-Object { $_.FileSystemRights }) -join ', ')
Check 'Installer default: automatic updates on' ((UpdateValue DefaultAutoUpdate) -eq 1)
try {
    Set-ItemProperty -Path $updateKey -Name InstallImmediately -Value 0 -Type DWord
    Set-ItemProperty -Path $updateKey -Name Enabled -Value 1 -Type DWord
    $userWrite = $true
} catch { $userWrite = $false }
Check 'Standard user can change the update settings' $userWrite

# ---- 2. start the feed and the installed openFences (as this user, not elevated)
$server = Start-Process -FilePath python -ArgumentList '-m', 'http.server', $Port, '--bind', '127.0.0.1', '--directory', "`"$feedDir`"" -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 2
Start-Process $exe
Start-Sleep -Seconds 6
$appLog = Join-Path $env:LOCALAPPDATA 'openFences\logs\openFences.log'
Check 'openFences uses the update service' ((Get-Content $appLog -Tail 40) -match 'Updates: installed by the update service') ''

# ---- 3a. a release younger than 3 days waits
WriteFeed "v$targetVersion" $targetVersion ([DateTime]::UtcNow.AddHours(-2)) $digest
$done = RunCheck
Check 'Check of a 2-hour-old release finishes' $done
Check 'Too new: not installed yet' ((UpdateValue ResultCode) -eq 2 -and (InstalledVersion) -eq $baselineVersion) "$(UpdateValue Result)"
Check 'Too new: days left reported' ((UpdateValue DaysLeft) -eq 3) "$(UpdateValue DaysLeft)"

# ---- 3b. wrong checksum
WriteFeed "v$targetVersion" $targetVersion ([DateTime]::UtcNow.AddDays(-5)) ('0' * 64)
RunCheck | Out-Null
Check 'Wrong checksum: refused' ((UpdateValue ResultCode) -eq 5 -and (UpdateValue Result) -match 'checksum' -and (InstalledVersion) -eq $baselineVersion) "$(UpdateValue Result)"

# ---- 3c. the package's version doesn't match the release
$parts = $targetVersion.Split('.'); $fake = "$($parts[0]).$($parts[1]).$([int]$parts[2] + 1)"
WriteFeed "v$fake" $fake ([DateTime]::UtcNow.AddDays(-5)) $digest
RunCheck | Out-Null
Check 'Version mismatch: refused' ((UpdateValue ResultCode) -eq 5 -and (UpdateValue Result) -match "doesn't match the release" -and (InstalledVersion) -eq $baselineVersion) "$(UpdateValue Result)"

# ---- 3d. a valid release, 5 days old: silent install while openFences runs; it closes itself and comes back
WriteFeed "v$targetVersion" $targetVersion ([DateTime]::UtcNow.AddDays(-5)) $digest
$oldPid = (AppProcess | Select-Object -First 1).ProcessId
RunCheck | Out-Null
Check 'Valid release: installation started' ((UpdateValue ResultCode) -in 3, 4) "$(UpdateValue Result)"
$deadline = (Get-Date).AddSeconds(240)
while ((Get-Date) -lt $deadline -and -not ((InstalledVersion) -eq $targetVersion -and (AppProcess))) { Start-Sleep -Seconds 3 }
Check 'Upgraded silently to the new version' ((InstalledVersion) -eq $targetVersion) "installed $(InstalledVersion)"
$newApp = AppProcess | Select-Object -First 1
Check 'openFences was restarted on the new version' ($newApp -and $newApp.ProcessId -ne $oldPid) "$($newApp.CommandLine)"
Start-Sleep -Seconds 5
Check 'The restarted openFences reports the update' ((UpdateValue ResultCode) -eq 4) "$(UpdateValue Result)"
if (-not (AppProcess)) { Start-Process $exe }

# ---- 4. cleanup
Stop-Process -Id $server.Id -ErrorAction SilentlyContinue
Elevated 'cleanup'
Check 'Test policy removed' (-not (Get-ItemProperty -Path $policyKey -Name UpdateFeedUrl -ErrorAction SilentlyContinue))
$runNow = (Get-ItemProperty HKCU:\Software\Microsoft\Windows\CurrentVersion\Run -Name openFences -ErrorAction SilentlyContinue).openFences
if ($savedRun -and $runNow -ne $savedRun) { Set-ItemProperty HKCU:\Software\Microsoft\Windows\CurrentVersion\Run -Name openFences -Value $savedRun }
$results | Format-Table -AutoSize | Out-String -Width 220
