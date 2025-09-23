:: Script to build all TTD samples

@echo off
setlocal enabledelayedexpansion

:: Capture remaining arguments as msbuild arguments
shift
set _msbuild_args=%*

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

if not exist %~dp0LiveRecorderApiSample\TTDDownload\x64\TTDRecordCPU.dll (
  pushd %~dp0LiveRecorderApiSample
  call Get-Ttd.cmd
  if not exist %~dp0LiveRecorderApiSample\TTDDownload\x64\TTDRecordCPU.dll echo Error: LiveRecorderApiSample Get-Ttd.cmd failed& goto :eof
  popd
)

if not exist %~dp0ReplayApi\GetTtd\TTDDownload\x64\TTDReplay.dll (
  pushd %~dp0ReplayApi\GetTtd
  call Get-Ttd.cmd
  if not exist %~dp0ReplayApi\GetTtd\TTDDownload\x64\TTDReplay.dll echo Error: ReplayApi Get-Ttd.cmd failed& goto :eof
  popd
)

:: Build each sample
set _error=0
for /F %%F in ('dir /s /b /a:-d %~dp0packages.config') do (
  call :Build %%~dpF
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

  :: X86
  msbuild %%F -p:Configuration=Debug;Platform=x86 %_msbuild_args%
  if errorlevel 1 set _error=1&goto :eof

  msbuild %%F -p:Configuration=Release;Platform=x86 %_msbuild_args%
  if errorlevel 1 set _error=1&goto :eof

  :: X64
  msbuild %%F -p:Configuration=Debug;Platform=x64 %_msbuild_args%
  if errorlevel 1 set _error=1&goto :eof

  msbuild %%F -p:Configuration=Release;Platform=x64 %_msbuild_args%
  if errorlevel 1 set _error=1&goto :eof

  :: ARM64
  msbuild %%F -p:Configuration=Debug;Platform=ARM64 %_msbuild_args%
  if errorlevel 1 set _error=1&goto :eof

  msbuild %%F -p:Configuration=Release;Platform=ARM64 %_msbuild_args%
  if errorlevel 1 set _error=1&goto :eof
)
popd
goto :eof
