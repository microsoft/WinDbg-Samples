# Interface TTD::ILiveRecorder

Defined in [TTD/TTDLiveRecorder.h](README.md).

This interface allows a TTD recording client to interact and interfere with the recorder
in order to control and enhance the recording.

## Overview

Obtaining the interface is described in the header file's [General overview](README.md#general-overview) section.

A working sample that demonstrates the use of this interface can be found on
[GitHub](https://github.com/microsoft/WinDbg-Samples/tree/HEAD/TTD/LiveRecorderApiSample).

This interface derives from the standard [IUnknown](https://learn.microsoft.com/windows/win32/api/unknwn/nn-unknwn-iunknown)
interface to manage object lifetime and access any other interfaces supported by the object, but it doesn't participate
in the COM registeration and activation infrastructure offered by Windows nor does it use HRESULT for error reporting.
Smart pointer facilities like [`WRL::ComPtr`](https://learn.microsoft.com/cpp/cppcx/wrl/comptr-class),
[`winrt::com_ptr`](https://learn.microsoft.com/uwp/cpp-ref-for-winrt/com-ptr) or
[`wil::com_ptr`](https://github.com/microsoft/wil/wiki/WinRT-and-COM-wrappers)
may be used to ensure lifetime is handled correctly.

## Factory function

- [`TTDMakeLiveRecorder`](function-TTDMakeLiveRecorder.md)

## Methods

### Object and lifetime management

These methods control the access and lifetime of the object that provides the interface.

- [`AddRef`](https://learn.microsoft.com/windows/win32/api/unknwn/nf-unknwn-iunknown-addref) (IUnknown)
- [`Release`](https://learn.microsoft.com/windows/win32/api/unknwn/nf-unknwn-iunknown-release) (IUnknown)
- [`QueryInterface`](https://learn.microsoft.com/windows/win32/api/unknwn/nf-unknwn-iunknown-queryinterface%28refiid_void%29) (IUnknown)

### Information

These methods provide information as requested by the caller.

- [`GetFileName`](ILiveRecorder_GetFileName.md)
- [`GetState`](ILiveRecorder_GetState.md)
- [`GetThrottleState`](ILiveRecorder_GetThrottleState.md)
- [`IsOpen`](ILiveRecorder_IsOpen.md)

### Controlling the recorder

These methods are used to control the recorder and provide ways to explicitly turn it on or off on the calling thread.

- [`Close`](ILiveRecorder_Close.md)
- [`ResetThrottle`](ILiveRecorder_ResetThrottle.md)
- [`ResumeRecording`](ILiveRecorder_ResumeRecording.md)
- [`StartRecordingCurrentThread`](ILiveRecorder_StartRecordingCurrentThread.md)
- [`StopRecordingCurrentThread`](ILiveRecorder_StopRecordingCurrentThread.md)
- [`TryPauseRecording`](ILiveRecorder_TryPauseRecording.md)

### Adding information to the trace

These methods work whether the calling thread is being recorded or not.
They are used to explicitly add data to the trace file.

- [`AddCustomEvent`](ILiveRecorder_AddCustomEvent.md)
- [`DumpHeaps`](ILiveRecorder_DumpHeaps.md)
- [`DumpModuleData`](ILiveRecorder_DumpModuleData.md)
- [`DumpSnapshot`](ILiveRecorder_DumpSnapshot.md)

## Types

Additional types scoped and defined inside of the interface class.

- [`ScopedPauseRecording`](ILiveRecorder_ScopedPauseRecording.md)
