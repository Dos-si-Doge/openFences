# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# Terminates openFences with a normal and a crash exit code and checks that the watchdog restores
# Explorer's desktop icons (and restarts the app only after a crash).
param([string]$Exe, [string]$DataDir)
$ErrorActionPreference = 'Stop'
$env:OPENFENCES_DATA_DIR = $DataDir
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class K {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string c, string t);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr p, IntPtr a, string c, string t);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
  [DllImport("kernel32.dll")] public static extern bool TerminateProcess(IntPtr h, uint code);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
}
"@
$results = @()
function Check($name, $cond, $detail = '') { $script:results += [pscustomobject]@{ Test = $name; Result = $(if ($cond) { 'PASS' } else { 'FAIL' }); Detail = $detail } }
function ListView {
  $p = [K]::FindWindowW('Progman', [NullString]::Value)
  $d = [K]::FindWindowExW($p, [IntPtr]::Zero, 'SHELLDLL_DefView', [NullString]::Value)
  [K]::FindWindowExW($d, [IntPtr]::Zero, 'SysListView32', [NullString]::Value)
}
function MainPid {
  $h = [K]::FindWindowW('openFences.Controller', [NullString]::Value); $procId = 0
  [K]::GetWindowThreadProcessId($h, [ref]$procId) | Out-Null; $procId
}
function TerminateApp($procId, $code) { $h = [K]::OpenProcess(0x0001, $false, $procId); [K]::TerminateProcess($h, $code) | Out-Null; [K]::CloseHandle($h) | Out-Null }

Start-Process $Exe; Start-Sleep -Seconds 3
Check 'Icons hidden while running' (-not [K]::IsWindowVisible((ListView)))
$pid1 = MainPid
TerminateApp $pid1 1; Start-Sleep -Seconds 3
Check 'Killed by user: watchdog restores Explorer icons' ([K]::IsWindowVisible((ListView)))
Check 'Killed by user: not restarted' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -eq 0)

Start-Process $Exe; Start-Sleep -Seconds 3
$pid2 = MainPid
TerminateApp $pid2 3221225477; Start-Sleep -Seconds 5
$pid3 = MainPid
Check 'Crash: watchdog restarts openFences' ($pid3 -ne 0 -and $pid3 -ne $pid2) "old $pid2, new $pid3"
Check 'Crash: restarted instance hides icons again' (-not [K]::IsWindowVisible((ListView)))
& $Exe --shutdown; Start-Sleep -Seconds 2
Check 'Clean shutdown afterwards' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -eq 0 -and [K]::IsWindowVisible((ListView)))
$results | Format-Table -AutoSize | Out-String -Width 200
