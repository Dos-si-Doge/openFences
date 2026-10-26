# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

# Renders the hero images of the built-in "Welcome to openFences" window into res\welcome\*.jpg.
# They are the same illustrations and screenshots the HTML user guide uses, so run this after changing them.
# Requires Python and Microsoft Edge (used headless to rasterize the SVG illustrations).
#   powershell -ExecutionPolicy Bypass -File scripts\render_welcome_images.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$outDir = Join-Path $root 'res\welcome'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Add-Type -AssemblyName System.Drawing

# Illustrations used as heroes (see the `hero` fields in src\core\Welcome.cpp), and screenshots from the guide.
$illustrations = 'hero', 'anatomy', 'portal', 'tabs', 'rollup', 'quickhide', 'peek', 'pages', 'appearance'
$screenshots = 'settings-rules', 'settings-layout'
$width = 1200

$edge = @("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe", "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $edge) { throw 'Microsoft Edge was not found.' }
$python = (Get-Command python -ErrorAction Stop).Source

function Save-Jpeg([System.Drawing.Image]$image, [string]$path) {
    $encoder = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
    $params = New-Object System.Drawing.Imaging.EncoderParameters 1
    $params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), 88L
    $image.Save($path, $encoder, $params)
}

$work = Join-Path $env:TEMP "openfences-welcome-$PID"
New-Item -ItemType Directory -Force -Path $work | Out-Null
try {
    $sizes = @{}
    & $python -I (Join-Path $root 'scripts\build_user_guide.py') --export-pages $work | ForEach-Object {
        $name, $w, $h = $_ -split ' '
        $sizes[$name] = [double]$h / [double]$w
    }
    foreach ($name in $illustrations) {
        if (-not $sizes.ContainsKey($name)) { throw "Unknown illustration '$name'." }
        $height = [int][Math]::Round($width * $sizes[$name])
        $png = Join-Path $work "$name.png"
        $url = 'file:///' + ((Join-Path $work "$name.html") -replace '\\', '/')
        # Start-Process: Edge reports progress on stderr, which Windows PowerShell would turn into an error.
        $edgeArgs = '--headless=new', '--disable-gpu', '--hide-scrollbars', "--user-data-dir=`"$work\edge`"", "--window-size=$width,$height",
            "--screenshot=`"$png`"", "`"$url`""
        Start-Process -FilePath $edge -ArgumentList $edgeArgs -Wait -WindowStyle Hidden
        if (-not (Test-Path $png)) { throw "Rendering $name failed." }
        $image = [System.Drawing.Image]::FromFile($png)
        try { Save-Jpeg $image (Join-Path $outDir "$name.jpg") } finally { $image.Dispose() }
        Write-Host "res\welcome\$name.jpg"
    }
    foreach ($name in $screenshots) {
        $image = [System.Drawing.Image]::FromFile((Join-Path $root "docs\user-guide\images\$name.png"))
        try { Save-Jpeg $image (Join-Path $outDir "$name.jpg") } finally { $image.Dispose() }
        Write-Host "res\welcome\$name.jpg"
    }
} finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
