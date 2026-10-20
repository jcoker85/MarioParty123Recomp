# Draws the app icons (original artwork, no game assets) into icons/:
#   hub.png/.ico  gold dice block with a star (the MarioParty123Recomp menu)
#   mp1..mp3.png/.ico  dice blocks numbered 1, 2, 3 for the three game executables
# Each .ico holds PNG-compressed 256, 128, 64, 48, 32 and 16 pixel images.
#
# Usage (from the repository root): powershell -ExecutionPolicy Bypass -File tools\make_icons.ps1
param([string]$OutDir = "$PSScriptRoot\..\icons")
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $OutDir | Out-Null

function RoundRect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function StarPath([float]$cx, [float]$cy, [float]$outer, [float]$inner) {
    $pts = @()
    for ($i = 0; $i -lt 10; $i++) {
        $r = if ($i % 2 -eq 0) { $outer } else { $inner }
        $a = [Math]::PI * (-90 + 36 * $i) / 180
        $pts += New-Object System.Drawing.PointF(($cx + $r * [Math]::Cos($a)), ($cy + $r * [Math]::Sin($a)))
    }
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $p.AddPolygon([System.Drawing.PointF[]]$pts)
    return $p
}

# A 256px block: rounded square with a vertical gradient, a lighter top bevel and dark outline,
# then either a number or a star in white with a dark outline.
function DrawBlock([System.Drawing.Color]$top, [System.Drawing.Color]$bottom, [string]$label, [string]$path) {
    $s = 256
    $bmp = New-Object System.Drawing.Bitmap($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.Clear([System.Drawing.Color]::Transparent)

    $outline = [System.Drawing.Color]::FromArgb(255, 40, 28, 10)
    $body = RoundRect 10 10 236 236 44
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush((New-Object System.Drawing.Rectangle 0, 10, $s, 236), $top, $bottom, 90)
    $g.FillPath($grad, $body)
    # Top bevel highlight.
    $shine = RoundRect 28 22 200 70 30
    $g.FillPath((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(70, 255, 255, 255))), $shine)
    $g.DrawPath((New-Object System.Drawing.Pen($outline, 12)), $body)

    $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $pen = New-Object System.Drawing.Pen($outline, 14)
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    if ($label -eq "star") {
        $star = StarPath 128 134 96 42
        $g.DrawPath($pen, $star)
        $g.FillPath($white, $star)
    } else {
        $text = New-Object System.Drawing.Drawing2D.GraphicsPath
        $family = New-Object System.Drawing.FontFamily("Segoe UI Black")
        $fmt = New-Object System.Drawing.StringFormat
        $fmt.Alignment = [System.Drawing.StringAlignment]::Center
        $fmt.LineAlignment = [System.Drawing.StringAlignment]::Center
        $text.AddString($label, $family, 0, 190, (New-Object System.Drawing.RectangleF(0, 8, $s, $s)), $fmt)
        $g.DrawPath($pen, $text)
        $g.FillPath($white, $text)
    }
    $g.Dispose()
    $bmp.Save("$path.png", [System.Drawing.Imaging.ImageFormat]::Png)

    # Pack the sizes into one .ico (PNG entries).
    $sizes = 256, 128, 64, 48, 32, 16
    $blobs = @()
    foreach ($z in $sizes) {
        $small = New-Object System.Drawing.Bitmap($z, $z, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $sg = [System.Drawing.Graphics]::FromImage($small)
        $sg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $sg.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $sg.DrawImage($bmp, 0, 0, $z, $z)
        $sg.Dispose()
        $ms = New-Object System.IO.MemoryStream
        $small.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $small.Dispose()
        $blobs += , $ms.ToArray()
    }
    $bmp.Dispose()
    $out = New-Object System.IO.MemoryStream
    $w = New-Object System.IO.BinaryWriter($out)
    $w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $z = $sizes[$i]
        $w.Write([byte]($(if ($z -ge 256) { 0 } else { $z })))
        $w.Write([byte]($(if ($z -ge 256) { 0 } else { $z })))
        $w.Write([byte]0); $w.Write([byte]0)
        $w.Write([UInt16]1); $w.Write([UInt16]32)
        $w.Write([UInt32]$blobs[$i].Length); $w.Write([UInt32]$offset)
        $offset += $blobs[$i].Length
    }
    foreach ($b in $blobs) { $w.Write($b) }
    $w.Flush()
    [System.IO.File]::WriteAllBytes("$path.ico", $out.ToArray())
}

$c = { param($r, $g, $b) [System.Drawing.Color]::FromArgb(255, $r, $g, $b) }
DrawBlock (& $c 255 214 64) (& $c 230 140 0)  "star" "$OutDir\hub"
DrawBlock (& $c 255 92 80)  (& $c 196 20 20)  "1"    "$OutDir\mp1"
DrawBlock (& $c 92 156 255) (& $c 20 70 200)  "2"    "$OutDir\mp2"
DrawBlock (& $c 110 220 100) (& $c 20 140 40) "3"    "$OutDir\mp3"
Get-ChildItem $OutDir | ForEach-Object { "{0} {1}" -f $_.Name, $_.Length }
