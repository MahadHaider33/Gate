# Gate's original geometric brand mark. Build-time asset generation only.
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
$images = @()
foreach ($size in @(16,32,48,256)) {
    $bitmap = [System.Drawing.Bitmap]::new($size,$size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([System.Drawing.Color]::FromArgb(255,22,28,33))
    $pen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255,40,137,255),[single]($size*.073))
    $pen.StartCap = $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $heights = @(.18,.48,.72,.44,.18)
    for ($i=0; $i -lt 5; $i++) {
        $x = [single]($size*(.2+.15*$i)); $half = [single]($size*$heights[$i]/2)
        $graphics.DrawLine($pen,$x,[single]($size/2-$half),$x,[single]($size/2+$half))
    }
    $stream = [System.IO.MemoryStream]::new()
    $bitmap.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
    $images += ,@($size,$stream.ToArray())
    $pen.Dispose();$graphics.Dispose();$bitmap.Dispose();$stream.Dispose()
}
$file = [System.IO.File]::Create((Join-Path $root 'resources/Gate.ico'))
$writer = [System.IO.BinaryWriter]::new($file)
$writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$images.Count)
$offset = 6+16*$images.Count
foreach($entry in $images) {
    $s = if($entry[0] -eq 256){0}else{$entry[0]}
    $writer.Write([byte]$s);$writer.Write([byte]$s);$writer.Write([uint16]0)
    $writer.Write([uint16]1);$writer.Write([uint16]32)
    $writer.Write([uint32]$entry[1].Length);$writer.Write([uint32]$offset)
    $offset += $entry[1].Length
}
foreach($entry in $images){$writer.Write([byte[]]$entry[1])}
$writer.Dispose();$file.Dispose()
