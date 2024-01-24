# TTD live recorder API sample

This is a sample demonstrating how a program can use TTD's live recording API to record portions of itself.

## Getting Started

A copy of TTD's recorder binaries is needed in order to build and run this sample.
There are general instructions describing how to obtain the binaries documented in [How to download and install the TTD.exe command line utility (Offline method)](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/time-travel-debugging-ttd-exe-command-line-util#how-to-download-and-install-the-ttdexe-command-line-utility-offline-method)

For the purposes of this sample, you can just run the provided script `.\Get-Ttd` from a command or powershell window.
This script downloads the most current binaries into a TTDDownload subdirectory.

After that is done, open LiveRecorderApiSample.sln in Visual Studio 2022 16.8 or newer and build an appropriate configuration with F5. The sample builds and works for x64, x86 and ARM64.

## Dependencies

Apart from TTD's binaries, this sample requires the [`Microsoft.TimeTravelDebugging.Apis` NuGet package](https://www.nuget.org/packages/Microsoft.TimeTravelDebugging.Apis) that provides the API.
This is downloaded automatically from the feed at https://nuget.org if it's configured in Visual Studio.


The API documentation can be found here: https://github.com/microsoft/WinDbg-Samples/tree/master/TTD/docs

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

Metadata recorded from the API can be examined in the debugger using the data model. For instance:

```
0:000> dx -r1 @$curprocess.TTD.RecordClients["6DA58208-3BF5-4B80-A711-781098BC4445"]
@$curprocess.TTD.RecordClients["6DA58208-3BF5-4B80-A711-781098BC4445"]                 : Record client with GUID 6DA58208-3BF5-4B80-A711-781098BC4445 and lifetime: [40:0, 12A90:0]
    Lifetime         : [40:0, 12A90:0]
    OpenUserData     : 14 bytes ( 48 65 6C 6C 6F 2C 20 74 68 65 72 65 21 00 )
    CloseUserData    : 7 bytes ( 41 64 69 6F 73 21 00 )
    Activities      
0:000> dx -r1 @$curprocess.TTD.RecordClients["6DA58208-3BF5-4B80-A711-781098BC4445"].Activities
@$curprocess.TTD.RecordClients["6DA58208-3BF5-4B80-A711-781098BC4445"].Activities                
    [0x0]            : Activity 1 with lifetime: [41:0, 4FE:0]
    [0x1]            : Activity 2 with lifetime: [4FF:0, FFFFFFFFFFFFFFFE:0]
0:000> dx -r1 @$curprocess.TTD.RecordClients["6DA58208-3BF5-4B80-A711-781098BC4445"].Activities[1]
@$curprocess.TTD.RecordClients["6DA58208-3BF5-4B80-A711-781098BC4445"].Activities[1]                 : Activity 2 with lifetime: [4FF:0, FFFFFFFFFFFFFFFE:0]
    Id               : 0x2
    Lifetime         : [4FF:0, FFFFFFFFFFFFFFFE:0]
    Islands         
0:000> dx -r1 @$curprocess.TTD.RecordClients["6DA58208-3BF5-4B80-A711-781098BC4445"].Activities[1].Islands
@$curprocess.TTD.RecordClients["6DA58208-3BF5-4B80-A711-781098BC4445"].Activities[1].Islands                
    [0x0]            : Island on activity 2 from client 6DA58208-3BF5-4B80-A711-781098BC4445 on thread 2 (0x16200) with lifetime: [4FF:0, FFFFFFFFFFFFFFFE:0]
    [0x1]            : Island on activity 2 from client 6DA58208-3BF5-4B80-A711-781098BC4445 on thread 3 (0x150D4) with lifetime: [509:0, FFFFFFFFFFFFFFFE:0]
    [0x2]            : Island on activity 2 from client 6DA58208-3BF5-4B80-A711-781098BC4445 on thread 4 (0x10DF0) with lifetime: [516:0, FFFFFFFFFFFFFFFE:0]
...
    [0x61]           : Island on activity 2 from client 6DA58208-3BF5-4B80-A711-781098BC4445 on thread 32 (0x10C3C) with lifetime: [C32A:0, C65E:0]
    [0x62]           : Island on activity 2 from client 6DA58208-3BF5-4B80-A711-781098BC4445 on thread 6 (0xC910) with lifetime: [C6E7:0, CABE:0]
    [0x63]           : Island on activity 2 from client 6DA58208-3BF5-4B80-A711-781098BC4445 on thread 52 (0xFB3C) with lifetime: [C788:0, C9C1:0]
    [...]           
```
