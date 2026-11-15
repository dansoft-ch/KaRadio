# Erzeugt Etiketten (6 mm Band, Brother P-touch) mit dem Pinout des ESP32-WROVER.
# Raster 2,54 mm = 18 Punkte bei 180 dpi (Aufloesung der P-touch-Drucker).
# Ausgabe: etikette_links.png, etikette_rechts.png (Druckdaten, 180 dpi)
#          vorschau.png (8-fach vergroessert, nur zum Anschauen)
# Aufruf:  powershell -ExecutionPolicy Bypass -File docs\etikette\etikette.ps1
#
# Reihenfolge: Pin 1 = Antennen-Ende (3V3 bzw. GND), letzter Pin = USB-Ende.
# Belegt = schwarz hinterlegt (weisse Schrift). Kuerzel wie im Factsheet:
#   Tst1..Tst6 = Sendertasten (GPIO 13, 14, 32, 15, 5, 2)
#   Pvol = Lautstaerke-Poti (39), Pmte = Mute-Poti (36)
#   LRC/BCLK/DIN = MAX98357A (25/26/22), IR = IR-Empfaenger (21), LED = Status-LED (4)

Add-Type -AssemblyName System.Drawing

# Text, belegt (true = schwarz hinterlegt)
$links = @(
    @('3V3', $false), @('EN', $false), @('Pmte', $true), @('Pvol', $true),
    @('34', $false), @('35', $false), @('Tst3', $true), @('33', $false),
    @('LRC', $true), @('BCLK', $true), @('27', $false), @('Tst2', $true),
    @('12', $false), @('GND', $false), @('Tst1', $true), @('D2', $false),
    @('D3', $false), @('CMD', $false), @('5V', $false)
)
$rechts = @(
    @('GND', $false), @('23', $false), @('DIN', $true), @('TX', $false),
    @('RX', $false), @('IR', $true), @('GND', $false), @('19', $false),
    @('18', $false), @('Tst5', $true), @('17', $false), @('16', $false),
    @('LED', $true), @('0', $false), @('Tst6', $true), @('Tst4', $true),
    @('D1', $false), @('D0', $false), @('CLK', $false)
)
# Verstaerker MAX98357A (7 Pins)
$max98357a = @(
    @('LRC', $true), @('BCLK', $true), @('DIN', $true), @('GAIN', $false),
    @('SD', $false), @('GND', $false), @('Vin', $false)
)

$pitch  = 18     # 2,54 mm bei 180 dpi
$height = 32     # bedruckbare Hoehe auf 6-mm-Band (ca. 4,5 mm)
$dpi    = 180
$gap    = 1      # Abstand zwischen den Buchstaben in Punkten

# Eigene Pixelschrift, 11 Punkte hoch, Striche 2 Punkte breit (fett).
# Eine kleine Druckerschrift laesst bei 3, 5, 6, 0 die Innenraeume zulaufen.
$font = New-Object System.Collections.Hashtable ([StringComparer]::Ordinal)
$font['0'] = '.####.','##..##','##..##','##..##','##..##','##..##','##..##','##..##','##..##','##..##','.####.'
$font['1'] = '..##..','.###..','####..','..##..','..##..','..##..','..##..','..##..','..##..','..##..','######'
$font['2'] = '.####.','##..##','....##','....##','...##.','..##..','.##...','##....','##....','##....','######'
$font['3'] = '.####.','##..##','....##','....##','..###.','....##','....##','....##','....##','##..##','.####.'
$font['4'] = '...##.','..###.','.####.','##.##.','##.##.','##.##.','######','...##.','...##.','...##.','...##.'
$font['5'] = '######','##....','##....','#####.','....##','....##','....##','....##','....##','##..##','.####.'
$font['6'] = '.####.','##..##','##....','##....','#####.','##..##','##..##','##..##','##..##','##..##','.####.'
$font['7'] = '######','....##','....##','...##.','...##.','..##..','..##..','..##..','.##...','.##...','.##...'
$font['8'] = '.####.','##..##','##..##','##..##','.####.','##..##','##..##','##..##','##..##','##..##','.####.'
$font['9'] = '.####.','##..##','##..##','##..##','##..##','.#####','....##','....##','....##','##..##','.####.'
$font['A'] = '.####.','##..##','##..##','##..##','##..##','######','##..##','##..##','##..##','##..##','##..##'
$font['S'] = '.####.','##..##','##....','##....','.####.','....##','....##','....##','....##','##..##','.####.'
$font['n'] = '......','......','......','......','#####.','##..##','##..##','##..##','##..##','##..##','##..##'
$font['i'] = '##','##','..','##','##','##','##','##','##','##','##'
$font['B'] = '#####.','##..##','##..##','##..##','#####.','##..##','##..##','##..##','##..##','##..##','#####.'
$font['C'] = '.####.','##..##','##....','##....','##....','##....','##....','##....','##....','##..##','.####.'
$font['D'] = '####..','##.##.','##..##','##..##','##..##','##..##','##..##','##..##','##..##','##.##.','####..'
$font['E'] = '######','##....','##....','##....','#####.','##....','##....','##....','##....','##....','######'
$font['G'] = '.####.','##..##','##....','##....','##....','##.###','##..##','##..##','##..##','##..##','.####.'
$font['I'] = '####','.##.','.##.','.##.','.##.','.##.','.##.','.##.','.##.','.##.','####'
$font['K'] = '##..##','##..##','##.##.','##.##.','####..','###...','####..','##.##.','##.##.','##..##','##..##'
$font['L'] = '##....','##....','##....','##....','##....','##....','##....','##....','##....','##....','######'
$font['M'] = '##...##','###.###','#######','##.#.##','##.#.##','##...##','##...##','##...##','##...##','##...##','##...##'
$font['N'] = '##...##','##...##','###..##','####.##','##.#.##','##.####','##..###','##...##','##...##','##...##','##...##'
$font['P'] = '#####.','##..##','##..##','##..##','##..##','#####.','##....','##....','##....','##....','##....'
$font['R'] = '#####.','##..##','##..##','##..##','##..##','#####.','####..','##.##.','##.##.','##..##','##..##'
$font['T'] = '######','..##..','..##..','..##..','..##..','..##..','..##..','..##..','..##..','..##..','..##..'
$font['V'] = '##...##','##...##','##...##','##...##','.##.##.','.##.##.','.##.##.','..###..','..###..','..###..','...#...'
$font['X'] = '##..##','##..##','.####.','.####.','..##..','..##..','..##..','.####.','.####.','##..##','##..##'
$font['e'] = '......','......','......','......','.####.','##..##','##..##','######','##....','##..##','.####.'
$font['l'] = '##','##','##','##','##','##','##','##','##','##','##'
$font['m'] = '........','........','........','........','#######.','##.##.##','##.##.##','##.##.##','##.##.##','##.##.##','##.##.##'
$font['o'] = '......','......','......','......','.####.','##..##','##..##','##..##','##..##','##..##','.####.'
$font['s'] = '......','......','......','......','.####.','##....','##....','.####.','....##','....##','.####.'
$font['t'] = '....','.##.','.##.','####','####','.##.','.##.','.##.','.##.','.##.','..##'
$font['v'] = '......','......','......','......','##..##','##..##','##..##','##..##','.####.','.####.','..##..'
$fontHeight = 11

# Liefert die schwarzen Punkte eines Textes (x = Leserichtung, y = nach unten)
function Get-TextPixels($text) {
    $pixels = New-Object System.Collections.Generic.List[object]
    $x = 0
    foreach ($ch in $text.ToCharArray()) {
        $glyph = $font[[string]$ch]
        if (-not $glyph) { throw "Zeichen '$ch' fehlt in der Pixelschrift" }
        for ($y = 0; $y -lt $glyph.Count; $y++) {
            for ($c = 0; $c -lt $glyph[$y].Length; $c++) {
                if ($glyph[$y][$c] -eq '#') { $pixels.Add(@(($x + $c), $y)) }
            }
        }
        $x += $glyph[0].Length + $gap
    }
    [pscustomobject]@{ W = $x - $gap; H = $fontHeight; Pixels = $pixels }
}

function New-Label($pins, $path) {
    $w = $pins.Count * $pitch
    $bmp = New-Object System.Drawing.Bitmap $w, $height
    $bmp.SetResolution($dpi, $dpi)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::White)
    $maxLen = 0

    for ($i = 0; $i -lt $pins.Count; $i++) {
        $text = $pins[$i][0]; $used = $pins[$i][1]
        $x0 = $i * $pitch
        $ink = [System.Drawing.Color]::Black
        if ($used) {
            # 1 Punkt Luecke links/rechts, damit benachbarte Felder getrennt bleiben
            $g.FillRectangle([System.Drawing.Brushes]::Black, $x0 + 1, 0, $pitch - 2, $height)
            $ink = [System.Drawing.Color]::White
        }
        # Text um 90 Grad gedreht eintragen (liest von unten nach oben)
        $line = Get-TextPixels $text
        $maxLen = [Math]::Max($maxLen, $line.W)
        if ($line.W -gt $height) { throw "'$text' ist $($line.W) Punkte lang, Band nur $height" }
        $left = $x0 + [Math]::Floor(($pitch - $line.H) / 2)
        $top  = [Math]::Floor(($height - $line.W) / 2)
        foreach ($p in $line.Pixels) {
            $bmp.SetPixel($left + $p[1], $top + $line.W - 1 - $p[0], $ink)
        }
        # Trennstrich zwischen den Pins (oben und unten kurz)
        if ($i -gt 0) {
            $g.DrawLine([System.Drawing.Pens]::Black, $x0, 0, $x0, 2)
            $g.DrawLine([System.Drawing.Pens]::Black, $x0, $height - 3, $x0, $height - 1)
        }
    }
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    "{0}: {1} Pins, {2:N2} mm lang, Pixelschrift {3} Punkte hoch, laengster Text {4} von {5} Punkten" -f (Split-Path $path -Leaf), $pins.Count, ($w / $dpi * 25.4), $fontHeight, $maxLen, $height
    return $bmp
}

$dir = $PSScriptRoot
$a = New-Label $links  (Join-Path $dir 'etikette_links.png')
$b = New-Label $rechts (Join-Path $dir 'etikette_rechts.png')
$c = New-Label $max98357a (Join-Path $dir 'etikette_max98357a.png')
$labels = @($a, $b, $c)

# Vorschau: alle Etiketten 8-fach vergroessert untereinander
$s = 8
$pv = New-Object System.Drawing.Bitmap ($a[-1].Width * $s + 40), (($height * $s + 20) * $labels.Count + 20)
$g = [System.Drawing.Graphics]::FromImage($pv)
$g.Clear([System.Drawing.Color]::LightGray)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
for ($k = 0; $k -lt $labels.Count; $k++) {
    $l = $labels[$k][-1]
    $g.DrawImage($l, 20, 20 + $k * ($height * $s + 20), $l.Width * $s, $height * $s)
}
$g.Dispose()
$pv.Save((Join-Path $dir 'vorschau.png'), [System.Drawing.Imaging.ImageFormat]::Png)
foreach ($l in $labels) { $l[0..($l.Count - 2)] }

# ---------------------------------------------------------------------------
# P-touch-Editor-Dateien (.lbt) mit den Bildern in fester Groesse.
# Beim Bild-Import streckt der Editor das Bild; hier stehen die Masse fest.
# Massstab und Positionen stammen aus der von Hand eingerichteten Datei
# Ettikette-PinOut-ESP32-WROVER.lbt (Druck passt auf die Stiftleiste):
# 342 Bildpunkte = 72.9752 Editor-Einheiten breit, 32 Bildpunkte = 5.54218 hoch.
# Format (aus Dateien des Editors gelesen): "PLL" 02, u16 16, u32 Textlaenge,
# 6 Nullbytes, Einstellungen als UTF-16LE-Text mit BOM, dann je Bild:
# "PLI" 00, u16 Kopflaenge, u16 28, u32 PNG-Laenge, u32 0, u32 Breite,
# u32 Hoehe, u32 24, Bildname UTF-16 mit Null, PNG-Daten.
# Band wie in den eigenen Dateien: PT-P700, TapeWidth 259.

function D([long]$bits) { [BitConverter]::Int64BitsToDouble($bits) }   # Wert bitgenau wie im Editor

function Format-PtDouble([double]$v) {
    $bits = [BitConverter]::DoubleToInt64Bits($v)
    $sign = ($bits -shr 63) -band 1
    $exp  = ($bits -shr 52) -band 0x7FF
    $mant = $bits -band 0xFFFFFFFFFFFFF
    '{0}({1}.{2:x3}.{3:x13})' -f $v.ToString('G6', [Globalization.CultureInfo]::InvariantCulture), $sign, $exp, $mant
}

$x0     = D 0x40384CCCCCCCCCCD   # 24.3     linker Rand
$areaY  = D 0x3FEF9BE02468ACF0   # 0.987778 oberer Rand
$areaH  = D 0x4025BB2A1907F6E6   # 10.8656  Hoehe Bearbeitungsbereich
$tapeW  = D 0x4027B4E81B4E81B5   # 11.8533  Bandbreite
$unitX  = (D 0x40523E6911111114) / 342   # Editor-Einheiten pro Bildpunkt (Breite)
$imgH   = D 0x40162B30F6E0B89E   # 5.54218  Bildhoehe (32 Punkte)

# $tapeLength: '1,<wert>' = automatisch, '2,<wert>' = fest
# $images: je Bild Path, Width, Height, Rect (x, y, w, h als double)
function New-Lbt($path, $images, $tapeLength, [double]$editW) {
    $d = { param($v) Format-PtDouble $v }
    $t = New-Object System.Text.StringBuilder
    $lines = @(
        '[Propaty]', 'Version=1.00', 'DoubleFormat=IEEE754(1.11.52)', 'DeviceName=Brother PT-P700',
        'SeriesCode=0', 'ModelCode=g',
        "TapeLength=$tapeLength",
        "TapeWidth=259,$(& $d $tapeW)",
        'TapeMargin=2', 'TapeDirect=1', 'FitPrintArea=1', 'InkColor=0', 'PaperColor=16777215',
        "EditMargin=$(& $d $x0),$(& $d $areaY),$(& $d $editW),$(& $d $areaH)",
        'MultiObjectMode=1', "ObjectCount=$($images.Count)"
    )
    foreach ($l in $lines) { [void]$t.Append("$l`r`n") }
    $names = @()
    foreach ($img in $images) {
        $name = ('{0:x16}.png' -f [BitConverter]::ToInt64([Guid]::NewGuid().ToByteArray(), 0)).Substring(0, 20)
        $names += $name
        $rect = ($img.Rect | ForEach-Object { & $d $_ }) -join ','
        [void]$t.Append("`r`n[Block]`r`nType=Image`r`nRect=$rect`r`nAspect=$(& $d ($img.Width / $img.Height))`r`n")
        [void]$t.Append("Paragraph=0`r`nParagraphRect=$rect`r`nDirect=1`r`nRotate=0`r`nCenterRect=$rect`r`nImageName=$name`r`n")
    }
    [byte[]]$text = [byte[]](0xFF, 0xFE) + [Text.Encoding]::Unicode.GetBytes($t.ToString())

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $ms
    $bw.Write([byte[]][char[]]'PLL'); $bw.Write([byte]2); $bw.Write([uint16]16)
    $bw.Write([uint32]$text.Length); $bw.Write([byte[]]::new(6)); $bw.Write($text)
    for ($k = 0; $k -lt $images.Count; $k++) {
        $png = [IO.File]::ReadAllBytes($images[$k].Path)
        $nameBytes = [Text.Encoding]::Unicode.GetBytes($names[$k] + [char]0)
        $bw.Write([byte[]][char[]]'PLI'); $bw.Write([byte]0)
        $bw.Write([uint16](28 + $nameBytes.Length)); $bw.Write([uint16]28)
        $bw.Write([uint32]$png.Length); $bw.Write([uint32]0)
        $bw.Write([uint32]$images[$k].Width); $bw.Write([uint32]$images[$k].Height); $bw.Write([uint32]24)
        $bw.Write($nameBytes); $bw.Write($png)
    }
    $bw.Flush()
    [IO.File]::WriteAllBytes($path, $ms.ToArray())
    "{0}: {1} Bild(er), TapeLength {2}" -f (Split-Path $path -Leaf), $images.Count, $tapeLength
}

function Get-ImgInfo($bmpResult, $file, $rect) {
    $bm = $bmpResult[-1]
    [pscustomobject]@{ Path = (Join-Path $dir $file); Width = $bm.Width; Height = $bm.Height; Rect = $rect }
}

# WROVER: Masse 1:1 aus der Referenzdatei, nur die Bilder sind neu
New-Lbt (Join-Path $dir 'Pinout-ESP32-WROVER.lbt') @(
    (Get-ImgInfo $a 'etikette_links.png'  @($x0, $areaY, (D 0x40523E6911111114), $imgH)),
    (Get-ImgInfo $b 'etikette_rechts.png' @($x0, (D 0x401A90E6E8B49B77), (D 0x40523EAB33333333), (D 0x4026647F6E5D4C3C)))
) '2,101(0.405.9400000000000)' (D 0x40532CCCCCCCCCCD)

# MAX98357A: gleicher Massstab wie die obere WROVER-Etikette
$wMax = $c[-1].Width * $unitX
New-Lbt (Join-Path $dir 'Pinout-MAX98357A.lbt') @(
    (Get-ImgInfo $c 'etikette_max98357a.png' @($x0, $areaY, $wMax, $imgH))
) "1,$(Format-PtDouble ($x0 + $wMax))" $wMax
