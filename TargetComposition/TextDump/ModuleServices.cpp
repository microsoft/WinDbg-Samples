//**************************************************************************
//
// ModuleServices.cpp
//
// Target composition services to provide module information to the debugger
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

HRESULT Module::GetName(_Out_ BSTR *pModuleName)
{
    //
    // We want the name of the file on disk (sans path).  While our format does save the original
    // name that the debugger assigned, this *IS NOT* what this method is asking for. 
    //
    wchar_t const* pFinalSep = wcsrchr(m_pModuleInfo->ModulePath.c_str(), L'\\');
    if (pFinalSep)
    {
        *pModuleName = SysAllocString(pFinalSep + 1);
    }
    else
    {
        *pModuleName = SysAllocString(m_pModuleInfo->ModulePath.c_str());
    }

    return (*pModuleName == nullptr ? E_OUTOFMEMORY : S_OK);
}

HRESULT Module::GetPath(_Out_ BSTR *pModulePath)
{
    *pModulePath = SysAllocString(m_pModuleInfo->ModulePath.c_str());
    return (*pModulePath == nullptr ? E_OUTOFMEMORY : S_OK);
}

HRESULT ModuleEnumerator::GetNext(_COM_Outptr_ ISvcModule **ppTargetModule)
{
    HRESULT hr = S_OK;
    *ppTargetModule = nullptr;

    //
    // E_BOUNDS indicates end of iteration.
    //
    auto&& moduleInfos = m_spParsedFile->GetModuleInformations();
    if (m_pos >= moduleInfos.size())
    {
        m_spModuleService->CompleteModuleEnumeration();
        return E_BOUNDS;
    }

    ModuleInformation const* pModuleInfo = &(moduleInfos[m_pos]);
    ++m_pos;

    ComPtr<Module> spModule;
    IfFailedReturn(MakeAndInitialize<Module>(&spModule, m_spModuleService.Get(), m_spParsedFile, pModuleInfo));

    *ppTargetModule = spModule.Detach();
    return hr;
}

HRESULT ModuleEnumerationService::FindModule(_In_opt_ ISvcProcess * /*pProcess*/,
                                             _In_ ULONG64 moduleKey,
                                             _COM_Outptr_ ISvcModule **ppTargetModule)
{
    HRESULT hr = S_OK;
    *ppTargetModule = nullptr;

    ComPtr<Module> spModule;

    //
    // Note that because we represent only a single process in our "text dump" format, we do not need
    // to go look at what process the debugger is asking about.  If this were a kernel target, pProcess would
    // be nullptr to indicate the set of modules loaded in the kernel (or in the "shared address mapping")
    //

    auto&& moduleInfos = m_spParsedFile->GetModuleInformations();
    for (auto&& moduleInfo : moduleInfos)
    {
        if (moduleInfo.StartAddress == moduleKey)
        {
            IfFailedReturn(MakeAndInitialize<Module>(&spModule, this, m_spParsedFile, &moduleInfo));
            *ppTargetModule = spModule.Detach();
            return S_OK;
        }
    }

    return E_BOUNDS;
}

HRESULT ModuleEnumerationService::FindModuleAtAddress(_In_opt_ ISvcProcess * /*pProcess*/,
                                                      _In_ ULONG64 moduleAddress,
                                                      _COM_Outptr_ ISvcModule **ppTargetModule)
{
    HRESULT hr = S_OK;
    *ppTargetModule = nullptr;

    ComPtr<Module> spModule;

    //
    // Note that because we represent only a single process in our "text dump" format, we do not need
    // to go look at what process the debugger is asking about.  If this were a kernel target, pProcess would
    // be nullptr to indicate the set of modules loaded in the kernel (or in the "shared address mapping")
    //

    auto&& moduleInfos = m_spParsedFile->GetModuleInformations();
    for (auto&& moduleInfo : moduleInfos)
    {
        if (moduleAddress >= moduleInfo.StartAddress && moduleAddress < moduleInfo.EndAddress)
        {
            IfFailedReturn(MakeAndInitialize<Module>(&spModule, this, m_spParsedFile, &moduleInfo));
            *ppTargetModule = spModule.Detach();
            return S_OK;
        }
    }

    return E_BOUNDS;
}

HRESULT ModuleEnumerationService::EnumerateModules(_In_opt_ ISvcProcess * /*pProcess*/,
                                                   _COM_Outptr_ ISvcModuleEnumerator **ppEnum)
{
    HRESULT hr = S_OK;
    *ppEnum = nullptr;

    //
    // Note that because we represent only a single process in our "text dump" format, we do not need
    // to go look at what process the debugger is asking about.  If this were a kernel target, pProcess would
    // be nullptr to indicate the set of modules loaded in the kernel (or in the "shared address mapping")
    //

    ComPtr<ModuleEnumerator> spEnum;
    IfFailedReturn(MakeAndInitialize<ModuleEnumerator>(&spEnum, this, m_spParsedFile));

    *ppEnum = spEnum.Detach();
    return hr;
}

void ModuleEnumerationService::CompleteModuleEnumeration()
{
    if (!m_firstEnumerationComplete)
    {
        m_firstEnumerationComplete = true;

        //
        // The first time a module enumeration is complete, fire an event into the service container.  Our
        // virtual memory service will listen to this and suquently modify the service container to *STACK*
        // an image backed virtual memory service on top of itself.
        //
        // NOTE: This is done as a performance optimization only (with the current state of the debugger).  
        //       It is perfectly legal to insert this service when the container spins up.  The problem here is that
        //       the debugger tends to try to read image headers of modules from the VA space when it starts up.
        //       If the image backed VM service happens to be present during this early phase, it will go to
        //       the symbol server and pull a binary.
        //
        //       We *DO NOT* want the startup of our "text dump" in the debugger to query the symbol server
        //       *IMMEDIATELY* for *EVERY* module upon startup.  We "defer" allowing image VA mapping until after
        //       the debugger has queried all the modules specifically for this reason.
        //
        //       It may be the case that some of this will be unnecessary in the future.
        //
        if (m_pServiceManager)
        {
            m_pServiceManager->FireEventNotification(DEBUG_TEXTDUMPEVENT_MODULEENUMERATIONCOMPLETE, nullptr, nullptr);
        }
    }
}

HRESULT ModuleIndexService::GetModuleIndexKey(_In_ ISvcModule *pModule,
                                              _Out_ BSTR *pModuleIndex,
                                              _Out_ GUID *pModuleIndexKind)
{
    HRESULT hr = S_OK;

    ULONG64 baseAddress;
    IfFailedReturn(pModule->GetBaseAddress(&baseAddress));

    auto&& moduleInfos = m_spParsedFile->GetModuleInformations();
    for(auto&& moduleInfo : moduleInfos)
    {
        if (moduleInfo.StartAddress == baseAddress)
        {
            //
            // The symbol server key for a PE is <time date stamp> padded (zero prefix) to 8 bytes
            // follwed by the <size of image> (from PE headers) not padded at all.
            //
            wchar_t buf[32];
            swprintf_s(buf, ARRAYSIZE(buf), L"%08X%x", (ULONG)moduleInfo.TimeStamp, (ULONG)moduleInfo.ImageSize);

            *pModuleIndex = SysAllocString(buf);
            *pModuleIndexKind = DEBUG_MODULEINDEXKEY_TIMESTAMP_IMAGESIZE;
            return (*pModuleIndex == nullptr ? E_OUTOFMEMORY : S_OK);
        }
    }

    return E_FAIL;
}

} // TextDump
} // Services
} // TargetComposition
} // Debugger

