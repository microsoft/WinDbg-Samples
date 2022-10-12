//**************************************************************************
//
// SymManager.cpp
//
// A management object which keeps track of the symbol sets that have been
// created and which modules they are assigned to.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "SymBuilder.h"

using namespace Microsoft::WRL;

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

IDebugServiceManager* SymbolBuilderProcess::GetServiceManager() const
{
    return m_pOwningManager->GetServiceManager();
}

ISvcMachineArchitecture *SymbolBuilderProcess::GetArchInfo() const
{
    return m_pOwningManager->GetArchInfo();
}

ISvcMemoryAccess *SymbolBuilderProcess::GetVirtualMemory() const
{
    return m_pOwningManager->GetVirtualMemory();
}

HRESULT SymbolBuilderProcess::CreateSymbolsForModule(_In_ ISvcModule *pModule,
                                                     _In_ ULONG64 moduleKey,
                                                     _COM_Outptr_ SymbolSet **ppSymbols)
{
    HRESULT hr = S_OK;
    *ppSymbols = nullptr;

    //
    // We do not allow creating symbols if they already exist.  The caller must have verified depending on
    // what they want to do here!
    //
    auto it = m_symbols.find(moduleKey);
    if (it != m_symbols.end())
    {
        return E_INVALIDARG;
    }

    ComPtr<SymbolSet> spSymbolSet;
    IfFailedReturn(MakeAndInitialize<SymbolSet>(&spSymbolSet, pModule, this));

    //
    // We cannot let a C++ exception escape.
    //
    auto fn = [&]()
    {
        m_symbols.insert( { moduleKey, spSymbolSet } );
        return S_OK;
    };
    IfFailedReturn(ConvertException(fn));

    *ppSymbols = spSymbolSet.Detach();
    return hr;
}

HRESULT SymbolBuilderManager::TrackProcessForKey(_In_ ULONG64 processKey,
                                                 _COM_Outptr_ SymbolBuilderProcess **ppProcess)
{
    HRESULT hr = S_OK;
    *ppProcess = nullptr;

    ComPtr<SymbolBuilderProcess> spProcess;

    auto it = m_trackedProcesses.find(processKey);
    if (it == m_trackedProcesses.end())
    {
        IfFailedReturn(MakeAndInitialize<SymbolBuilderProcess>(&spProcess, processKey, this));

        //
        // We cannot let an exception escape the COM boundary.
        //
        auto fn = [&]()
        {
            m_trackedProcesses.insert( { processKey, spProcess } );
            return S_OK;
        };
        IfFailedReturn(ConvertException(fn));
    }
    else
    {
        spProcess = it->second;
    }

    *ppProcess = spProcess.Detach();
    return hr;
}

HRESULT SymbolBuilderManager::TrackProcessForModule(_In_ ISvcModule *pModule, 
                                                    _COM_Outptr_ SymbolBuilderProcess **ppProcess)
{
    HRESULT hr = S_OK;
    *ppProcess = nullptr;

    ULONG64 processKey;
    IfFailedReturn(pModule->GetContainingProcessKey(&processKey));

    ComPtr<SymbolBuilderProcess> spProcess;
    IfFailedReturn(TrackProcessForKey(processKey, &spProcess));

    *ppProcess = spProcess.Detach();
    return hr;
}

HRESULT SymbolBuilderManager::TrackProcess(_In_ ISvcProcess *pProcess, 
                                           _COM_Outptr_ SymbolBuilderProcess **ppProcess)
{
    HRESULT hr = S_OK;
    *ppProcess = nullptr;

    ULONG64 processKey;
    IfFailedReturn(pProcess->GetKey(&processKey));

    ComPtr<SymbolBuilderProcess> spProcess;
    IfFailedReturn(TrackProcessForKey(processKey, &spProcess));

    *ppProcess = spProcess.Detach();
    return hr;
}

HRESULT SymbolBuilderManager::ProcessKeyToProcess(_In_ ULONG64 processKey,
                                                  _COM_Outptr_ ISvcProcess **ppProcess)
{
    HRESULT hr = S_OK;
    *ppProcess = nullptr;

    if (m_spProcEnum == nullptr)
    {
        return E_FAIL;
    }

    return m_spProcEnum->FindProcess(processKey, ppProcess);
}

HRESULT SymbolBuilderManager::PidToProcess(_In_ ULONG64 pid,
                                           _COM_Outptr_ ISvcProcess **ppProcess)
{
    HRESULT hr = S_OK;
    *ppProcess = nullptr;

    if (m_spProcEnum == nullptr)
    {
        return E_FAIL;
    }

    //
    // The process enumeration service provides a quick "key" -> "object" method.  It does not provide
    // a quick "id" -> "object" method.  They may or may not be the same thing depending on who implements
    // the process enumeration service.
    //
    // We need to take the long route.
    //
    ComPtr<ISvcProcessEnumerator> spProcEnum;
    IfFailedReturn(m_spProcEnum->EnumerateProcesses(&spProcEnum));

    for(;;)
    {
        ComPtr<ISvcProcess> spProcess;
        IfFailedReturn(spProcEnum->GetNext(&spProcess));

        ULONG64 curPid;
        IfFailedReturn(spProcess->GetId(&curPid));
        if (curPid == pid)
        {
            *ppProcess = spProcess.Detach();
            return S_OK;
        }
    }

    return E_BOUNDS;
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

