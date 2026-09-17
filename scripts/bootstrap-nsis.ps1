$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$archive = Join-Path $root '.tools/nsis-3.11.zip'
$expected = 'c7d27f780ddb6cffb4730138cd1591e841f4b7edb155856901cdf5f214394fa1'
New-Item -ItemType Directory -Force (Join-Path $root '.tools') | Out-Null
if (!(Test-Path $archive) -or (Get-FileHash $archive).Hash.ToLowerInvariant() -ne $expected) {
    Invoke-WebRequest 'https://pilotfiber.dl.sourceforge.net/project/nsis/NSIS%203/3.11/nsis-3.11.zip?viasf=1' -UserAgent 'curl/8.12.1' -OutFile $archive
}
if ((Get-FileHash $archive).Hash.ToLowerInvariant() -ne $expected) { throw 'NSIS archive checksum mismatch' }
Expand-Archive $archive (Join-Path $root '.tools') -Force
