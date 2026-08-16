$ErrorActionPreference = "Stop"

$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
$cmake = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninja = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $root "build"
$outBmp = Join-Path $root "main_screen.bmp"

if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }
if (-not (Test-Path $cmake)) { throw "cmake.exe not found: $cmake" }

New-Item -ItemType Directory -Force -Path $build | Out-Null

$cmd = @"
call `"$vcvars`" && `"$cmake`" -G Ninja -S `"$root`" -B `"$build`" -DCMAKE_MAKE_PROGRAM=`"$ninja`" && `"$cmake`" --build `"$build`" && `"$build\hmi_pc_sim.exe`" `"$outBmp`"
"@

cmd /c $cmd
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "BMP: $outBmp"
