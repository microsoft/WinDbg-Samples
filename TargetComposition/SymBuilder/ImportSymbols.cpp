//**************************************************************************
//
// ImportSymbols.cpp
//
// The implementation of our notion of importing symbols from another source on demand.  If we
// have not looked at an "import source" for a given query (symbol by offset / by name / general
// enumeration), this will do so and, if there are matches, will effectively import them
// into the symbol builder by copying the symbols/structures as needed.
//
// Note that there are several levels at which a symbol import could potentially work:
//
//     1) The target composition layer (ISvc* APIs)
//     2) The DIA layer (IDia* APIs)
//     3) The DbgHelp layer (Sym* APIs)
//
// *IDEALLY*, the import would be based upon the first (#1) of these and everything would operate
// purely at the target composition level.  Unfortunately, that is not **CURRENTLY** feasible
// for the functionality we want in this sample.  The current symbol architecture of the debugger
// at a high level looks somewhat like this:
//
//                  |--------------xxxxx-------------------------|
//                  | Data Model   xxxxx   Other Parts of DbgEng |
//                  |--------------xxxxx-------------------------|
//                      |                      |
//                      |                      |
//                      |                      v
//                      |      |---------------------|
//                      |      | DbgHelp (Sym* APIs) | (3)
//                      |      |---------------------|
//                      |                 |
//                      |                 |
//                      |                / \
//                      |          ------   ------
//                      |         /               \
//                      |         |               |
//                      v         v               v
//                |------------------|          |----------------------|
//                | DIA (IDia* APIs) | (2)      | PE Parsing (Exports) |
//                |------------------|          |----------------------|
//                          |
//                          |
//                         / \
//                   ------   ------
//                  /               \
//                  |               |
//                  v               v
//        |-----------|           |-------------|
//        |    PDB    |           | DIA Wrapper |
//        |-----------|           |-------------|
//                                       |
//                                       |
//                                       v
//                        |------------------------------|
//                        | ISvcSymbolSet Implementation |  (1)
//                        |------------------------------|
//                                       |
//                                       |
//                                     / | -------------------\
//                                    /  |                     \
//                      --------------   |                      \
//                     /                 |                       \
//                     |                 |                       |
//                     v                 v                       v
//       |---------------|   |-------------|                   |----------------|
//       | DWARF Symbols |   | ELF Exports |     (others...)   | Symbol Builder |
//       |---------------|   |-------------|                   |----------------|
//
// Choosing to import at each of these levels has a consequence (at least of the time of
// authoring of this sample):
//
// 1) (ISvcSymbolSet) This would *NOT* cover PDB and PE export symbols.  These are *ONLY* currently
//    accessible via DIA or DbgHelp.  As the primary use of importing like this is for adding data to
//    "limited symbols" such as public symbols or export symbols, this is a non-starter.
//
// 2) (DIA) Unfortunately, this will *NOT* cover PE export symbols.  Properly done, this *CAN* cover
//    every other type of symbol.  All target composition symbols are wrapped in something that looks
//    like an IDiaSession / IDiaSymbol.  Unfortunately, again, the primary use here is adding data to
//    "limited symbols" and that often includes PE exports.
//
// 3) (DbgHelp) Properly done, importing from DbgHelp *CAN* cover every other type of symbol.  Asking
//    DbgHelp to find symbols will only find PDB/PE, but the debugger points DbgHelp at the "DIA wrapper"
//    for other types of symbols, so it *CAN* cover other types.
//
//    For now, this sample will focus on the "ask DbgHelp" and PDB / PE export scenario only.  This covers
//    the vast majority of the use cases of this sample.
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

HRESULT SymbolImporter_DbgHelp::ConnectToSource()
{
    HRESULT hr = InternalConnectToSource();
    if (FAILED(hr))
    {
        DisconnectFromSource();
    }
    return hr;
}

HRESULT SymbolImporter_DbgHelp::InternalConnectToSource()
{
    HRESULT hr = S_OK;

    m_symInfoBuf.reset(new char[SymInfoBufSize]);
    m_pSymInfo = reinterpret_cast<SYMBOL_INFOW *>(m_symInfoBuf.get());
    m_pSymInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    m_pSymInfo->MaxNameLen = static_cast<ULONG>(MaxNameLen);

    //
    // Create our own session to DbgHelp so we are *NOT* interfering with the debugger's usage of it.
    //
    if (m_symHandle != NULL)
    {
        return E_FAIL;
    }

    m_symHandle = static_cast<HANDLE>(this);
    if (!SymInitialize(m_symHandle, nullptr, FALSE))
    {
        m_symHandle = NULL;
        return HRESULT_FROM_WIN32(GetLastError());
    }

    //
    // Ensure that DbgHelp has the appropriate search path!
    //
    SymSetSearchPathW(m_symHandle, m_searchPath.c_str());

    //
    // Register a callback with DbgHelp so that it can explicitly call us back to read memory
    // from this address space.  Such just gets redirected down the service stack.
    //
    if (!SymRegisterCallbackW64(m_symHandle,
                                (PSYMBOL_REGISTERED_CALLBACK64)&SymbolImporter_DbgHelp::LegacySymbolCallbackBridge,
                                reinterpret_cast<ULONG64>(reinterpret_cast<PVOID>(this))))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    ISvcModule *pModule = m_pOwningSet->GetModule();

    IfFailedReturn(pModule->GetBaseAddress(&m_moduleBase));
    IfFailedReturn(pModule->GetSize(&m_moduleSize));

    IMAGEHLP_MODULEW64 modInfo { };
    modInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULEW64);

    BSTR moduleName;
    IfFailedReturn(pModule->GetName(&moduleName));
    bstr_ptr spModuleName(moduleName);

    BSTR modulePath;
    IfFailedReturn(pModule->GetPath(&modulePath));
    bstr_ptr spModulePath(modulePath);

    //
    // Force immediate load.  This is *NOT* per-call.  This is global.  We need to restore any state
    // changes to preserve behavior in the debugger.
    //
    DWORD symOpt = SymGetOptions();
    SymSetOptions(symOpt & ~SYMOPT_DEFERRED_LOADS);

    if (!SymLoadModuleExW(m_symHandle,
                          NULL,
                          modulePath,
                          moduleName,
                          m_moduleBase,
                          static_cast<ULONG>(m_moduleSize),
                          NULL,
                          0))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        if (SUCCEEDED(hr))
        {
            hr = E_FAIL;
        }

        SymSetOptions(symOpt);
        return hr;
    }

    SymSetOptions(symOpt);

    if (!SymGetModuleInfoW64(m_symHandle, m_moduleBase, &modInfo))
    {
        return E_FAIL;
    }

    return S_OK;
}

void SymbolImporter_DbgHelp::DisconnectFromSource()
{
    if (m_symHandle != NULL)
    {
        SymCleanup(m_symHandle);
        m_symHandle = NULL;
    }
}

bool SymbolImporter_DbgHelp::LegacyReadMemory(_Inout_ IMAGEHLP_CBA_READ_MEMORY *pReadMemory)
{
    auto pOwningProcess = m_pOwningSet->GetOwningProcess();
    auto pVirtualMemory = pOwningProcess->GetVirtualMemory();
    ULONG64 processKey = pOwningProcess->GetProcessKey();

    ComPtr<ISvcProcess> spProcess;
    ComPtr<ISvcAddressContext> spAddrCtx;
    if (FAILED(pOwningProcess->GetSymbolBuilderManager()->ProcessKeyToProcess(processKey, &spProcess)) ||
        FAILED(spProcess.As(&spAddrCtx)))
    {
        return false;
    }

    ULONG64 bytesRead;
    HRESULT hr = pVirtualMemory->ReadMemory(spAddrCtx.Get(),
                                            pReadMemory->addr,
                                            pReadMemory->buf,
                                            pReadMemory->bytes,
                                            &bytesRead);

    if (SUCCEEDED(hr))
    {
        *(pReadMemory->bytesread) = static_cast<ULONG>(bytesRead);
        return true;
    }

    return false;
}

bool SymbolImporter_DbgHelp::LegacySymbolEnumerate(_In_ SymbolQueryInformation *pQueryInfo,
                                                   _In_ PSYMBOL_INFOW pSymInfo,
                                                   _In_ ULONG symbolSize)
{
    auto fn = [&]()
    {
        auto it = m_importedIndicies.find(pSymInfo->Index);
        if (it != m_importedIndicies.end())
        {
            return S_OK;
        }

        return S_OK;
    };
    HRESULT hr = ConvertException(fn);

    if (FAILED(hr))
    {
        return FALSE;
    }
    return TRUE;
}

HRESULT SymbolImporter_DbgHelp::ImportForOffsetQuery(_In_ SvcSymbolKind searchKind,
                                                     _In_ ULONG64 offset)
{
    auto fn = [&]()
    {
        //
        // If we've done a full import, don't ever bother checking again.
        //
        if (m_fullGlobalImport)
        {
            return S_FALSE;
        }

        //
        // If we've already done this for a given offset, don't ever bother doing it again.
        //
        auto it = m_addressQueries.find(offset);
        if (it != m_addressQueries.end())
        {
            return S_FALSE;
        }

        ULONG64 displacement;
        if (!SymFromAddrW(m_symHandle,
                          m_moduleBase + offset,
                          &displacement,
                          m_pSymInfo))
        {
            return S_FALSE;
        }

        m_addressQueries.insert(offset);

        return S_OK;
    };
    return ConvertException(fn);
}

HRESULT SymbolImporter_DbgHelp::ImportForNameQuery(_In_ SvcSymbolKind searchKind,
                                                   _In_opt_ PCWSTR pwszName)
{
    auto fn = [&]()
    {
        //
        // If we've done a full import, don't ever bother checking again.
        //
        if (m_fullGlobalImport)
        {
            return S_FALSE;
        }

        std::wstring searchName;
        if (pwszName != nullptr)
        {
            //
            // If we've already done this for a given name, don't ever bother doing it again.
            //
            searchName = pwszName;
            auto it = m_nameQueries.find(searchName);
            if (it != m_nameQueries.end())
            {
                return S_FALSE;
            }
        }

        SymbolQueryCallbackInformation info { };
        info.Query.SearchKind = searchKind;
        info.Query.SearchMask = pwszName;
        info.Query.MaskIsRegEx = false;
        info.Query.QueryOffset = 0;
        info.Importer = this;

        if (!SymEnumSymbolsExW(m_symHandle,
                               m_moduleBase,
                               pwszName,
                               &SymbolImporter_DbgHelp::LegacySymbolEnumerateBridge,
                               reinterpret_cast<void *>(&info),
                               SYMENUM_OPTIONS_DEFAULT))
        {
            return S_FALSE;
        }

        return S_OK;
    };
    return ConvertException(fn);
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

