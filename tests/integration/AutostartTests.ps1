# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# End-to-end check of "start at sign-in" through the Settings checkbox, including the Task Manager
# "disabled" mark. Uses the real HKCU Run key and leaves it as it found it.
param([string]$Exe, [string]$DataDir)
$ErrorActionPreference = 'Stop'
$env:OPENFENCES_DATA_DIR = $DataDir
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class A {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string c, string t);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr p, IntPtr a, string c, string t);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@
$run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$approved = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run'
$results = @()
function Check($name, $cond, $detail = '') { $script:results += [pscustomobject]@{ Test = $name; Result = $(if ($cond) { 'PASS' } else { 'FAIL' }); Detail = $detail } }
function RunValue { (Get-ItemProperty -Path $run -Name openFences -ErrorAction SilentlyContinue).openFences }
function Checkbox {
  & $Exe --settings welcome; Start-Sleep -Milliseconds 700
  $frame = [A]::FindWindowW('openFences.Settings', [NullString]::Value)
  $panel = [A]::FindWindowExW($frame, [IntPtr]::Zero, 'openFences.SettingsPanel', [NullString]::Value)
  [A]::GetDlgItem($panel, 100)
}
$BM_CLICK = 0xF5; $BM_GETCHECK = 0xF0

Check 'No Run entry before the test' ($null -eq (RunValue))
Start-Process $Exe; Start-Sleep -Seconds 3

$cb = Checkbox
Check 'Start-at-sign-in checkbox starts unchecked' ([A]::SendMessageW($cb, $BM_GETCHECK, 0, 0) -eq 0)
[A]::SendMessageW($cb, $BM_CLICK, 0, 0) | Out-Null; Start-Sleep -Milliseconds 300
$expected = '"' + $Exe + '" --autostart'
Check 'Checking it writes HKCU Run\openFences' ((RunValue) -eq $expected) (RunValue)

# Simulate Task Manager > Startup > Disable.
New-Item -Path $approved -Force | Out-Null
Set-ItemProperty -Path $approved -Name openFences -Type Binary -Value ([byte[]](3,0,0,0,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x01))
$cb = Checkbox
Check 'Disabled in Task Manager shows as unchecked' ([A]::SendMessageW($cb, $BM_GETCHECK, 0, 0) -eq 0)
[A]::SendMessageW($cb, $BM_CLICK, 0, 0) | Out-Null; Start-Sleep -Milliseconds 300
$mark = (Get-ItemProperty -Path $approved -Name openFences -ErrorAction SilentlyContinue).openFences
Check 'Re-enabling clears the Task Manager disabled mark' ($null -eq $mark -and (RunValue) -eq $expected)

[A]::SendMessageW($cb, $BM_CLICK, 0, 0) | Out-Null; Start-Sleep -Milliseconds 300
Check 'Unchecking removes the Run entry' ($null -eq (RunValue))

# The command line Windows runs at sign-in starts the app.
[A]::SendMessageW($cb, $BM_CLICK, 0, 0) | Out-Null; Start-Sleep -Milliseconds 300
$cmd = RunValue
& $Exe --shutdown; Start-Sleep -Seconds 2
Start-Process -FilePath $Exe -ArgumentList '--autostart'; Start-Sleep -Seconds 3
Check 'The sign-in command line starts openFences' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -ge 1) $cmd
& $Exe --unregister-autostart; Start-Sleep -Milliseconds 300
Check '--unregister-autostart (uninstaller) removes the entry' ($null -eq (RunValue))
& $Exe --shutdown; Start-Sleep -Seconds 2
Check 'Clean exit' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -eq 0)

$results | Format-Table -AutoSize | Out-String -Width 200
