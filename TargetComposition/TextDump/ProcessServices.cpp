//**************************************************************************
//
// ProcessServices.cpp
//
// Target composition services to provide process information to the debugger
// from our "text dump" file format.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "TextDump.h"

using namespace Microsoft::WRL;

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

HRESULT ProcessEnumerationService::FindProcess(_In_ ULONG64 processKey,
                                               _COM_Outptr_ ISvcProcess **ppTargetProcess)
{
    HRESULT hr = S_OK;
    *ppTargetProcess = nullptr;

    //
    // This is a request to find a process by its key (which may or may not be the PID).  As we only have
    // a single process, we fake a key/id of "1".
    //
    if (processKey != 1)
    {
        return E_BOUNDS;
    }

    ComPtr<Process> spProcess;
    IfFailedReturn(MakeAndInitialize<Process>(&spProcess, m_spParsedFile));

    *ppTargetProcess = spProcess.Detach();
    return hr;
}

HRESULT ProcessEnumerationService::EnumerateProcesses(_COM_Outptr_ ISvcProcessEnumerator **ppEnum)
{
    HRESULT hr = S_OK;
    *ppEnum = nullptr;

    ComPtr<ProcessEnumerator> spEnum;
    IfFailedReturn(MakeAndInitialize<ProcessEnumerator>(&spEnum, m_spParsedFile));

    *ppEnum = spEnum.Detach();
    return hr;
}

} // TextDump
} // Services
} // TargetComposition
} // Debugger

