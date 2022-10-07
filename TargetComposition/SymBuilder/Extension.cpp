//**************************************************************************
//
// Extension.cpp
//
// Main export functions to be a debugger extension.  These exports are the standard
// DbgEng export functions for an extension DLL and hook up the functionality necessary
// for us to "extend" symbols.  Note that this extension is an extension at two different
// levels of the debugger:
//
// 1) The target composition (lower level)
//
//    Here -- the extension exposes a new kind of symbol -- one which we construct in memory
//    based on a series of API calls.
//
// 2) The data model (upper level)
//
//     Here -- the extension exposes APIs that allow modification of the symbols provided
//     at the target composition level.
//
// A **GREAT DEAL** of care must be taken to keep a clean division between these two parts.  Data
// model extensions and things at the data model level frequently depend on target composition
// components and services.  Things at the target composition level **CANNOT** depend on anything
// at the data model level without **EXTREME CARE**.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "SymBuilder.h"

//*************************************************
// GUID Defintions for the plug-in
// 

#define INITGUID
#include <guiddef.h>
#include "InternalGuids.h"
#undef INITGUID

#define KDEXT_64BIT
#include <dbgeng.h>

using namespace Microsoft::WRL;
using namespace Debugger::TargetComposition::Services::SymbolBuilder;

IDebugTargetComposition *g_pCompositionManager = nullptr;
IDebugTargetCompositionBridge *g_pCompositionBridge = nullptr;

//*************************************************
// Hooks For Data Model:
//
// These are the *ONLY* things files outside the API / data model portion of
// the extension are allowed to touch!
//

HRESULT InitializeObjectModel(_In_ IUnknown *pHostInterface);
void UninitializeObjectModel();

//*************************************************
// Utility
//

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymBuilder
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
// Here, we simply call to initialize our provider.  Everything else is keyed off API 
// calls from the projected data model objects.
//

extern "C"
HRESULT CALLBACK DebugExtensionInitialize(PULONG /*pVersion*/, PULONG /*pFlags*/)
{
    HRESULT hr = S_OK;

    ComPtr<IDebugClient> spClient;
    IfFailedReturn(DebugCreate(__uuidof(IDebugClient), (void **)&spClient));

    IfFailedReturn(InitializeObjectModel(spClient.Get()));

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
    UninitializeObjectModel();
}

extern "C"
void CALLBACK DebugExtensionUnload(void)
{
}

