//**************************************************************************
//
// Extension.cpp
//
// Main export functions to be a debugger extension.  These exports are the standard
// DbgEng export functions for an extension DLL and hook up the functionality necessary
// to handle a new file format -- our "text dump" file format.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "TextDump.h"

#define KDEXT_64BIT
#include <dbgeng.h>

using namespace Microsoft::WRL;
using namespace Debugger::TargetComposition::Services::TextDump;
using namespace Debugger::TargetComposition::FileActivators;

Debugger::TargetComposition::FileActivators::TextDumpActivator *g_pActivator = nullptr;
IDebugTargetComposition *g_pCompositionManager = nullptr;
IDebugTargetCompositionBridge *g_pCompositionBridge = nullptr;

//*************************************************
// Utility
//

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

HRESULT DiagnosticLog(_In_opt_ ISvcDiagnosticLogging *pDiagnosticLogging,
                      _In_ DiagnosticLogLevel level,
                      _In_ DiagnosticLogLevel setLevel,
                      _In_opt_ PCWSTR pwszComponent,
                      _In_opt_ PCWSTR pwszCategory,
                      _In_ _Printf_format_string_ PCWSTR pwszMessage,
                      ...)
{
    if (pDiagnosticLogging == nullptr || level < setLevel)
    {
        return S_FALSE;
    }

    HRESULT hr = E_FAIL;

    va_list vl;
    va_start(vl, pwszMessage);

    wchar_t msg[1024];
    if (vswprintf_s(msg, ARRAYSIZE(msg), pwszMessage, vl) > 0)
    {
        hr = pDiagnosticLogging->Log(level, msg, pwszComponent, pwszCategory);
    }

    va_end(vl);

    return hr;
}

} // TextDump
} // Services
} // TargetComposition
} // Debugger

//**************************************************************************
// Classic DbgEng Style Initialization:
//
// - Register (or unregister) an activator for text dump files
//

extern "C"
HRESULT CALLBACK DebugExtensionInitialize(PULONG /*pVersion*/, PULONG /*pFlags*/)
{
    HRESULT hr = S_OK;

    if (g_pActivator != nullptr)
    {
        return E_UNEXPECTED;
    }

    //
    // Several key interfaces for the target composition model can be accessed from a "bridge" interface
    // (IDebugTargetCompositionBridge) which can be found from a standard IDebugClient*.
    //
    // We need to get these and then register an *activator* which handles a particular file format.  
    // We will indicate that we handle files with a .txt extension.  This means we will get an 
    // opportunity to handle ANY file with a .txt extension that is opened as a post-mortem dump target.  
    // Our activator must do a "format check" and indicate whether or not the file format is the one that we handle.  
    //
    // While multiple activators can register handling a file by extension, only a *single* activator can
    // indicate that it wants to handle the file format.
    //
    ComPtr<IDebugClient> spClient;
    IfFailedReturn(DebugCreate(__uuidof(IDebugClient), (void **)&spClient));

    ComPtr<IDebugTargetCompositionBridge> spCompositionBridge;
    IfFailedReturn(spClient.As(&spCompositionBridge));

    ComPtr<IDebugTargetComposition> spCompositionManager;
    IfFailedReturn(spCompositionBridge->GetCompositionManager(&spCompositionManager));

    ComPtr<TextDumpActivator> spActivator;
    IfFailedReturn(MakeAndInitialize<TextDumpActivator>(&spActivator, spCompositionManager.Get()));

    //
    // NOTE: It is incredibly important that how we register here matches how we specified the trigger
    //       in the extension's manifest.  Our manifest indicates:
    //
    //       <IdentifyTargetTrigger FileExtension="txt" />
    //
    //       Therefore, we must call RegisterFileActivatorForExtension with "txt"!
    //
    IfFailedReturn(spCompositionBridge->RegisterFileActivatorForExtension(L"txt", spActivator.Get()));

    g_pActivator = spActivator.Get();
    g_pCompositionBridge = spCompositionBridge.Detach();
    g_pCompositionManager = spCompositionManager.Detach();

    return hr;
}

extern "C"
HRESULT CALLBACK DebugExtensionCanUnload(void)
{
    //
    // We can successfully unload if there are *NO OBJECTS* left.  When we uninitialize, we will unregister
    // our activator and release any global references.  Note that just because the activator will no longer
    // open *NEW FILES*, that does *NOT* mean that there isn't still a file open using this extension.
    // We cannot successfully unload if *ANY* objects are still alive.
    //
    // Only if there are zero objects left do we say this is okay.
    //
    auto objCount = Microsoft::WRL::Module<InProc>::GetModule().GetObjectCount();
    return (objCount == 0) ? S_OK : S_FALSE;
}

extern "C"
void CALLBACK DebugExtensionUninitialize(void)
{
    if (g_pCompositionManager != nullptr)
    {
        g_pCompositionManager->Release();
        g_pCompositionManager = nullptr;
    }

    if (g_pActivator != nullptr)
    {
        (void)g_pCompositionBridge->UnregisterFileActivatorForExtension(L"txt", g_pActivator);
        g_pActivator = nullptr;
    }

    if (g_pCompositionBridge != nullptr)
    {
        g_pCompositionBridge->Release();
        g_pCompositionBridge = nullptr;
    }
}

extern "C"
void CALLBACK DebugExtensionUnload(void)
{
}

