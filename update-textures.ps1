$pal = "assets\not-64-palette.hex"
$ptool = "utilities\PaletteTools"
$outdir = "assets\indexed"
$n = 0

if (!(Test-Path $outdir)) { mkdir $outdir | Out-Null }

# Gather all source PNGs
$sources = @()
$sources += Get-ChildItem "assets\*.png" | Where-Object { $_.Name -ne "palette_lut.png" }
if (Test-Path "assets\textures") {
    $sources += Get-ChildItem "assets\textures\*.png"
}

foreach ($f in $sources) {
    $idx = Join-Path $outdir "$($f.BaseName).idx"
    $png = Join-Path $outdir "$($f.BaseName).png"

    $needsUpdate = $false
    if (!(Test-Path $idx)) {
        $needsUpdate = $true
    } elseif ($f.LastWriteTime -gt (Get-Item $idx).LastWriteTime) {
        $needsUpdate = $true
    }

    if ($needsUpdate) {
        Write-Host "  [IDX] $($f.Name)"
        & dotnet run --project $ptool -- convert $f.FullName $pal $png --dither --idx $idx 2>&1 | Out-Null
        $n++
    }
}

if ($n -eq 0) {
    Write-Host "[INDEX] All indexed textures up to date."
} else {
    Write-Host "[INDEX] Updated $n indexed texture(s)."
}
