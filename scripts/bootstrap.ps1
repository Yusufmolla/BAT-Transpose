$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
function Get-PinnedRepo([string]$Name, [string]$Url, [string]$Commit) {
    $destination = Join-Path $root "external/$Name"
    if (-not (Test-Path -LiteralPath $destination)) {
        git clone $Url $destination
        if ($LASTEXITCODE) { throw "Clone failed: $Name" }
    }
    $actual = git -C $destination rev-parse HEAD
    if ($actual -ne $Commit) {
        if (git -C $destination status --porcelain) { throw "Local changes in $Name; refusing to overwrite" }
        git -C $destination fetch origin $Commit
        if ($LASTEXITCODE) { throw "Fetch failed: $Name" }
        git -C $destination checkout --detach $Commit
        if ($LASTEXITCODE) { throw "Checkout failed: $Name" }
    }
}
Get-PinnedRepo 'vst3sdk' 'https://github.com/steinbergmedia/vst3sdk.git' '9fad9770f2ae8542ab1a548a68c1ad1ac690abe0'
git -C (Join-Path $root 'external/vst3sdk') submodule update --init --depth 1 base cmake pluginterfaces public.sdk vstgui4
if ($LASTEXITCODE) { throw 'SDK submodule setup failed' }
