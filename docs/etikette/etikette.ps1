# Erzeugt Etiketten (6 mm Band, Brother P-touch) mit dem Pinout des ESP32-WROVER.
# Raster 2,54 mm = 18 Punkte bei 180 dpi (Aufloesung der P-touch-Drucker).
# Ausgabe: etikette_links.png, etikette_rechts.png (Druckdaten, 180 dpi)
#          vorschau.png (8-fach vergroessert, nur zum Anschauen)
# Aufruf:  powershell -ExecutionPolicy Bypass -File docs\etikette\etikette.ps1
#
# Reihenfolge: Pin 1 = Antennen-Ende (3V3 bzw. GND), letzter Pin = USB-Ende.
# Belegt = schwarz hinterlegt (weisse Schrift). Kuerzel wie im Factsheet:
#   Tst1..Tst6 = Sendertasten (GPIO 13, 14, 27, 15, 5, 2)
#   Pvol = Lautstaerke-Poti (39), Pmte = Mute-Poti (36)
#   LRC/BCLK/DIN = MAX98357A (25/26/22), IR = IR-Empfaenger (21), LED = Status-LED (4)

Add-Type -AssemblyName System.Drawing

# Text, belegt (true = schwarz hinterlegt)
$links = @(
    @('3V3', $false), @('EN', $false), @('Pmte', $true), @('Pvol', $true),
    @('34', $false), @('35', $false), @('32', $false), @('33', $false),
    @('LRC', $true), @('BCLK', $true), @('Tst3', $true), @('Tst2', $true),
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
$font['V'] = '##..##','##..##','##..##','##..##','##..##','##..##','##..##','##..##','.####.','.####.','..##..'
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

# Vorschau: beide Etiketten 8-fach vergroessert untereinander
$s = 8
$pv = New-Object System.Drawing.Bitmap ($a[-1].Width * $s + 40), ($height * $s * 2 + 60)
$g = [System.Drawing.Graphics]::FromImage($pv)
$g.Clear([System.Drawing.Color]::LightGray)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$g.DrawImage($a[-1], 20, 20, $a[-1].Width * $s, $height * $s)
$g.DrawImage($b[-1], 20, 40 + $height * $s, $b[-1].Width * $s, $height * $s)
$g.Dispose()
$pv.Save((Join-Path $dir 'vorschau.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$a[0..($a.Count - 2)]; $b[0..($b.Count - 2)]
