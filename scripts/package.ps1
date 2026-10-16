param([string]$BuildDir = 'build')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root $BuildDir
cpack --config (Join-Path $build 'CPackConfig.cmake') -C Release -B (Join-Path $root 'dist')
if ($LASTEXITCODE) { throw 'Packaging failed' }
