# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# Checks how Settings adapts to its window size, on the Layout page: the snapshot preview scales with the width and keeps
# the monitor's proportions, content stops widening at a readable width, and long labels wrap instead of being cut off.
# Optional -ShotDir saves PrintWindow captures of the Settings window only.
#   powershell -ExecutionPolicy Bypass -File tests\integration\SettingsTests.ps1 -Exe <path\openFences.exe> -DataDir <empty folder>
param([string]$Exe, [string]$DataDir, [string]$ShotDir = '')
$ErrorActionPreference = 'Stop'
if (Get-Process openFences -ErrorAction SilentlyContinue) { throw 'openFences is already running; stop it first so commands reach the test instance.' }
$env:OPENFENCES_DATA_DIR = $DataDir
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class W {
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string c, string t);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetWindowLongPtrW(IntPtr h, int i);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  public struct RECT { public int L,T,R,B; }
  public static List<IntPtr> Children(IntPtr p) { var l = new List<IntPtr>(); EnumChildWindows(p, (h, x) => { l.Add(h); return true; }, IntPtr.Zero); return l; }
  public static string Cls(IntPtr h) { var sb = new StringBuilder(64); GetClassNameW(h, sb, 64); return sb.ToString(); }
  public static string Text(IntPtr h) { var sb = new StringBuilder(512); GetWindowTextW(h, sb, 512); return sb.ToString(); }
}
"@
[W]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
$results = @()
function Check($name, $cond, $detail = '') { $script:results += [pscustomobject]@{ Test = $name; Result = $(if ($cond) { 'PASS' } else { 'FAIL' }); Detail = $detail } }
function Rect($h) { $r = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null; $r }
function Shot($h, $name) {
  if (-not $ShotDir) { return }
  $r = Rect $h
  $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
  $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
  [W]::PrintWindow($h, $dc, 2) | Out-Null   # PW_RENDERFULLCONTENT: this window only
  $g.ReleaseHdc($dc); $g.Dispose()
  $bmp.Save((Join-Path $ShotDir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
}
# The snapshot preview is the page's only owner-drawn static (SS_OWNERDRAW = 0x0D).
function Preview($s) { foreach ($c in [W]::Children($s)) { if (([W]::Cls($c)) -eq 'Static' -and (([int64][W]::GetWindowLongPtrW($c, -16)) -band 0x1F) -eq 0x0D) { return $c } }; [IntPtr]::Zero }
function Control($s, $pattern) { foreach ($c in [W]::Children($s)) { if ([W]::Text($c) -like $pattern) { return $c } }; [IntPtr]::Zero }
function Resize($s, $w) { $r = Rect $s; [W]::MoveWindow($s, $r.L, $r.T, $w, 900, $true) | Out-Null; Start-Sleep -Milliseconds 700 }

Start-Process $Exe -ArgumentList '--no-welcome'; Start-Sleep -Seconds 4
& $Exe --snapshot; Start-Sleep -Seconds 1
& $Exe --settings; Start-Sleep -Seconds 1
$s = [W]::FindWindowW('openFences.Settings', [NullString]::Value)
$side = [W]::GetDlgItem($s, 1)
[W]::SendMessageW($side, 0x0186, [IntPtr]10, [IntPtr]::Zero) | Out-Null                     # LB_SETCURSEL: Layout
[W]::SendMessageW($s, 0x0111, [IntPtr]((1 -shl 16) -bor 1), $side) | Out-Null              # LBN_SELCHANGE
Start-Sleep -Milliseconds 600
$scale = [W]::GetDpiForWindow($s) / 96.0
$screen = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
$aspect = $screen.Width / $screen.Height

# 1. The preview's height follows its width (monitor proportions) instead of staying a fixed strip.
Resize $s ([int](700 * $scale))
$p1 = Rect (Preview $s); $w1 = $p1.R - $p1.L; $h1 = $p1.B - $p1.T
Shot $s 'settings-layout-narrow'
Resize $s ([int](1000 * $scale))
$p2 = Rect (Preview $s); $w2 = $p2.R - $p2.L; $h2 = $p2.B - $p2.T
Shot $s 'settings-layout-wide'
Check 'Snapshot preview is shown' ($w1 -gt 0 -and $w2 -gt 0) "$($w1)x$h1, $($w2)x$h2"
Check 'Preview grows with the window' ($w2 -gt $w1 -and $h2 -gt $h1) "$($w1)x$h1 -> $($w2)x$h2"
$exp2 = [math]::Min([math]::Max($w2 / $aspect, 120 * $scale), 360 * $scale)
Check 'Preview keeps the monitor proportions' ([math]::Abs($h2 - $exp2) -le 2) "height $h2, expected $([int]$exp2)"

# 2. Content stops widening at a readable width in a very wide window.
Resize $s ([int]([math]::Min(1800 * $scale, $screen.Width)))
$p3 = Rect (Preview $s); $w3 = $p3.R - $p3.L
Check 'Content width is capped' ($w3 -le [int](860 * $scale) + 1) "preview $w3 px wide, cap $([int](860 * $scale))"

# 3. In the narrowest window, long labels wrap instead of being cut off.
Resize $s ([int](640 * $scale))
$check = Control $s 'Keep a separate layout for each monitor arrangement*'
$label = Control $s 'Space between snapped fences'
$cr = Rect $check; $lr = Rect $label
Check 'Long checkbox label wraps' (($cr.B - $cr.T) -gt [int](30 * $scale)) "height $($cr.B - $cr.T) px"
Check 'Long slider label wraps' (($lr.B - $lr.T) -gt [int](24 * $scale)) "height $($lr.B - $lr.T) px"
Shot $s 'settings-layout-narrowest'

& $Exe --shutdown; Start-Sleep -Seconds 2
Check 'Clean exit' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -eq 0)
$results | Format-Table -AutoSize | Out-String -Width 200
