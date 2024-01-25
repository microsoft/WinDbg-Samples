# TTD live recorder API sample

This is a sample demonstrating how a program can use TTD's live recording API to record portions of itself.

## Getting Started

A copy of TTD's recorder binaries is needed in order to build and run this sample.
There are general instructions describing how to obtain the binaries documented in [How to download and install the TTD.exe command line utility (Offline method)](https://learn.microsoft.com/windows-hardware/drivers/debuggercmds/time-travel-debugging-ttd-exe-command-line-util#how-to-download-and-install-the-ttdexe-command-line-utility-offline-method)

For the purposes of this sample, you can just run the provided script `.\Get-Ttd` from a command or powershell window.
This script downloads the most current binaries into a TTDDownload subdirectory.

After that is done, open LiveRecorderApiSample.sln in Visual Studio 2022 16.8 or newer and build an appropriate configuration with F5. The sample builds and works for x64, x86 and ARM64.

## Dependencies

Apart from TTD's binaries, this sample requires the [`Microsoft.TimeTravelDebugging.Apis` NuGet package](https://www.nuget.org/packages/Microsoft.TimeTravelDebugging.Apis) that provides the API.
This is downloaded automatically from the feed at https://nuget.org if it's configured in Visual Studio.


The API documentation can be found here: https://github.com/microsoft/WinDbg-Samples/tree/HEAD/TTD/docs

## Running the sample

When the sample is run, it will ask for the administrator pin or password because TTD requires admin privileges in order to record.
Note that the sample itself will not run as admin, only TTD's command-line tool TTD.exe as invoked by the sample.

After the admin request is granted the sample will run and record several portions of itself. The output will look like this:

```
Trace file: X:\repos\Microsoft\WinDbg-Samples\TTD\LiveRecorderApiSample\out\x64-Debug\LiveRecorderApiSample01.run
All done!
```

This provides the full path to the trace file that was recorded. This trace file can then be loaded into [WinDbg](https://aka.ms/WinDbg), where it will show like this:

```
...
Time Travel Position: 42:0
TTDRecordCPU!InitializeNirvanaClient+0x7c69:
       00007ffe`629f5b39 488b4df8        mov     rcx,qword ptr [rbp-8] ss:00000051`8031d4e8=0000c6709491b298
0:000> k
 # Child-SP          RetAddr               Call Site
00 00000051`8031d490 00007ffe`f8981404     TTDRecordCPU!InitializeNirvanaClient+0x7c69
01 00000051`8031d500 00007ff6`3f8c9923     TTDLiveRecorder+0x1404
02 00000051`8031d540 00007ff6`3f8eae95     LiveRecorderApiSample!TTD::ILiveRecorder::StartRecordingCurrentThread+0x63 [X:\repos\Microsoft\WinDbg-Samples\TTD\LiveRecorderApiSample\packages\Microsoft.TimeTravelDebugging.Apis.0.8.0-TTDLiveRecordingBringup.1\include\TTD\TTDLiveRecorder.h @ 184] 
03 00000051`8031d660 00007ff6`3f902609     LiveRecorderApiSample!wmain+0x6c5 [X:\repos\Microsoft\WinDbg-Samples\TTD\LiveRecorderApiSample\main.cpp @ 147] 
...
```
