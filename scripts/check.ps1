param([string]$BuildDir = 'build')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root $BuildDir
$bundle = Join-Path $build 'VST3/Release/BAT-transpose.vst3'
ctest --test-dir $build --output-on-failure
if ($LASTEXITCODE) { throw 'CTest failed' }
$validator = Join-Path $build 'bin/validator.exe'
if (-not (Test-Path -LiteralPath $validator)) { throw 'Build validator with scripts/build.ps1 -Target validator -SkipTests' }
& $validator $bundle
if ($LASTEXITCODE) { throw 'VST3 validation failed' }
& (Join-Path $build 'bat_host.exe') $bundle --editor-test
if ($LASTEXITCODE) { throw 'Editor smoke test failed' }
