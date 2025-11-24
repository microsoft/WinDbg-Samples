:: Script to build all TTD samples

@echo off
setlocal enabledelayedexpansion

set _script_dir=%~dp0

goto :SkipUsage

:Usage

echo Usage: build.cmd [^<option^>^|^<msbuild arg^>]...
echo.
echo ^<option^> can be one of:
echo      -all                    Build all configurations (Debug and Release) for all platforms (x86, x64, ARM64).
echo                              Default is to build only x64 Release.
echo      -clean                  Clean all build outputs instead of building.
echo      -out ^<path^>             Specify an output directory for all built binaries.
echo.
echo ^<msbuild arg^> can be any legal msbuild argument. These will be passed directly to msbuild.

goto :eof

:SkipUsage
call :ParseArgs %*
if %PrintUsage%==yes goto :Usage
goto :ArgsParsed

:ParseArgs
set PrintUsage=no
set Configs=Release
set Platforms=x64
set AdditionalArguments=
set MsbuildArgs=

:GatherArgs

if           [%1]==[]                   ( rem Done gathering args
) else if /I "%~1"=="-?"                ( set PrintUsage=yes&                                     shift /1 & goto :GatherArgs
) else if /I "%~1"=="-all"              ( set Configs=Debug Release& set Platforms=x86 x64 ARM64& shift /1 & goto :GatherArgs
) else if /I "%~1"=="-clean"            ( set MsbuildArgs=%MsbuildArgs% -t:Clean&                 shift /1 & goto :GatherArgs
) else if /I "%~1"=="-out"              ( set MsbuildArgs=%MsbuildArgs% -p:OutDir=%2&             shift /1 & shift /1 & goto :GatherArgs
) else (
    set AdditionalArguments=%AdditionalArguments% %1
    shift /1
    goto :GatherArgs
)

goto :eof
:ArgsParsed

:: Capture remaining arguments as msbuild arguments
set MsbuildArgs=%MsbuildArgs% %AdditionalArguments%

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

:: Exit the script
goto :eof

:: Build a target
:Build
echo Building %1
pushd %1
nuget restore
for %%F in (*.sln) do (

  for %%C in (%Configs%) do (
    for %%P in (%Platforms%) do (
      echo *** msbuild %%F -p:Configuration=%%C;Platform=%%P %MsbuildArgs%
      msbuild %%F -p:Configuration=%%C;Platform=%%P %MsbuildArgs%
      if errorlevel 1 set _error=1&goto :eof
    )
  )
)
popd
goto :eof
