// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// TtdExtension - A simple debugger extension that demonstrates how to access TTD Replay API interfaces
//                for the loaded trace file.
// 
// To use it:
// 1. Build the project.
// 2. Start WinDbg and open a trace file.
// 3. Load the extension in the command window: .load <path>\<to>\TtdExtension.dll
// 4. Run the extension: !info

#include <Windows.h>

// The TTD Replay headers include a replaceable assert macro that can be used to check conditions during replay. If
// one is not defined, it defaults to no assertion checks. This sample will use the standard assert from the C runtime.
#include <assert.h>
#define DBG_ASSERT(cond) assert(cond)

#include <TTD/IReplayEngineStl.h>
#include <exception>
#include <stdexcept>

// Make the sample code easier to read by bringing the TTD namespaces into scope.
using namespace TTD;
using namespace Replay;

//
// Define KDEXT_64BIT to make all wdbgexts APIs recognize 64 bit addresses
// It is recommended for extensions to use 64 bit headers from wdbgexts so
// the extensions could support 64 bit targets.
//
#define KDEXT_64BIT
#include <DbgEng.h>
#include <WDBGEXTS.H>

WINDBG_EXTENSION_APIS ExtensionApis;

// Helper to query the debugger client for a specific interface
template < typename Interface >
inline Interface* QueryInterfaceByIoctl()
{
    WDBGEXTS_QUERY_INTERFACE wqi = {};
    wqi.Iid = &__uuidof(Interface);
    auto const ioctlSuccess = Ioctl(IG_QUERY_TARGET_INTERFACE, &wqi, sizeof(wqi));
    if (!ioctlSuccess)
    {
        throw std::invalid_argument("Unable to get interface.");
    }
    if (wqi.Iface == nullptr)
    {
        throw std::invalid_argument("Unable to get interface. Query succeeded, but interface was NULL.");
    }
    return static_cast<Interface*>(wqi.Iface);
}

// info command - Demonstrates how to obtain TTD Replay API interfaces from the debugger client
extern "C" HRESULT CALLBACK info(_Inout_ IDebugClient* const /* pClient */, _In_z_ char const* const /* pArgs */) noexcept
try
{
    // Get the current replay cursor in use by the debugger. Use this to read information
    // such as the current position, but do not modify the cursor. If you want to make modifications,
    // such as setting watchpoints or moving the cursor position, get the replay engine
    // interface and create a new cursor using that interface.
    auto const pDebuggerCursor = QueryInterfaceByIoctl<ICursorView>();
    Position const currentPosition = pDebuggerCursor->GetPosition();
    dprintf("Current position: %llX:%llX\n", currentPosition.Sequence, currentPosition.Steps);

    // Get the replay engine in use by the debugger. Use this to read information or to
    // create cursors for analyzing the trace file.
    auto const pEngine = QueryInterfaceByIoctl<IReplayEngineView>();
    SystemInfo const& systemInfo = pEngine->GetSystemInfo();
    dprintf("PID: %X\n", systemInfo.ProcessId);

    // Create a cursor and perform an operation on it (in this case get the program counter). Note
    // that changing the cursor position does not affect the debugger's current position.
    UniqueCursor pQueryCursor(pEngine->NewCursor());
    Position interestingPosition(SequenceId(0x100), StepCount(0x2));
    pQueryCursor->SetPosition(interestingPosition);
    dprintf("PC at %llX:%llx is %p\n", interestingPosition.Sequence, interestingPosition.Steps,
        pQueryCursor->GetProgramCounter());

    return S_OK;
}
catch (...)
{
    return E_UNEXPECTED;
}

// help command
extern "C" HRESULT CALLBACK help(_Inout_ IDebugClient* const /* pClient */, _In_z_ char const* const /* pArgs */) noexcept
try
{
    dprintf("!info - Print information about the trace file using the TTD Replay API\n");

    return S_OK;
}
catch (...)
{
    return E_UNEXPECTED;
}

// Standard initialization code for debugger extension
extern "C" HRESULT CALLBACK DebugExtensionInitialize(_Out_ ULONG* pVersion, _Out_ ULONG* pFlags) noexcept
try
{
    *pVersion = DEBUG_EXTENSION_VERSION(1, 0);
    *pFlags = 0;

    IDebugClient* pDebugClient = nullptr;
    HRESULT hr = DebugCreate(IID_PPV_ARGS(&pDebugClient));
    if (SUCCEEDED(hr))
    {
        IDebugControl* pDebugControl = nullptr;
        hr = pDebugClient->QueryInterface(IID_PPV_ARGS(&pDebugControl));
        if (SUCCEEDED(hr))
        {
            // Get the windbg-style extension APIS
            // Used by a bunch of macros in wdbgexts.h, including dprintf.
            ExtensionApis.nSize = sizeof(ExtensionApis);
            hr = pDebugControl->GetWindbgExtensionApis64(&ExtensionApis);

            pDebugControl->Release();
        }

        pDebugClient->Release();
    }

    return hr;
}
catch (...)
{
    return E_UNEXPECTED;
}

// Standard uninitialization code for debugger extension
extern "C" void CALLBACK DebugExtensionUninitialize() noexcept
try
{
}
catch (...)
{
}
