$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$lock = Get-Content (Join-Path $root 'dependencies.lock.json') -Raw | ConvertFrom-Json
foreach ($name in @('rnnoise', 'speexdsp', 'q', 'infra')) {
    $relative = if ($name -eq 'infra') { 'third_party/q/infra' } else { "third_party/$name" }
    $path = Join-Path $root $relative
    $entry = $lock.$name
    if (!(Test-Path (Join-Path $path '.git'))) {
        New-Item -ItemType Directory -Force -Path $path | Out-Null
        & git -C $path init --quiet
        & git -C $path remote add origin $entry.url
        & git -C $path fetch --depth 1 origin $entry.commit
        if ($LASTEXITCODE) { throw "Could not fetch $name" }
        & git -C $path checkout --detach $entry.commit
    }
    $actual = & git -C $path rev-parse HEAD
    if ($actual -ne $entry.commit) { throw "$name revision mismatch. Expected $($entry.commit), got $actual. No local files were overwritten." }
}
$archive = Join-Path $root 'third_party/rnnoise/model.tar.gz'
if (!(Test-Path $archive)) { Invoke-WebRequest $lock.model.url -OutFile $archive }
if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $lock.model.sha256) { throw 'RNNoise model checksum mismatch' }
& tar -xzf $archive -C (Join-Path $root 'third_party/rnnoise') src/rnnoise_data.c src/rnnoise_data.h
if ($LASTEXITCODE) { throw 'Model extraction failed' }
Write-Host 'Pinned source dependencies and model are ready. Nothing is downloaded at app runtime.'
