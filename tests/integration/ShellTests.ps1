# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# Optional -Shot saves a screen capture of the fence and its menu (popup menus can't be captured with PrintWindow).
# Simulates an Explorer restart (TaskbarCreated), opens the shell context menu inside a fence, and checks the desktop
# menu (no empty "openFences" submenu from the installer's registry entry).
param([string]$Exe, [string]$DataDir, [string]$Shot)
$ErrorActionPreference = 'Stop'
$env:OPENFENCES_DATA_DIR = $DataDir
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class S {
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string c, string t);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessageW(string s);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetWindowLongPtrW(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  public struct RECT { public int L,T,R,B; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  public static List<IntPtr> ByClass(string cls) {
    var list = new List<IntPtr>();
    EnumWindows((h, l) => { var sb = new StringBuilder(256); GetClassNameW(h, sb, 256); if (sb.ToString() == cls && IsWindowVisible(h)) list.Add(h); return true; }, IntPtr.Zero);
    return list;
  }
  public static string Title(IntPtr h) { var sb = new StringBuilder(256); GetWindowTextW(h, sb, 256); return sb.ToString(); }
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr m);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetMenuStringW(IntPtr m, uint pos, StringBuilder s, int n, uint flags);
  // Labels of the popup menu shown in a #32768 window (MN_GETHMENU), with "&" mnemonics removed.
  public static List<string> MenuLabels(IntPtr menuWindow) {
    var list = new List<string>();
    IntPtr m = SendMessageW(menuWindow, 0x01E1, IntPtr.Zero, IntPtr.Zero);
    int n = GetMenuItemCount(m);
    for (int i = 0; i < n; i++) { var sb = new StringBuilder(256); GetMenuStringW(m, (uint)i, sb, 256, 0x400); list.Add(sb.ToString().Replace("&", "")); }
    return list;
  }
}
"@
[S]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
$results = @()
function Check($name, $cond, $detail = '') { $script:results += [pscustomobject]@{ Test = $name; Result = $(if ($cond) { 'PASS' } else { 'FAIL' }); Detail = $detail } }
function Fence($title) { foreach ($h in [S]::ByClass('openFences.Fence')) { if ([S]::Title($h) -eq $title) { return $h } }; [IntPtr]::Zero }

Start-Process $Exe; Start-Sleep -Seconds 3
$progman = [S]::FindWindowW('Progman', [NullString]::Value)
$ctrl = [S]::FindWindowW('openFences.Controller', [NullString]::Value)

# Explorer restart broadcast: windows are rebuilt and re-attached to the desktop.
$before = [S]::ByClass('openFences.Fence')
[S]::PostMessageW($ctrl, [S]::RegisterWindowMessageW('TaskbarCreated'), [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Seconds 2
$after = [S]::ByClass('openFences.Fence')
Check 'TaskbarCreated: fences are recreated' ($after.Count -eq $before.Count -and $after.Count -gt 0) "$($before.Count) -> $($after.Count)"
Check 'TaskbarCreated: new windows (re-attached)' (($after | Where-Object { $before -contains $_ }).Count -eq 0)
Check 'TaskbarCreated: still owned by Progman' ([S]::GetWindowLongPtrW($after[0], -8) -eq $progman)

# Shell context menu on the first item of the Programs fence, shown during Peek so it is visible.
& $Exe --peek; Start-Sleep -Milliseconds 800
$prog = Fence 'Programs'
$r = New-Object S+RECT; [S]::GetWindowRect($prog, [ref]$r) | Out-Null
$x = 45; $y = 70   # first icon cell
[S]::PostMessageW($prog, 0x0205, [IntPtr]::Zero, [IntPtr](($y -shl 16) -bor $x)) | Out-Null   # WM_RBUTTONUP
Start-Sleep -Seconds 2
$menus = [S]::ByClass('#32768')
Check 'Right-click opens a popup menu' ($menus.Count -ge 1) "$($menus.Count) menu window(s)"
if ($menus.Count -ge 1 -and $Shot) {
  $m = New-Object S+RECT; [S]::GetWindowRect($menus[0], [ref]$m) | Out-Null
  $L = [Math]::Min($r.L, $m.L); $T = [Math]::Min($r.T, $m.T); $R = [Math]::Max($r.R, $m.R); $B = [Math]::Max($r.B, $m.B)
  $bmp = New-Object System.Drawing.Bitmap ($R - $L), ($B - $T)
  [System.Drawing.Graphics]::FromImage($bmp).CopyFromScreen($L, $T, 0, 0, $bmp.Size)
  $bmp.Save($Shot, [System.Drawing.Imaging.ImageFormat]::Png)
}
[S]::PostMessageW($prog, 0x001F, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null   # WM_CANCELMODE closes the menu
Start-Sleep -Milliseconds 800
Check 'Menu dismissed' (([S]::ByClass('#32768')).Count -eq 0)
& $Exe --peek; Start-Sleep -Milliseconds 500

# Desktop menu: right-click empty desktop (bottom-right of the primary canvas) and read the menu.
$canvas = $null
foreach ($h in [S]::ByClass('openFences.Desktop')) { $cr = New-Object S+RECT; [S]::GetWindowRect($h, [ref]$cr) | Out-Null; if ($cr.L -eq 0 -and $cr.T -eq 0) { $canvas = $h } }
$c = New-Object S+RECT; [S]::GetClientRect($canvas, [ref]$c) | Out-Null
$x = $c.R - 40; $y = $c.B - 40
[S]::PostMessageW($canvas, 0x0205, [IntPtr]::Zero, [IntPtr](($y -shl 16) -bor $x)) | Out-Null   # WM_RBUTTONUP
Start-Sleep -Seconds 2
$menus = [S]::ByClass('#32768')
$labels = if ($menus.Count -ge 1) { [S]::MenuLabels($menus[0]) } else { @() }
$registered = Test-Path 'Registry::HKEY_CLASSES_ROOT\DesktopBackground\Shell\openFences'
Check 'Desktop menu opens with openFences entries' (@($labels -like 'Configure openFences*').Count -eq 1 -and $labels -contains 'New fence here') "$($labels.Count) items: $(($labels | Where-Object { $_ }) -join ' | ')"
$visible = @($labels | Where-Object { $_ })
Check 'Configure openFences is the last desktop menu item' ($visible.Count -gt 0 -and $visible[-1] -like 'Configure openFences*') "last: $($visible[-1])"
Check 'Desktop menu has no empty openFences submenu' (-not ($labels -contains 'openFences')) "installer menu entry registered: $registered"
[S]::PostMessageW($canvas, 0x001F, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null   # WM_CANCELMODE
Start-Sleep -Milliseconds 800
Check 'Desktop menu dismissed' (([S]::ByClass('#32768')).Count -eq 0)

& $Exe --shutdown; Start-Sleep -Seconds 2
Check 'Clean exit' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -eq 0)
$results | Format-Table -AutoSize | Out-String -Width 200
