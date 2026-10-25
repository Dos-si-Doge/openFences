# Simulates an Explorer restart (TaskbarCreated) and opens the shell context menu inside a fence.
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
if ($menus.Count -ge 1) {
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
& $Exe --shutdown; Start-Sleep -Seconds 2
Check 'Clean exit' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -eq 0)
$results | Format-Table -AutoSize | Out-String -Width 200
