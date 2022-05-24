//**************************************************************************
//
// ThreadServices.cpp
//
// Target composition services to provide thread information to the debugger
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

HRESULT Thread::GetContext(_In_ SvcContextFlags /*contextFlags*/,
                           _Out_ ISvcRegisterContext **ppRegisterContext)
{
    HRESULT hr = S_OK;
    *ppRegisterContext = nullptr;

    ISvcMachineArchitecture *pMachineArch = m_spThreadService->GetMachineArch();

    //
    // If there was no register context saved in the "text dump" just return E_NOTIMPL indicating that
    // we do not have any register context available.
    //
    if (!m_spParsedFile->HasRegisters() || pMachineArch == nullptr)
    {
        return E_NOTIMPL;
    }

    //
    // We do not want to implement our own "register context" structure.  It's entirely possible to do that;
    // however, it is far easier (and more typical) to go ask the architecture service to just give us one.
    //
    ComPtr<ISvcRegisterContext> spRegisterContext;
    IfFailedReturn(pMachineArch->CreateRegisterContext(&spRegisterContext));

    //
    // NOTE: The caller is only asking for what's in contextFlags (e.g.: maybe only integer registers)
    //       We do not have to fill in anything beyond that.  For sample purposes here, we fill in everything
    //       that we know.
    //    
    std::unordered_map<std::wstring, ULONG> const* pRegisterMappings;
    IfFailedReturn(m_spThreadService->GetRegisterMappings(&pRegisterMappings));

    for (auto&& registerValue : m_spParsedFile->GetRegisters())
    {
        auto it = pRegisterMappings->find(registerValue.Name);
        if (it == pRegisterMappings->end())
        {
            //
            // We don't understand this register...  Just skip it...
            //
        }
        else
        {
            //
            // The value is set by canonical ID (for our architecture, this is a CodeView CV_* constant; however,
            // we already queried the architecture service for IDs and thus did not have to hard code anything.
            //
            IfFailedReturn(spRegisterContext->SetRegisterValue64(it->second, registerValue.Value));
        }
    }

    *ppRegisterContext = spRegisterContext.Detach();
    return hr;
}

HRESULT ThreadEnumerationService::InitializeRegisterMappings()
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        //
        // NOTE: We do this specifically because we allow "<registerName>=<value>" in the "text dump"
        //       format.  Instead of hard coding all the register names, we will query the architecture
        //       service for this information and utilize it.
        //
        //       There are many other ways to get register context.  Register contexts can be set from
        //       a "classic interface" which is, effectively, the Windows _CONTEXT record for the given
        //       architecture.  Registers can be set via their canonical domain IDs (which for the
        //       target composition APIs and known architectures are the CodeView CV_* constants).
        //       There are also "conditional services" which understand other domain register context
        //       records (e.g.: a Linux PRSTATUS record) and can convert such into an ISvcRegisterContext.
        //

        m_registerMappings.clear();
        if (m_spMachineArch != nullptr)
        {
            ComPtr<ISvcRegisterEnumerator> spRegisterEnum;
            IfFailedReturn(m_spMachineArch->EnumerateRegisters(SvcContextCategorizationMask, &spRegisterEnum));

            ComPtr<ISvcRegisterInformation> spRegInfo;
            for(;;)
            {
                //
                // E_BOUNDS would indicate the end.  We cannot, however, recover from other errors (e.g.:
                // memory allocation failure) so just bail out.
                //
                if (FAILED(spRegisterEnum->GetNext(&spRegInfo)))
                {
                    break;
                }

                BSTR bstrRegName;
                IfFailedReturn(spRegInfo->GetName(&bstrRegName));
                bstr_ptr spRegName(bstrRegName);

                std::wstring regName = bstrRegName;

                //
                // Just to be EXTRA cautious about ensuring it's in the format we expect, convert
                // all the names to lower case.
                //
                wchar_t *pc = const_cast<wchar_t *>(regName.data());
                while (*pc)
                {
                    *pc = towlower(*pc);
                    ++pc;
                }

                m_registerMappings.insert( { regName, spRegInfo->GetId() } );
            }
        }

        return hr;
    };

    //
    // We cannot let a C++ exception escape the boundary of our plug-in into the debugger.
    //
    return ConvertException(fn);
}

HRESULT ThreadEnumerationService::FindThread(_In_ ISvcProcess * /*pProcess*/,
                                             _In_ ULONG64 threadKey,
                                             _COM_Outptr_ ISvcThread **ppTargetThread)
{
    HRESULT hr = S_OK;
    *ppTargetThread = nullptr;

    //
    // This is a request to find a thread by its key (which may or may not be the TID) within the context
    // of the given process.  We only have one process, so we do not even need to check pProcess.
    // Since we only have a single thread within that process, we fake a key/id of "1".
    //
    if (threadKey != 1)
    {
        return E_BOUNDS;
    }

    ComPtr<Thread> spThread;
    IfFailedReturn(MakeAndInitialize<Thread>(&spThread, this, m_spParsedFile));

    *ppTargetThread = spThread.Detach();
    return hr;
}

HRESULT ThreadEnumerationService::EnumerateThreads(_In_ ISvcProcess * /*pProcess*/,
                                                   _COM_Outptr_ ISvcThreadEnumerator **ppEnum)
{
    HRESULT hr = S_OK;
    *ppEnum = nullptr;

    //
    // Note that we only have one process.  We do not need to check which process the debugger is talking
    // about via the "pProcess" argument.
    //

    ComPtr<ThreadEnumerator> spEnum;
    IfFailedReturn(MakeAndInitialize<ThreadEnumerator>(&spEnum, this, m_spParsedFile));

    *ppEnum = spEnum.Detach();
    return hr;
}

} // TextDump
} // Services
} // TargetComposition
} // Debugger

