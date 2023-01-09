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

HRESULT SymbolBuilderManager::TrackProcessForKey(_In_ bool isKernel,
                                                 _In_ ULONG64 processKey,
                                                 _COM_Outptr_ SymbolBuilderProcess **ppProcess)
{
    HRESULT hr = S_OK;
    *ppProcess = nullptr;

    ComPtr<SymbolBuilderProcess> spProcess;

    auto it = m_trackedProcesses.find(processKey);
    if (it == m_trackedProcesses.end())
    {
        IfFailedReturn(MakeAndInitialize<SymbolBuilderProcess>(&spProcess, isKernel, processKey, this));

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

HRESULT SymbolBuilderManager::TrackProcessForModule(_In_ bool isKernel,
                                                    _In_ ISvcModule *pModule, 
                                                    _COM_Outptr_ SymbolBuilderProcess **ppProcess)
{
    HRESULT hr = S_OK;
    *ppProcess = nullptr;

    ULONG64 processKey;
    IfFailedReturn(pModule->GetContainingProcessKey(&processKey));

    ComPtr<SymbolBuilderProcess> spProcess;
    IfFailedReturn(TrackProcessForKey(isKernel, processKey, &spProcess));

    *ppProcess = spProcess.Detach();
    return hr;
}

HRESULT SymbolBuilderManager::TrackProcess(_In_ bool isKernel,
                                           _In_ ISvcProcess *pProcess, 
                                           _COM_Outptr_ SymbolBuilderProcess **ppProcess)
{
    HRESULT hr = S_OK;
    *ppProcess = nullptr;

    if (pProcess == nullptr && !isKernel)
    {
        return E_INVALIDARG;
    }

    ULONG64 processKey = 0;
    if (pProcess != nullptr)
    {
        IfFailedReturn(pProcess->GetKey(&processKey));
    }

    ComPtr<SymbolBuilderProcess> spProcess;
    IfFailedReturn(TrackProcessForKey(isKernel, processKey, &spProcess));

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

HRESULT SymbolBuilderManager::InitArchBased()
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        m_regInfosById.clear();
        m_regIds.clear();

        ComPtr<ISvcRegisterEnumerator> spRegEnum;
        IfFailedReturn(m_spArchInfo->EnumerateRegisters(static_cast<SvcContextFlags>(
                                                            SvcContextCategorizationMask |
                                                            SvcContextSubRegister),
                                                        &spRegEnum));

        for(;;)
        {
            ComPtr<ISvcRegisterInformation> spRegInfo;
            HRESULT hrReg = spRegEnum->GetNext(&spRegInfo);
            if (FAILED(hrReg))
            {
                break;
            }

            BSTR regName;
            IfFailedReturn(spRegInfo->GetName(&regName));
            bstr_ptr spRegName(regName);

            ULONG regId = spRegInfo->GetId();
            ULONG regSize = spRegInfo->GetSize();

            m_regInfosById.insert( { regId, { regName, regId, regSize } } );
            m_regIds.insert( { regName, regId } );
        }

        return hr;
    };
    return ConvertException(fn);
}

bool SymbolBuilderManager::ParseHex(_In_ wchar_t const *pc, 
                                    _Out_ wchar_t const **ppn, 
                                    _Out_ ULONG64 *pValue)
{
    ULONG64 value = 0;

    wchar_t const *pn = pc;
    for(;;)
    {
        if (*pn >= L'0' && *pn <= L'9')
        {
            value = (value << 4) | (*pn - L'0');
        }
        else if (*pn >= L'a' && *pn <= 'f')
        {
            value = (value << 4) | (*pn - L'a' + 10);
        }
        else if (*pn >= L'A' && *pn <= 'F')
        {
            value = (value << 4) | (*pn - L'A' + 10);
        }
        else
        {
            break;
        }
        ++pn;
    }

    *ppn = pn;
    *pValue = value;
    return (pn != pc);
}

bool SymbolBuilderManager::ParseReg(_In_ wchar_t const *pc, 
                                    _Out_ wchar_t const **ppn, 
                                    _Out_ RegisterInformation **ppReg)
{
    *ppReg = nullptr;

    wchar_t const *pn = pc;
    if (*pc != '@')
    {
        return false;
    }

    pn++;
    while (*pn && (*pn == L'_' || iswalnum(*pn))) { ++pn; }

    if (pn - pc <= 1)
    {
        return false;
    }

    std::wstring regName;
    if (FAILED(ConvertException([&]()
    {
        regName = std::wstring(pc + 1, pn - (pc + 1));
        return S_OK;
    })))
    {
        return false;
    }

    *ppn = pn;
    return SUCCEEDED(FindInformationForRegister(regName.c_str(), ppReg));
}

HRESULT SymbolBuilderManager::LocationToString(_In_ SvcSymbolLocation const *pLocation,
                                               _Out_ std::wstring *pString)
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;
        std::wstring str;
        wchar_t buf[64];

        switch(pLocation->Kind)
        {
            case SvcSymbolLocationRegister:
            case SvcSymbolLocationRegisterRelative:
            case SvcSymbolLocationRegisterRelativeIndirectOffset:
            {
                str = L"";

                RegisterInformation *pReg;
                IfFailedReturn(FindInformationForRegisterById(pLocation->RegInfo.Number, &pReg));

                if (pLocation->Kind != SvcSymbolLocationRegister)
                {
                    str += L"[";
                    str += L"@";
                    str += pReg->Name;

                    ULONG64 preOffset;
                    if (pLocation->Kind == SvcSymbolLocationRegisterRelative)
                    {
                        preOffset = pLocation->Offset;
                    }
                    else
                    {
                        preOffset = static_cast<ULONG64>(static_cast<LONG64>(static_cast<LONG>(pLocation->Offsets.Pre)));
                    }

                    if (preOffset != 0)
                    {
                        LONG64 preOffsetSigned = static_cast<LONG64>(preOffset);
                        if (preOffsetSigned < 0)
                        {
                            swprintf_s(buf, ARRAYSIZE(buf), L" - %I64x", -preOffsetSigned);
                        }
                        else
                        {
                            swprintf_s(buf, ARRAYSIZE(buf), L" + %I64x", preOffsetSigned);
                        }

                        str += buf;
                    }

                    str += L"]";

                    if (pLocation->Kind == SvcSymbolLocationRegisterRelativeIndirectOffset)
                    {
                        LONG postOffsetSigned = static_cast<LONG>(pLocation->Offsets.Post);
                        if (postOffsetSigned != 0)
                        {
                            if (postOffsetSigned < 0)
                            {
                                swprintf_s(buf, ARRAYSIZE(buf), L" - %lx", -postOffsetSigned);
                            }
                            else
                            {
                                swprintf_s(buf, ARRAYSIZE(buf), L" + %lx", postOffsetSigned);
                            }
                        }

                        str += buf;
                    }

                    *pString = std::move(str);
                    return S_OK;
                }
                else
                {
                    str += L"@";
                    str += pReg->Name;
                    *pString = std::move(str);
                    return S_OK;
                }
            }

            case SvcSymbolLocationVirtualAddress:
            {
                swprintf_s(buf, ARRAYSIZE(buf), L"%I64x", pLocation->Offset);
                *pString = buf;
                return S_OK;
            }

            default:
                return E_INVALIDARG;
        }
    };
    return ConvertException(fn);
}

HRESULT SymbolBuilderManager::ParseLocation(_In_ PCWSTR pwszLocation,
                                            _Out_ SvcSymbolLocation *pLocation)
{
    //
    // There are a number of forms we accept for a location.
    //
    //     1) A straight hex value  (e.g.: 7ff23ac)
    //     2) A register with @ as a prefix (e.g.: @rcx)
    //     3) A memory dereference with [] (e.g.: [@rbp]
    //     4) A memory dereference with basic + / - math against a hex number (e.g.: [@rbp + 8])
    //     5) A memory dereference with basic + / - math after the reference (e.g.: [@rbp + 8] + 1c)
    //

    SvcSymbolLocation loc{ };
    loc.Kind = SvcSymbolLocationNone;

    RegisterInformation *pReg;
    ULONG64 value;

    wchar_t const *pc = pwszLocation;
    while (*pc && iswspace(*pc)) { ++pc; }

    if (*pc == '[')
    {
        ++pc;
        while (*pc && iswspace(*pc)) { ++pc; }
        if (*pc == '@')
        {
            if (!ParseReg(pc, &pc, &pReg))
            {
                return E_INVALIDARG;
            }

            loc.Kind = SvcSymbolLocationRegisterRelative;
            loc.RegInfo.Number = pReg->Id;
            loc.RegInfo.Size = pReg->Size;

            while (*pc && iswspace(*pc)) { ++pc; }
            if (*pc == L'+' || *pc == L'-')
            {
                bool neg = (*pc == L'-');
                ++pc;
                while (*pc && iswspace(*pc)) { ++pc; }

                if (!ParseHex(pc, &pc, &value))
                {
                    return E_INVALIDARG;
                }

                if (neg)
                {
                    loc.Offset = static_cast<ULONG64>(-static_cast<LONG64>(value));
                }
                else
                {
                    loc.Offset = value;
                }

                while (*pc && iswspace(*pc)) { ++pc; }
            }
            else
            {
                loc.Offset = 0;
            }
        }
        else
        {
            //
            // We do not support location being in in a memory location.
            //
            return E_INVALIDARG;
        }

        if (*pc != L']')
        {
            return E_INVALIDARG;
        }

        ++pc;
        while (*pc && iswspace(*pc)) { ++pc; }

        if (*pc == L'+' || *pc == L'-')
        {
            bool neg = (*pc == L'-');
            ++pc;
            while (*pc && iswspace(*pc)) { ++pc; }

            if (!ParseHex(pc, &pc, &value))
            {
                return E_INVALIDARG;
            }

            loc.Kind = SvcSymbolLocationRegisterRelativeIndirectOffset;
            ULONG64 preOffset = loc.Offset;

            loc.Offsets.Pre = static_cast<ULONG>(preOffset);
            if (neg)
            {
                loc.Offsets.Post = static_cast<ULONG>(static_cast<LONG>(-static_cast<LONG64>(value)));
            }
            else
            {
                loc.Offsets.Post = static_cast<ULONG>(value);
            }
        }
    }
    else if (*pc == '@')
    {
        if (!ParseReg(pc, &pc, &pReg))
        {
            return E_INVALIDARG;
        }

        loc.Kind = SvcSymbolLocationRegister;
        loc.RegInfo.Number = pReg->Id;
        loc.RegInfo.Size = pReg->Size;
    }
    else
    {
        if (!ParseHex(pc, &pc, &value))
        {
            return E_INVALIDARG;
        }

        loc.Kind = SvcSymbolLocationVirtualAddress;
        loc.Offset = value;
    }

    while (*pc && iswspace(*pc)) { ++pc; }
    if (*pc)
    {
        return E_INVALIDARG;
    }

    *pLocation = loc;
    return S_OK;
    
}

HRESULT SymbolBuilderManager::FindInformationForRegister(_In_ PCWSTR pwszRegisterName,
                                                         _Out_ RegisterInformation **ppRegisterInfo)
{
    auto fn = [&]()
    {
        std::wstring regName(pwszRegisterName);
        auto it = m_regIds.find(regName);
        if (it == m_regIds.end())
        {
            return E_FAIL;
        }

        ULONG regId = it->second;
        auto itr = m_regInfosById.find(regId);
        if (itr == m_regInfosById.end())
        {
            return E_FAIL;
        }

        *ppRegisterInfo = &(itr->second);
        return S_OK;
    };
    return ConvertException(fn);
}

HRESULT SymbolBuilderManager::FindInformationForRegisterById(_In_ ULONG id,
                                                             _Out_ RegisterInformation **ppRegisterInfo)
{
    auto itr = m_regInfosById.find(id);
    if (itr == m_regInfosById.end())
    {
        return E_FAIL;
    }

    *ppRegisterInfo = &(itr->second);
    return S_OK;
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

