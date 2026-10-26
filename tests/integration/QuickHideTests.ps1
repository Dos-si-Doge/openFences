# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# Quick-Hide keeps excluded desktop icons usable: double-clicking one opens it without turning Quick-Hide off,
# while a double-click on empty desktop still toggles it. Opens (and then closes) a Recycle Bin window.
param([string]$Exe, [string]$DataDir)
$ErrorActionPreference = 'Stop'
$env:OPENFENCES_DATA_DIR = $DataDir
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class Q {
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public struct RECT { public int L,T,R,B; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  public static List<IntPtr> ByClass(string cls) {
    var list = new List<IntPtr>();
    EnumWindows((h, l) => { var sb = new StringBuilder(256); GetClassNameW(h, sb, 256); if (sb.ToString() == cls && IsWindowVisible(h)) list.Add(h); return true; }, IntPtr.Zero);
    return list;
  }
}
"@
[Q]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null
$results = @()
function Check($name, $cond, $detail = '') { $script:results += [pscustomobject]@{ Test = $name; Result = $(if ($cond) { 'PASS' } else { 'FAIL' }); Detail = $detail } }
$cfgPath = Join-Path $DataDir 'config.json'
$recycle = '::{645FF040-5081-101B-9F08-00AA002F954E}'

# First run creates the default layout; then pin the Recycle Bin, keep it visible, and turn Quick-Hide on.
Start-Process $Exe; Start-Sleep -Seconds 4
& $Exe --shutdown; Start-Sleep -Seconds 2
$cfg = Get-Content -Raw $cfgPath | ConvertFrom-Json
$layout = ($cfg.layouts.PSObject.Properties | Select-Object -First 1).Value
$primary = $layout.pages[0].groups[0].monitor
$cfg.hide.hidden = $true
$cfg.hide.excludeItems = @($recycle)
$layout.loose | Add-Member -NotePropertyName $recycle -NotePropertyValue ([pscustomobject]@{ monitor = $primary; col = 20; row = 2 }) -Force
$cfg | ConvertTo-Json -Depth 30 | Set-Content -Path $cfgPath -Encoding UTF8

$before = @([Q]::ByClass('CabinetWClass'))
Start-Process $Exe; Start-Sleep -Seconds 4
$canvas = $null
foreach ($c in [Q]::ByClass('openFences.Desktop')) { $r = New-Object Q+RECT; [Q]::GetWindowRect($c, [ref]$r) | Out-Null; if ($r.L -eq 0 -and $r.T -eq 0) { $canvas = $c } }
Check 'Primary desktop canvas found' ($null -ne $canvas)
Check 'Quick-Hide is on at start' ((Get-Content -Raw $cfgPath | ConvertFrom-Json).hide.hidden -eq $true)

# Cell (20, 2) at 100% scaling: padding 6, cell 78 x 94.
$x = 6 + 78 * 20 + 39; $y = 6 + 94 * 2 + 47
[Q]::PostMessageW($canvas, 0x0203, [IntPtr]::Zero, [IntPtr](($y -shl 16) -bor $x)) | Out-Null   # WM_LBUTTONDBLCLK
Start-Sleep -Seconds 3
$after = @([Q]::ByClass('CabinetWClass'))
$opened = @($after | Where-Object { $before -notcontains $_ })
Check 'Double-clicking the visible Recycle Bin opens it' ($opened.Count -ge 1) "$($opened.Count) new Explorer window(s)"
Check 'Quick-Hide stays on after opening it' ((Get-Content -Raw $cfgPath | ConvertFrom-Json).hide.hidden -eq $true)
foreach ($w in $opened) { [Q]::PostMessageW($w, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }   # WM_CLOSE

# Control: double-click empty desktop still toggles Quick-Hide off.
[Q]::PostMessageW($canvas, 0x0203, [IntPtr]::Zero, [IntPtr]((700 -shl 16) -bor 2000)) | Out-Null
Start-Sleep -Seconds 2
Check 'Double-clicking empty desktop turns Quick-Hide off' ((Get-Content -Raw $cfgPath | ConvertFrom-Json).hide.hidden -eq $false)

& $Exe --shutdown; Start-Sleep -Seconds 2
Check 'Clean exit' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -eq 0)
$results | Format-Table -AutoSize | Out-String -Width 200
