:: Script to build all TTD samples

@echo off
setlocal enabledelayedexpansion

set _script_dir=%~dp0

set _configs=Release
set _platforms=x64
if "%1"=="-all" (
  set _configs=Debug Release
  set _platforms=x86 x64 ARM64
  shift
)

:: Capture remaining arguments as msbuild arguments
set _msbuild_args=%1 %2 %3 %4 %5 %6 %7 %8 %9

:: Make sure necessary tools are available

where /Q nuget.exe
if %errorlevel%== 0 goto :have_nuget
echo nuget.exe must be on command line. Install from https://www.nuget.org/downloads& goto :eof
:have_nuget

where /Q msbuild.exe
if %errorlevel%== 0 goto :have_msbuild
echo msbuild.exe must be on command line. Run this script from a developer prompt&goto :eof
:have_msbuild

:: Make sure the TTD bits are downloaded for both projects

if not exist "%_script_dir%LiveRecorderApiSample\TTDDownload\x64\TTDRecordCPU.dll" (
  pushd "%_script_dir%LiveRecorderApiSample"
  call Get-Ttd.cmd
  if not exist "%_script_dir%LiveRecorderApiSample\TTDDownload\x64\TTDRecordCPU.dll" popd & echo Error: LiveRecorderApiSample Get-Ttd.cmd failed& goto :eof
  popd
)

if not exist "%_script_dir%ReplayApi\GetTtd\TTDDownload\x64\TTDReplay.dll" (
  pushd "%_script_dir%ReplayApi\GetTtd"
  call Get-Ttd.cmd
  if not exist "%_script_dir%ReplayApi\GetTtd\TTDDownload\x64\TTDReplay.dll" popd & echo Error: ReplayApi Get-Ttd.cmd failed& goto :eof
  popd
)

:: Build each sample
set _error=0
for /F "delims=$" %%F in ('dir /s /b /a:-d "%_script_dir%packages.config"') do (
  call :Build "%%~dpF"
  if !_error! NEQ 0 exit /B 1
)

:: Report binaries that are built
echo Binaries produced:
dir /s /b *.exe *.dll *.sys

:: Exit the script
goto :eof

:: Build a target
:Build
echo Building %1
pushd %1
nuget restore
for %%F in (*.sln) do (

  for %%C in (%_configs%) do (
    for %%P in (%_platforms%) do (
      msbuild %%F -p:Configuration=%%C;Platform=%%P %_msbuild_args%
      if errorlevel 1 set _error=1&goto :eof
    )
  )
)
popd
goto :eof
