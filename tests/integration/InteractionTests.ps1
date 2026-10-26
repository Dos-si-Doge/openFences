# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# Drives a running openFences through posted window messages (the real mouse is never moved) and checks
# the saved configuration. Starts from an empty data directory, so the first-run layout is created.
#   powershell -ExecutionPolicy Bypass -File tests\integration\InteractionTests.ps1 -Exe <path\openFences.exe> -DataDir <empty folder>
param([string]$Exe, [string]$DataDir)
$ErrorActionPreference = "Stop"
$env:OPENFENCES_DATA_DIR = $DataDir
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class U {
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string c, string t);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr p, IntPtr a, string c, string t);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint cmd);
  [DllImport("user32.dll")] public static extern IntPtr GetWindowLongPtrW(IntPtr h, int i);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
  public struct RECT { public int L,T,R,B; }
  public struct POINT { public int X,Y; }
  public static List<IntPtr> ByClass(string cls) {
    var list = new List<IntPtr>();
    EnumWindows((h, l) => { var sb = new StringBuilder(256); GetClassNameW(h, sb, 256); if (sb.ToString() == cls) list.Add(h); return true; }, IntPtr.Zero);
    return list;
  }
  public static string Title(IntPtr h) { var sb = new StringBuilder(256); GetWindowTextW(h, sb, 256); return sb.ToString(); }
}
"@
[U]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null

$WM_MOUSEMOVE = 0x200; $WM_LBUTTONDOWN = 0x201; $WM_LBUTTONUP = 0x202; $WM_LBUTTONDBLCLK = 0x203
$results = @()
# Synthetic drags only work while openFences' mouse capture is inert; otherwise Windows feeds real-cursor moves into the
# drag and the fence follows the real pointer. Two situations make the capture live, and the drag checks are then
# reported as SKIP (the real mouse is never moved to avoid them):
#  - an explorer.exe window (File Explorer, Control Panel) is in the foreground: openFences' windows are owned by the
#    Explorer desktop and share its input queue;
#  - the real pointer rests over an openFences window (a fence or the empty desktop). Any real mouse event then goes to
#    the capturing window. Virtual machines send pointer events even while the mouse is idle or outside the VM window.
# Returns '' when drags are reliable, otherwise the reason.
function DragsUnreliableReason {
  $procId = 0
  [U]::GetWindowThreadProcessId([U]::GetForegroundWindow(), [ref]$procId) | Out-Null
  $p = Get-Process -Id $procId -ErrorAction SilentlyContinue
  if ($p -and $p.ProcessName -eq 'explorer') { return 'an Explorer window is in the foreground (shared input); rerun with another app in front' }
  $pt = New-Object U+POINT; [U]::GetCursorPos([ref]$pt) | Out-Null
  $under = 0; [U]::GetWindowThreadProcessId([U]::WindowFromPoint($pt), [ref]$under) | Out-Null
  $p = Get-Process -Id $under -ErrorAction SilentlyContinue
  if ($p -and $p.ProcessName -eq 'openFences') { return "the mouse pointer rests on openFences ($($pt.X),$($pt.Y)); rerun with it over another window, such as this terminal" }
  return ''
}
function CheckDrag($name, $cond, $detail = '') {
  if (-not $script:dragsUnreliable) { Check $name $cond $detail }
  else { $script:results += [pscustomobject]@{ Test = $name; Result = 'SKIP'; Detail = $script:dragsUnreliable } }
}
function Check($name, $cond, $detail = '') {
  $script:results += [pscustomobject]@{ Test = $name; Result = $(if ($cond) { 'PASS' } else { 'FAIL' }); Detail = $detail }
}
function MakeLParam($x, $y) { [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF)) }
function Rect($h) { $r = New-Object U+RECT; [U]::GetWindowRect($h, [ref]$r) | Out-Null; $r }
function Fence($title) { foreach ($h in [U]::ByClass('openFences.Fence')) { if ([U]::Title($h) -eq $title) { return $h } }; return [IntPtr]::Zero }
function Config { Get-Content -Raw (Join-Path $DataDir 'config.json') | ConvertFrom-Json }
function Layout { $c = Config; ($c.layouts.PSObject.Properties | Select-Object -First 1).Value }
function Groups { $l = Layout; @($l.persistent) + @($l.pages[$l.currentPage].groups) | Where-Object { $_ } }
function GroupTitled($t) { Groups | Where-Object { $_.tabs[0].title -eq $t } | Select-Object -First 1 }
function Post($h, $msg, $x, $y, $keys = 0) { [U]::PostMessageW($h, $msg, [IntPtr]$keys, (MakeLParam $x $y)) | Out-Null }
# Drags in screen coordinates; converts to the window's current client coordinates at each step.
function Drag($h, $sx, $sy, $ex, $ey, $steps = 8) {
  # An unreliable drag follows the real pointer and can merge fences at random, which would break the later checks.
  if ($script:dragsUnreliable) { return }
  $r = Rect $h; Post $h $WM_LBUTTONDOWN ($sx - $r.L) ($sy - $r.T) 1; Start-Sleep -Milliseconds 80
  for ($i = 1; $i -le $steps; $i++) {
    $x = [int]($sx + ($ex - $sx) * $i / $steps); $y = [int]($sy + ($ey - $sy) * $i / $steps)
    $r = Rect $h; Post $h $WM_MOUSEMOVE ($x - $r.L) ($y - $r.T) 1; Start-Sleep -Milliseconds 60
  }
  $r = Rect $h; Post $h $WM_LBUTTONUP ($ex - $r.L) ($ey - $r.T); Start-Sleep -Milliseconds 300
}
function WaitSave { Start-Sleep -Milliseconds 1600 }

# --- start
$lv = [IntPtr]::Zero
$progman = [U]::FindWindowW('Progman', [NullString]::Value)
$dv = [U]::FindWindowExW($progman, [IntPtr]::Zero, 'SHELLDLL_DefView', [NullString]::Value)
if ($dv -ne [IntPtr]::Zero) { $lv = [U]::FindWindowExW($dv, [IntPtr]::Zero, 'SysListView32', [NullString]::Value) }
Check 'Explorer icons visible before start' ([U]::IsWindowVisible($lv))

# --no-welcome: the first start would otherwise open "Welcome to openFences", making openFences the foreground app, and
# mouse capture during the synthetic drags below would then also pick up the real cursor. WelcomeTests covers it.
Start-Process $Exe -ArgumentList '--verbose', '--no-welcome'
Start-Sleep -Seconds 4
Check 'Explorer icon view hidden while running' (-not [U]::IsWindowVisible($lv))
$canvases = [U]::ByClass('openFences.Desktop')
Check 'One desktop canvas per monitor' ($canvases.Count -ge 1) "$($canvases.Count) canvases"
$owner = [U]::GetWindowLongPtrW($canvases[0], -8)
Check 'Canvas is owned by the desktop (Progman)' ($owner -eq $progman)

$prog = Fence 'Programs'
Check 'Programs fence window exists' ($prog -ne [IntPtr]::Zero)
$r0 = Rect $prog
$fullH = $r0.B - $r0.T

# --- roll-up: double-click the title bar
Post $prog $WM_LBUTTONDBLCLK 150 10; Start-Sleep -Milliseconds 400
$r1 = Rect $prog
Check 'Double-click title rolls the fence up' (($r1.B - $r1.T) -lt 60) "height $($r1.B - $r1.T) (was $fullH)"
WaitSave
Check 'Roll-up state is saved' ((GroupTitled 'Programs').rolledUp -eq $true)
Post $prog $WM_LBUTTONDBLCLK 150 10; Start-Sleep -Milliseconds 400
$r2 = Rect $prog
Check 'Second double-click rolls it down' (($r2.B - $r2.T) -eq $fullH) "height $($r2.B - $r2.T)"

# --- move with snapping
$script:dragsUnreliable = DragsUnreliableReason
$files = Fence 'Files and documents'
$rf = Rect $files
Drag $files ($rf.L + 200) ($rf.T + 10) ($rf.L + 200 + 37) ($rf.T + 10 + 403)
$rf2 = Rect $files
CheckDrag 'Dragging the title moves the fence' ([Math]::Abs(($rf2.T - $rf.T) - 403) -lt 40) "moved by $($rf2.L - $rf.L),$($rf2.T - $rf.T)"
WaitSave
$g = GroupTitled 'Files and documents'
CheckDrag 'New position is saved as monitor fractions' ($g.rect[1] -gt 0.2) "rect $($g.rect -join ',')"

# --- resize from the right edge
$rp = Rect $prog
Drag $prog ($rp.R - 2) ($rp.T + 100) ($rp.R + 120) ($rp.T + 100)
$rp2 = Rect $prog
CheckDrag 'Dragging the right edge resizes' ((($rp2.R - $rp2.L) - ($rp.R - $rp.L)) -gt 80) "width $($rp.R - $rp.L) -> $($rp2.R - $rp2.L)"

# --- merge: drop the Folders title onto the Programs title
$fold = Fence 'Folders'
$rfo = Rect $fold; $rp = Rect $prog
Drag $fold ($rfo.L + 120) ($rfo.T + 10) ($rp.L + 120) ($rp.T + 10) 12
Start-Sleep -Milliseconds 500
WaitSave
$merged = GroupTitled 'Programs'
CheckDrag 'Dropping a title on another fence merges into tabs' ($merged.tabs.Count -eq 2) "tabs: $($merged.tabs.title -join ', ')"
CheckDrag 'Merged fence window is gone' ((Fence 'Folders') -eq [IntPtr]::Zero -or -not [U]::IsWindowVisible((Fence 'Folders')))

# --- detach: drag the second tab down out of the title bar
$rp = Rect $prog
$tabX = $rp.L + [int](($rp.R - $rp.L) * 0.5) + 38
Drag $prog $tabX ($rp.T + 12) ($tabX + 40) ($rp.T + 300) 10
Start-Sleep -Milliseconds 500
WaitSave
$after = GroupTitled 'Programs'
CheckDrag 'Dragging a tab out makes a separate fence' ($after.tabs.Count -eq 1 -and @(Groups).Count -eq 3) "groups: $((Groups | ForEach-Object { $_.tabs[0].title }) -join ', ')"

# --- quick-hide: double-click empty desktop
$cv = $canvases[0]; $rc = Rect $cv
Post $cv $WM_LBUTTONDBLCLK ($rc.R - $rc.L - 300) ($rc.B - $rc.T - 200); Start-Sleep -Milliseconds 900
Check 'Double-click on empty desktop hides fences' (-not [U]::IsWindowVisible($prog))
WaitSave
Check 'Hidden state is saved' ((Config).hide.hidden -eq $true)
Post $cv $WM_LBUTTONDBLCLK ($rc.R - $rc.L - 300) ($rc.B - $rc.T - 200); Start-Sleep -Milliseconds 900
Check 'Second double-click shows them again' ([U]::IsWindowVisible($prog))

# --- CLI commands
& $Exe --new-fence; Start-Sleep -Milliseconds 800; WaitSave
Check '--new-fence creates a fence' (@(Groups).Count -eq 4)
& $Exe --snapshot "Harness"; Start-Sleep -Milliseconds 500
Check '--snapshot writes a snapshot' ((Get-ChildItem (Join-Path $DataDir 'Backups') -Filter 'snap-*Harness.json').Count -eq 1)
$export = Join-Path $DataDir 'export.json'
& $Exe --export $export; Start-Sleep -Milliseconds 500
Check '--export writes a layout template' ((Test-Path $export) -and ((Get-Content -Raw $export | ConvertFrom-Json).template.name -eq 'Exported layout'))
& $Exe --apply-template (Join-Path (Split-Path $Exe) 'templates\work.json'); Start-Sleep -Milliseconds 1200; WaitSave
Check 'Applying the Work template replaces fences' (@(Groups | Where-Object { $_.tabs[0].title -eq 'Downloads' }).Count -eq 1) "groups: $((Groups | ForEach-Object { $_.tabs[0].title }) -join ', ')"
Check 'Work template portal window exists' ((Fence 'Downloads') -ne [IntPtr]::Zero)
& $Exe --next-page; Start-Sleep -Milliseconds 600
& $Exe --restore (Get-ChildItem (Join-Path $DataDir 'Backups') -Filter 'snap-*Harness.json').FullName; Start-Sleep -Milliseconds 1200; WaitSave
Check '--restore brings back the snapshot layout' (@(Groups).Count -eq 4) "groups: $((Groups | ForEach-Object { $_.tabs[0].title }) -join ', ')"

# --- shutdown restores Explorer's icons
& $Exe --shutdown; Start-Sleep -Seconds 2
Check 'Shutdown exits all openFences processes' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -eq 0)
Check 'Explorer icon view visible again after exit' ([U]::IsWindowVisible($lv))

$results | Format-Table -AutoSize | Out-String -Width 200
"{0} passed, {1} failed, {2} skipped" -f @($results | Where-Object Result -eq 'PASS').Count, @($results | Where-Object Result -eq 'FAIL').Count, @($results | Where-Object Result -eq 'SKIP').Count
