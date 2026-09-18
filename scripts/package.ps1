$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$stage = [IO.Path]::GetFullPath((Join-Path $root 'dist/Gate'))
$archive = Join-Path $root 'dist/Gate-0.1.1-windows-x64.zip'
# Runtime allowlist: never recursively copy a build or dependency folder.
$files = [ordered]@{
    'Gate.exe' = 'build/release/Gate.exe'
    'README.txt' = 'packaging/README.txt'
    'LICENSE' = 'LICENSE'
    'THIRD_PARTY_NOTICES.md' = 'THIRD_PARTY_NOTICES.md'
    'licenses/RNNoise.txt' = 'third_party/rnnoise/COPYING'
    'licenses/SpeexDSP.txt' = 'third_party/speexdsp/COPYING'
    'licenses/Cycfi-Q.txt' = 'third_party/q/LICENSE'
    'licenses/Cycfi-infra.txt' = 'packaging/licenses/Cycfi-infra.txt'
    'licenses/Signalsmith-Stretch.txt' = 'third_party/signalsmith-stretch/LICENSE.txt'
    'licenses/Signalsmith-Linear.txt' = 'third_party/signalsmith-linear/LICENSE.txt'
}
foreach ($source in $files.Values) {
    if (!(Test-Path -LiteralPath (Join-Path $root $source) -PathType Leaf)) { throw "Missing package input: $source" }
}
if (Test-Path -LiteralPath $stage) {
    if ($stage -ne (Join-Path $root 'dist\Gate')) { throw 'Unexpected staging path' }
    $items = @(Get-Item -LiteralPath $stage) + @(Get-ChildItem -LiteralPath $stage -Recurse -Force)
    if ($items | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) { throw 'Refusing to clean staging directory containing links' }
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Path (Join-Path $stage 'licenses') -Force | Out-Null
foreach ($entry in $files.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $root $entry.Value) -Destination (Join-Path $stage $entry.Key)
}
# Retain additional source-level notices required for binary redistribution.
foreach ($dependency in @('rnnoise','speexdsp')) {
    $target = Join-Path $stage ('licenses/' + $(if ($dependency -eq 'rnnoise') {'RNNoise'} else {'SpeexDSP'}) + '.txt')
    $sources = if ($dependency -eq 'rnnoise') {
        Get-ChildItem -LiteralPath (Join-Path $root 'third_party/rnnoise/src') -Recurse -File | Where-Object Extension -In '.c','.h'
    } else { Get-Item (Join-Path $root 'third_party/speexdsp/libspeexdsp/resample.c') }
    $notices = [Collections.Generic.HashSet[string]]::new()
    foreach ($source in $sources) {
        $reader = [IO.File]::OpenText($source.FullName)
        try { $prefix = New-Object char[] 12000; $count = $reader.Read($prefix,0,$prefix.Length); $header = [string]::new($prefix,0,$count) }
        finally { $reader.Dispose() }
        foreach ($match in [regex]::Matches($header,'(?s)/\*.*?\*/')) {
            if ($match.Value -match '(?i)copyright|redistribution and use') { [void]$notices.Add($match.Value) }
        }
    }
    foreach ($notice in $notices) { Add-Content -LiteralPath $target -Value "`n$notice" -Encoding UTF8 }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream = [IO.File]::Open($archive,[IO.FileMode]::Create)
$zip = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($entry in $files.GetEnumerator()) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,(Join-Path $stage $entry.Key),('Gate/' + $entry.Key),[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $zip.Dispose(); $stream.Dispose() }
$bytes = (Get-ChildItem -LiteralPath $stage -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host "Runnable folder: $stage ($bytes bytes)"
Write-Host "ZIP: $archive ($((Get-Item -LiteralPath $archive).Length) bytes)"
