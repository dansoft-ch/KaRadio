param([string]$In, [string]$OutSvg, [string]$OutPng, [double]$Tol = 0.6, [double]$MmPerPx = 0.1, [double]$MinArea = 1500, [double]$CornerDeg = 40)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System; using System.Collections.Generic; using System.Drawing; using System.Drawing.Drawing2D; using System.Text; using System.Globalization;
public static class Tracer {
  static double[,] F; static int W, H;
  static PointF EdgePt(long key) {
    int k = (int)(key / 2); bool vert = (key % 2) == 1; int x = k % W, y = k / W;
    double a = F[x, y], b = vert ? F[x, y + 1] : F[x + 1, y];
    double t = a / (a - b);
    return vert ? new PointF(x, (float)(y + t)) : new PointF((float)(x + t), y);
  }
  public static List<List<PointF>> Contours(Bitmap bmp) {
    W = bmp.Width + 2; H = bmp.Height + 2; F = new double[W, H];
    for (int x = 0; x < W; x++) for (int y = 0; y < H; y++) {
      if (x == 0 || y == 0 || x == W - 1 || y == H - 1) { F[x, y] = -75; continue; }
      Color c = bmp.GetPixel(x - 1, y - 1);
      double v = c.R - (c.G + c.B) / 2.0 - 75; if (v == 0) v = 0.01; F[x, y] = v;
    }
    var adj = new Dictionary<long, List<long>>();
    Action<long, long> link = (p, q) => {
      if (!adj.ContainsKey(p)) adj[p] = new List<long>(); if (!adj.ContainsKey(q)) adj[q] = new List<long>();
      adj[p].Add(q); adj[q].Add(p); };
    for (int x = 0; x < W - 1; x++) for (int y = 0; y < H - 1; y++) {
      long top = 2L * (y * W + x), bottom = 2L * ((y + 1) * W + x), left = 2L * (y * W + x) + 1, right = 2L * (y * W + x + 1) + 1;
      int c = (F[x, y] > 0 ? 1 : 0) | (F[x + 1, y] > 0 ? 2 : 0) | (F[x + 1, y + 1] > 0 ? 4 : 0) | (F[x, y + 1] > 0 ? 8 : 0);
      switch (c) {
        case 1: case 14: link(left, top); break;
        case 2: case 13: link(top, right); break;
        case 4: case 11: link(right, bottom); break;
        case 8: case 7: link(bottom, left); break;
        case 3: case 12: link(left, right); break;
        case 6: case 9: link(top, bottom); break;
        case 5: link(left, top); link(right, bottom); break;
        case 10: link(top, right); link(bottom, left); break;
      }
    }
    var seen = new HashSet<long>(); var res = new List<List<PointF>>();
    foreach (var start in adj.Keys) {
      if (seen.Contains(start)) continue;
      var loop = new List<PointF>(); long prev = -1, cur = start;
      while (true) {
        seen.Add(cur); var p = EdgePt(cur); loop.Add(new PointF(p.X - 1, p.Y - 1));
        long nxt = adj[cur][0] == prev ? adj[cur][1] : adj[cur][0];
        if (adj[cur].Count > 2) { foreach (var n in adj[cur]) if (n != prev && !seen.Contains(n)) { nxt = n; break; } }
        prev = cur; cur = nxt; if (cur == start || seen.Contains(cur)) break;
      }
      res.Add(loop);
    }
    return res;
  }
  public static double Area(List<PointF> p) { double a = 0; for (int i = 0; i < p.Count; i++) { var q = p[i]; var r = p[(i + 1) % p.Count]; a += q.X * r.Y - r.X * q.Y; } return a / 2; }
  public static List<PointF> Smooth(List<PointF> p) {
    var r = new List<PointF>(); int n = p.Count;
    for (int i = 0; i < n; i++) { var a = p[(i - 1 + n) % n]; var b = p[i]; var c = p[(i + 1) % n]; r.Add(new PointF((a.X + 2 * b.X + c.X) / 4, (a.Y + 2 * b.Y + c.Y) / 4)); }
    return r;
  }
  static double SegDist(PointF p, PointF a, PointF b) {
    double dx = b.X - a.X, dy = b.Y - a.Y, l = dx * dx + dy * dy; if (l == 0) return Math.Sqrt((p.X - a.X) * (p.X - a.X) + (p.Y - a.Y) * (p.Y - a.Y));
    double t = Math.Max(0, Math.Min(1, ((p.X - a.X) * dx + (p.Y - a.Y) * dy) / l)); double ex = a.X + t * dx - p.X, ey = a.Y + t * dy - p.Y; return Math.Sqrt(ex * ex + ey * ey);
  }
  static void DP(List<PointF> p, int i, int j, double tol, List<int> keep) {
    double md = 0; int mi = -1; for (int k = i + 1; k < j; k++) { double d = SegDist(p[k], p[i], p[j]); if (d > md) { md = d; mi = k; } }
    if (md > tol) { DP(p, i, mi, tol, keep); keep.Add(mi); DP(p, mi, j, tol, keep); }
  }
  public static List<PointF> Simplify(List<PointF> p, double tol) {
    // split closed loop at the two mutually farthest-ish points
    int n = p.Count, far = 0; double md = 0;
    for (int k = 0; k < n; k++) { double d = (p[k].X - p[0].X) * (p[k].X - p[0].X) + (p[k].Y - p[0].Y) * (p[k].Y - p[0].Y); if (d > md) { md = d; far = k; } }
    var ring = new List<PointF>(p); ring.Add(p[0]);
    var keep = new List<int> { 0 }; DP(ring, 0, far, tol, keep); keep.Add(far); DP(ring, far, n, tol, keep);
    var r = new List<PointF>(); foreach (var k in keep) r.Add(ring[k]); return r;
  }
  static PointF U(PointF a, PointF b) { double dx = b.X - a.X, dy = b.Y - a.Y, l = Math.Sqrt(dx * dx + dy * dy); return l == 0 ? new PointF(0, 0) : new PointF((float)(dx / l), (float)(dy / l)); }
  static double Len(PointF a, PointF b) { return Math.Sqrt((b.X - a.X) * (b.X - a.X) + (b.Y - a.Y) * (b.Y - a.Y)); }
  // returns list of cubic segments: p0,c1,c2,p1
  public static List<PointF[]> Bez(List<PointF> p, double cornerDeg) {
    int n = p.Count; var tin = new PointF[n]; var tout = new PointF[n];
    for (int i = 0; i < n; i++) {
      var a = p[(i - 1 + n) % n]; var b = p[i]; var c = p[(i + 1) % n];
      var u1 = U(a, b); var u2 = U(b, c); double ang = Math.Acos(Math.Max(-1, Math.Min(1, u1.X * u2.X + u1.Y * u2.Y))) * 180 / Math.PI;
      if (ang > cornerDeg) { tin[i] = u1; tout[i] = u2; }
      else { double l1 = Len(a, b), l2 = Len(b, c); var t = U(new PointF(0, 0), new PointF((float)(u1.X * l1 + u2.X * l2), (float)(u1.Y * l1 + u2.Y * l2))); tin[i] = t; tout[i] = t; }
    }
    var segs = new List<PointF[]>();
    for (int i = 0; i < n; i++) {
      var a = p[i]; var b = p[(i + 1) % n]; double l = Len(a, b) / 3;
      segs.Add(new[] { a, new PointF((float)(a.X + tout[i].X * l), (float)(a.Y + tout[i].Y * l)), new PointF((float)(b.X - tin[(i + 1) % n].X * l), (float)(b.Y - tin[(i + 1) % n].Y * l)), b });
    }
    return segs;
  }
}
"@
$bmp = [System.Drawing.Bitmap]::FromFile($In)
$loops = [Tracer]::Contours($bmp)
$kept = @(); foreach ($l in $loops) { $a = [Math]::Abs([Tracer]::Area($l)); if ($a -ge $MinArea) { $kept += ,$l; "Kontur: $($l.Count) Punkte, Flaeche $([int]$a) px2" } else { "verworfen: Flaeche $([int]$a)" } }
$ci = [Globalization.CultureInfo]::InvariantCulture
$minX = 1e9; $minY = 1e9; $maxX = -1e9; $maxY = -1e9
$allSegs = @()
foreach ($l in $kept) {
  $s = [Tracer]::Simplify([Tracer]::Smooth([Tracer]::Smooth($l)), $Tol)
  "  vereinfacht: $($s.Count) Knoten"
  $segs = [Tracer]::Bez($s, $CornerDeg); $allSegs += ,$segs
  foreach ($q in $s) { $minX = [Math]::Min($minX, $q.X); $minY = [Math]::Min($minY, $q.Y); $maxX = [Math]::Max($maxX, $q.X); $maxY = [Math]::Max($maxY, $q.Y) }
}
$m = $MmPerPx
function f($v) { $v.ToString("0.###", $ci) }
$d = New-Object System.Text.StringBuilder
foreach ($segs in $allSegs) {
  [void]$d.Append("M " + (f (($segs[0][0].X - $minX) * $m)) + " " + (f (($segs[0][0].Y - $minY) * $m)) + " ")
  foreach ($sg in $segs) { [void]$d.Append("C"); foreach ($k in 1..3) { [void]$d.Append(" " + (f (($sg[$k].X - $minX) * $m)) + " " + (f (($sg[$k].Y - $minY) * $m))) }; [void]$d.Append(" ") }
  [void]$d.Append("Z ")
}
$wmm = f (($maxX - $minX) * $m); $hmm = f (($maxY - $minY) * $m)
$svg = @"
<?xml version="1.0" encoding="UTF-8"?>
<!-- Armbrust-Logo fuer die Rueckwand, automatisch aus dem Originalbild nachgefahren.
     1 Einheit = 1 mm, Logo $wmm x $hmm mm (in Fusion beliebig skalierbar).
     Aussenkontur plus zwei Loecher zwischen Bogen und Sehne. In Fusion: Einfuegen > SVG einfuegen,
     dann die Logo-Flaeche (ohne die beiden Loecher) waehlen und extrudieren. -->
<svg xmlns="http://www.w3.org/2000/svg" width="${wmm}mm" height="${hmm}mm" viewBox="0 0 $wmm $hmm">
  <path fill="#000" fill-rule="evenodd" d="$($d.ToString().Trim())"/>
</svg>
"@
[IO.File]::WriteAllText($OutSvg, $svg)
# Overlay
$s = 2; $out = New-Object System.Drawing.Bitmap ($bmp.Width * $s), ($bmp.Height * $s)
$g = [System.Drawing.Graphics]::FromImage($out); $g.DrawImage($bmp, 0, 0, $bmp.Width * $s, $bmp.Height * $s)
$g.ScaleTransform($s, $s); $g.SmoothingMode = 'AntiAlias'
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Blue), 0.6
foreach ($segs in $allSegs) { $gp = New-Object System.Drawing.Drawing2D.GraphicsPath; foreach ($sg in $segs) { $gp.AddBezier($sg[0], $sg[1], $sg[2], $sg[3]) }; $gp.CloseFigure(); $g.DrawPath($pen, $gp) }
$out.Save($OutPng)
"SVG: $wmm x $hmm mm"
