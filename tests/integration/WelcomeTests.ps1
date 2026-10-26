# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# Checks the built-in "Welcome to openFences" window: it opens on the first start, topics switch, "Open settings" jumps
# to the right Settings page, pictures scale with the window without cropping, and after an upgrade it opens on
# "What's new". Optional -ShotDir saves screenshots. Assumes the light theme (white window background).
#   powershell -ExecutionPolicy Bypass -File tests\integration\WelcomeTests.ps1 -Exe <path\openFences.exe> -DataDir <empty folder>
param([string]$Exe, [string]$DataDir, [string]$ShotDir = '')
$ErrorActionPreference = 'Stop'
$env:OPENFENCES_DATA_DIR = $DataDir
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class W {
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string c, string t);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr p, IntPtr a, string c, string t);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  public struct RECT { public int L,T,R,B; }
  public struct POINT { public int X,Y; }
}
"@
[W]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null
Add-Type -AssemblyName System.Drawing
$results = @()
function Check($name, $cond, $detail = '') { $script:results += [pscustomobject]@{ Test = $name; Result = $(if ($cond) { 'PASS' } else { 'FAIL' }); Detail = $detail } }
function Welcome { [W]::FindWindowW('openFences.Welcome', [NullString]::Value) }
function Nav { [W]::GetDlgItem((Welcome), 1) }
function CurrentTopic { [int][W]::SendMessageW((Nav), 0x0188, [IntPtr]::Zero, [IntPtr]::Zero) }   # LB_GETCURSEL
function SelectTopic($i) {
  [W]::SendMessageW((Nav), 0x0186, [IntPtr]$i, [IntPtr]::Zero) | Out-Null                         # LB_SETCURSEL
  [W]::SendMessageW((Welcome), 0x0111, [IntPtr]((1 -shl 16) -bor 1), (Nav)) | Out-Null            # WM_COMMAND(LBN_SELCHANGE)
  Start-Sleep -Milliseconds 400
}
function Shot($name) {
  if (-not $ShotDir) { return }
  $h = Welcome; [W]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 500
  $r = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null
  $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
  [System.Drawing.Graphics]::FromImage($bmp).CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
  $bmp.Save((Join-Path $ShotDir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
}
# Height of the picture band at the top of the topic page, measured on screen: the band ends where the white page
# background starts (scanning down a column near the left edge). Returns @(pageWidth, bandHeight).
function HeroBand {
  $page = [W]::FindWindowExW((Welcome), [IntPtr]::Zero, 'openFences.WelcomePage', [NullString]::Value)
  [W]::SetForegroundWindow((Welcome)) | Out-Null; Start-Sleep -Milliseconds 700
  $c = New-Object W+RECT; [W]::GetClientRect($page, [ref]$c) | Out-Null
  $o = New-Object W+POINT; [W]::ClientToScreen($page, [ref]$o) | Out-Null
  $bmp = New-Object System.Drawing.Bitmap 1, $c.B
  [System.Drawing.Graphics]::FromImage($bmp).CopyFromScreen($o.X + 3, $o.Y, 0, 0, $bmp.Size)
  $white = 0
  for ($y = 0; $y -lt $c.B; $y++) {
    $px = $bmp.GetPixel(0, $y)
    if ($px.R -eq 255 -and $px.G -eq 255 -and $px.B -eq 255) { $white++; if ($white -ge 8) { return @($c.R, ($y - 7)) } } else { $white = 0 }
  }
  return @($c.R, -1)
}

# 1. First start opens the overview.
Start-Process $Exe; Start-Sleep -Seconds 5
Check 'First start opens Welcome to openFences' ((Welcome) -ne [IntPtr]::Zero -and [W]::IsWindowVisible((Welcome)))
Check 'It opens on the Welcome topic' ((CurrentTopic) -eq 0)
$count = [int][W]::SendMessageW((Nav), 0x018B, [IntPtr]::Zero, [IntPtr]::Zero)   # LB_GETCOUNT
Check 'All topics are listed' ($count -eq 12) "$count topics"
Shot 'welcome-overview'

# 2. Topics switch; "Open settings" opens the matching Settings page.
SelectTopic 7
Check 'Selecting a topic switches the page' ((CurrentTopic) -eq 7)
Shot 'welcome-peek'
$page = [W]::FindWindowExW((Welcome), [IntPtr]::Zero, 'openFences.WelcomePage', [NullString]::Value)
$settingsButton = [W]::GetDlgItem($page, 10)
Check 'Peek topic shows Open settings' ([W]::IsWindowVisible($settingsButton))
[W]::PostMessageW($settingsButton, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null   # BM_CLICK
Start-Sleep -Seconds 1
$settings = [W]::FindWindowW('openFences.Settings', [NullString]::Value)
$sidebar = [W]::GetDlgItem($settings, 1)
Check 'Open settings opens Settings on the Peek page' ([W]::IsWindowVisible($settings) -and [int][W]::SendMessageW($sidebar, 0x0188, [IntPtr]::Zero, [IntPtr]::Zero) -eq 4)
SelectTopic 11
Check "What's new topic has no Settings button" (-not [W]::IsWindowVisible($settingsButton))
Shot 'welcome-whatsnew'
& $Exe --welcome rules; Start-Sleep -Milliseconds 800
Check '--welcome <topic> opens that topic' ((CurrentTopic) -eq 2)

# 2b. Pictures are shown whole: proportional to the page width, and never larger than native size (hero.jpg is 1200x640).
SelectTopic 0
[W]::ShowWindow((Welcome), 3) | Out-Null; Start-Sleep -Milliseconds 800   # SW_MAXIMIZE
$wide = HeroBand
$expected = [math]::Floor(640 * [math]::Min($wide[0], 1200) / 1200)
Check 'Wide window: picture stops at its native size' ([math]::Abs($wide[1] - $expected) -le 3) "page $($wide[0]) px, band $($wide[1]) px, expected $expected"
Shot 'welcome-wide'
[W]::ShowWindow((Welcome), 9) | Out-Null; Start-Sleep -Milliseconds 500   # SW_RESTORE
$r = New-Object W+RECT; [W]::GetWindowRect((Welcome), [ref]$r) | Out-Null
[W]::MoveWindow((Welcome), $r.L, $r.T, [int](($r.R - $r.L) * 0.75), ($r.B - $r.T), $true) | Out-Null; Start-Sleep -Milliseconds 800
$narrow = HeroBand
$expected = [math]::Floor(640 * [math]::Min($narrow[0], 1200) / 1200)
Check 'Narrow window: picture scales down proportionally' ([math]::Abs($narrow[1] - $expected) -le 3) "page $($narrow[0]) px, band $($narrow[1]) px, expected $expected"
Shot 'welcome-narrow'
& $Exe --shutdown; Start-Sleep -Seconds 2

# 3. An upgrade opens "What's new".
$cfgPath = Join-Path $DataDir 'config.json'
$cfg = Get-Content -Raw $cfgPath | ConvertFrom-Json
Check 'The running version is remembered' ($cfg.general.lastSeenVersion -ne '' -and $cfg.general.lastSeenVersion -ne $null) $cfg.general.lastSeenVersion
$cfg.general.lastSeenVersion = '1.0.1'
$cfg | ConvertTo-Json -Depth 30 | Set-Content -Path $cfgPath -Encoding UTF8
Start-Process $Exe; Start-Sleep -Seconds 5
Check "After an upgrade it opens on What's new" ((Welcome) -ne [IntPtr]::Zero -and [W]::IsWindowVisible((Welcome)) -and (CurrentTopic) -eq 11)
& $Exe --shutdown; Start-Sleep -Seconds 2

# 4. No upgrade, no window.
Start-Process $Exe; Start-Sleep -Seconds 4
Check 'A normal start does not open it' ((Welcome) -eq [IntPtr]::Zero -or -not [W]::IsWindowVisible((Welcome)))
& $Exe --shutdown; Start-Sleep -Seconds 2
Check 'Clean exit' ((Get-Process openFences -ErrorAction SilentlyContinue).Count -eq 0)
$results | Format-Table -AutoSize | Out-String -Width 200
