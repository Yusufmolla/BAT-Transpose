param([string]$BuildDir = 'build', [switch]$DspOnly, [switch]$SkipTests, [string]$Target = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Install Visual Studio 2022 Build Tools with Desktop development with C++' }
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$build = Join-Path $root $BuildDir
$plugin = if ($DspOnly) { 'OFF' } else { 'ON' }
cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DBAT_BUILD_PLUGIN=$plugin"
if ($LASTEXITCODE) { throw 'Configure failed' }
$buildArgs = @('--build', $build, '--parallel', '6')
if ($Target) { $buildArgs += @('--target', $Target) }
cmake @buildArgs
if ($LASTEXITCODE) { throw 'Build failed' }
if (-not $SkipTests) {
    ctest --test-dir $build --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed' }
}
