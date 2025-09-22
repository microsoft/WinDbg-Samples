# TTD/TTDLiveRecorder.h

This header is used by a program that is being recorded by TTD, or a component (DLL) running within a process
that is being recorded by TTD, to interface with the recorder, control what is being recorded and add custom
pieces of data to the trace file that contains the recording.

It resides in the [Microsoft.TimeTravelDebugging.Apis](../Microsoft.TimeTravelDebugging.Apis.md) NuGet package
and interfaces with the TTDLiveRecorder.dll component found in [TTD recorder's binary distribution](https://learn.microsoft.com/windows-hardware/drivers/debuggercmds/time-travel-debugging-ttd-exe-command-line-util#how-to-download-and-install-the-ttdexe-command-line-utility-offline-method).

## Definitions

- **recorder** is TTD's code that resides in a process that is being recorded, and performs the recording.
- **client** or **recording client** is the code that uses this API to interact with the recorder.
- **client GUID** is a GUID value defined by and belonging to the client. This value can be used to later identify
  the various pieces of data that this particular client puts in the recording.
- **island** is a single stretch of recording in a single thread, from when the recording was started
  to when it was stopped.
- **activity** is a group of islands, possibly on multiple threads, that the client marks with a common ID,
  for some purpose known to the client.
- **custom event** is a point of interest in the recording marked by the client, with some data associated with it.
- **throttle** is a voluntary end of the current island of recording, based on some condition indicated by the client.
- **user data** is a raw array of bytes that the client may provide at various points during the recording,
  for instance when opening the API interface, starting islands or recording custom events.
  This data will be included in the recording and it is up to the client to later correctly interpret it.

## General overview

In order to interact with the recorder, a recording client application or component needs to:

1. Be built using C++20 or later.
2. Include the TTD/TTDLiveRecorder.h header.
3. Either link to the TTDLiveRecorder.lib static library or dynamically load the TTDLiveRecorder.dll component.
4. Generate a new GUID to identify the particular client that's being developed.
5. Call the [`TTDMakeLiveRecorder`](function-TTDMakeLiveRecorder.md) function to obtain
   the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface into the recorder.

Minimal self-contained example:

```C++
// 2. Include the API header.
#include <TTD/TTDLiveRecorder.h>

// 3. Link the DLL. Like this or in the project properties or dynamically with LoadLibrary and GetProcAddress.
#pragma comment(lib, "TTDLiveRecorder.lib")

// 4. GUID specific to this client.
// {6DA58208-3BF5-4B80-A711-781098BC4445}
static constexpr GUID ClientGuid = { 0x6da58208, 0x3bf5, 0x4b80, { 0xa7, 0x11, 0x78, 0x10, 0x98, 0xbc, 0x44, 0x45 } };

int main()
{
    // 5. Obtain the interface to the recorder.
    TTD::ILiveRecorder* pRecorder = TTD::TTDMakeLiveRecorder(ClientGuid, nullptr, 0);

    // Verify success obtaining the recorder interface.
    if (pRecorder != nullptr)
    {
        // The interface may be used here.

        // The interface uses IUnknown to manage lifetime, so call Release() when you're done with it.
        pRecorder->Release();
    }
}
```

## Dependencies

- [TTD/TTDCommonTypes.h](../TTDCommonTypes.h/README.md)

## Interfaces

- [`TTD::ILiveRecorder`](interface-ILiveRecorder.md)
