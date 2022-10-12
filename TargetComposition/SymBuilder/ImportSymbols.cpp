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

bool SymbolImporter_DbgHelp::TagMatchesSearchCriteria(_In_ ULONG tag, _In_ SvcSymbolKind searchKind)
{
    switch(tag)
    {
        //
        // We do not care about any of these:
        //
        default:
        case SymTagExe:
        case SymTagCompiland:
        case SymTagCompilandDetails:
        case SymTagCompilandEnv:
        case SymTagBlock:
        case SymTagAnnotation:
        case SymTagLabel:
        case SymTagFriend:
        case SymTagFuncDebugStart:
        case SymTagFuncDebugEnd:
        case SymTagUsingNamespace:
        case SymTagVTable:
        case SymTagCustom:
        case SymTagThunk:
        case SymTagCustomType:
        case SymTagManagedType:
        case SymTagDimension:
            return false;

        //
        // Function related:
        //
        case SymTagFunction:
            return (searchKind == SvcSymbol || searchKind == SvcSymbolFunction);

        case SymTagFunctionArgType:
            return false;

        //
        // Data related:
        //
        case SymTagData:
            return (searchKind == SvcSymbol || searchKind == SvcSymbolData || searchKind == SvcSymbolDataParameter
                                            || searchKind == SvcSymbolDataLocal);

        case SymTagPublicSymbol:
            return (searchKind == SvcSymbol || searchKind == SvcSymbolPublic);
        
        //
        // Type related:
        //
        case SymTagUDT:
        case SymTagFunctionType:
        case SymTagPointerType:
        case SymTagArrayType:
        case SymTagBaseType:
        case SymTagTypedef:
        case SymTagBaseClass:
            return (searchKind == SvcSymbol || searchKind == SvcSymbolType);

    }
}

HRESULT SymbolImporter_DbgHelp::ImportBaseType(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId)
{
    HRESULT hr = S_OK;

    ULONG64 size;
    ULONG baseType;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_BASETYPE, &baseType) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_LENGTH, &size))
    {
        return E_FAIL;
    }

    wchar_t const *pDefaultName = nullptr;
    SvcSymbolIntrinsicKind ik;

    switch(baseType)
    {
        case btVoid:
            pDefaultName = L"void";
            ik = SvcSymbolIntrinsicVoid;
            break;

        case btInt:
        {
            ik = SvcSymbolIntrinsicInt;
            switch(size)
            {
                case 1:
                    pDefaultName = L"char";
                    break;
                case 2:
                    pDefaultName = L"short";
                    break;
                case 4:
                    pDefaultName = L"int";
                    break;
                case 8:
                    pDefaultName = L"__int64";
                    break;
            }
            break;
        }
        case btUInt:
        {
            ik = SvcSymbolIntrinsicUInt;
            switch(size)
            {
                case 1:
                    pDefaultName = L"unsigned char";
                    break;
                case 2:
                    pDefaultName = L"unsigned short";
                    break;
                case 4:
                    pDefaultName = L"unsigned int";
                    break;
                case 8:
                    pDefaultName = L"unsigned __int64";
                    break;
            }
            break;
        }

        case btFloat:
        {
            ik = SvcSymbolIntrinsicFloat;
            switch(size)
            {
                case 4:
                    pDefaultName = L"float";
                    break;
                case 8:
                    pDefaultName = L"double";
                    break;
            }
            break;
        }

        case btBool:
        {
            ik = SvcSymbolIntrinsicBool;
            switch(size)
            {
                case 1:
                    pDefaultName = L"bool";
                    break;
            }
            break;
        }

        case btLong:
        {
            ik = SvcSymbolIntrinsicLong;
            switch(size)
            {
                case 4:
                    pDefaultName = L"long";
                    break;
                case 8:
                    pDefaultName = L"long long";
                    break;
            }
            break;
        }

        case btULong:
        {
            ik = SvcSymbolIntrinsicULong;
            switch(size)
            {
                case 4:
                    pDefaultName = L"unsigned long";
                    break;
                case 8:
                    pDefaultName = L"unsigned long long";
                    break;
            }
            break;
        }

        case btChar:
        {
            ik = SvcSymbolIntrinsicChar;
            switch(size)
            {
                case 1:
                    pDefaultName = L"char";
                    break;
            }
            break;
        }

        case btWChar:
        {
            ik = SvcSymbolIntrinsicWChar;
            switch(size)
            {
                case 2:
                    pDefaultName = L"wchar_t";
                    break;
            }
            break;
        }

        case btChar16:
        {
            ik = SvcSymbolIntrinsicChar16;
            switch(size)
            {
                case 2:
                    pDefaultName = L"char16_t";
                    break;
            }
        }
        
        case btChar32:
        {
            ik = SvcSymbolIntrinsicChar32;
            switch(size)
            {
                case 4:
                    pDefaultName = L"char32_t";
                    break;
            }
        }

        default:
            break;
    }

    //
    // We cannot create one of our synthetic symbols without a name.  If this is a basic type
    // that we do not recognize, we are sunk.
    //
    if (pDefaultName == nullptr)
    {
        return E_FAIL;
    }

    //
    // If we already know this basic type, just return it.
    //
    ULONG64 builderId;
    HRESULT hrFind = m_pOwningSet->FindTypeByName(pDefaultName, &builderId, nullptr, false);
    if (SUCCEEDED(hrFind))
    {
        *pBuilderId = builderId;
        return S_FALSE;
    }

    ComPtr<BasicTypeSymbol> spBasicTypeSymbol;
    IfFailedReturn(MakeAndInitialize<BasicTypeSymbol>(&spBasicTypeSymbol, 
                                                      m_pOwningSet, 
                                                      ik,  
                                                      static_cast<ULONG>(size), 
                                                      pDefaultName));

    *pBuilderId = spBasicTypeSymbol->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportUDT(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId)
{
    HRESULT hr = S_OK;

    WCHAR *pSymName;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pSymName))
    {
        return E_FAIL;
    }
    localstr_ptr spSymName(pSymName);

    ULONG64 udtSize;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_LENGTH, &udtSize))
    {
        return E_FAIL;
    }

    ULONG childCount;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_CHILDRENCOUNT, &childCount))
    {
        return E_FAIL;
    }

    //
    // Now that we have some basic information about the UDT, go and create the shell of it in the symbol
    // builder and then copy over base classes, fields, and any other data we wish to import one by one.
    //
    ComPtr<UdtTypeSymbol> spUdt;
    IfFailedReturn(MakeAndInitialize<UdtTypeSymbol>(&spUdt,
                                                    m_pOwningSet,
                                                    0,
                                                    pSymName,
                                                    nullptr));

    std::unique_ptr<char []> spBuf(new char[sizeof(TI_FINDCHILDREN_PARAMS) + sizeof(ULONG) * childCount]);
    TI_FINDCHILDREN_PARAMS *pChildQuery = reinterpret_cast<TI_FINDCHILDREN_PARAMS *>(spBuf.get());

    pChildQuery->Count = childCount;
    pChildQuery->Start = 0;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_FINDCHILDREN, pChildQuery))
    {
        return E_FAIL;
    }

    enum SymTagEnum passTags[] = { SymTagBaseClass, SymTagData };
    for (size_t pass = 0; pass < ARRAYSIZE(passTags); ++pass)
    {
        for (ULONG i = 0; i < childCount; ++i)
        {
            ULONG childTag;
            if (!SymGetTypeInfo(m_symHandle, m_moduleBase, pChildQuery->ChildId[i], TI_GET_SYMTAG, &childTag))
            {
                return E_FAIL;
            }

            if (childTag != static_cast<ULONG>(passTags[pass]))
            {
                continue;
            }

            switch(passTags[pass])
            {
                case SymTagBaseClass:
                {
                    // @TODO:
                    break;
                }

                case SymTagData:
                {
                    //
                    // If we have a data, go ask what kind of data it is.
                    //
                    enum DataKind dk;
                    ULONG childTI;
                    WCHAR *pDataName;

                    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, pChildQuery->ChildId[i], TI_GET_DATAKIND, &dk) ||
                        !SymGetTypeInfo(m_symHandle, m_moduleBase, pChildQuery->ChildId[i], TI_GET_TYPEID, &childTI) ||
                        !SymGetTypeInfo(m_symHandle, m_moduleBase, pChildQuery->ChildId[i], TI_GET_SYMNAME, &pDataName))
                    {
                        return E_FAIL;
                    }

                    localstr_ptr spDataName(pDataName);

                    switch(dk)
                    {
                        //
                        // If it's a field of a UDT, we need to import the type.
                        //
                        case DataIsMember:
                        {
                            ULONG64 memberBuilderType;
                            IfFailedReturn(ImportSymbol(childTI, &memberBuilderType));

                            ULONG offset;
                            if (!SymGetTypeInfo(m_symHandle, m_moduleBase, pChildQuery->ChildId[i], TI_GET_OFFSET, &offset))
                            {
                                return E_FAIL;
                            }

                            ComPtr<FieldSymbol> spField;
                            IfFailedReturn(MakeAndInitialize<FieldSymbol>(&spField,
                                                                          m_pOwningSet,
                                                                          spUdt->InternalGetId(),
                                                                          offset,
                                                                          memberBuilderType,
                                                                          pDataName));

                            break;
                        }

                        default:
                            //
                            // We don't import this part of the type.
                            //
                            break;
                    }

                    break;
                }

                default:
                    return E_UNEXPECTED;
            }
        }
    }

    *pBuilderId = spUdt->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportFunction(_In_ ULONG /*symIndex*/, _Out_ ULONG64 * /*pBuilderId*/)
{
    return E_NOTIMPL;
}

HRESULT SymbolImporter_DbgHelp::ImportPointer(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId)
{
    HRESULT hr = S_OK;

    //
    // NOTE: Unfortunately, the DBGHelp APIs cannot differentiate a standard reference from an rvalue
    //       one despite DIA (and ISvcSymbolSet) being able to do that.  We are stuck with * or & on
    //       an import.
    //
    ULONG pointerToId;
    ULONG isReference;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &pointerToId) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_IS_REFERENCE, &isReference))
    {
        return E_FAIL;
    }

    ULONG64 pointerToBuilderId;
    IfFailedReturn(ImportTypeSymbol(pointerToId, &pointerToBuilderId));

    ComPtr<PointerTypeSymbol> spPointer;
    IfFailedReturn(MakeAndInitialize<PointerTypeSymbol>(&spPointer,
                                                        m_pOwningSet,
                                                        pointerToBuilderId,
                                                        isReference == 0 ? SvcSymbolPointerStandard
                                                                         : SvcSymbolPointerReference));

    *pBuilderId = spPointer->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportArray(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId)
{
    HRESULT hr = S_OK;

    ULONG arrayOfId;
    ULONG64 arraySize;
    ULONG64 baseSize;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &arrayOfId) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_LENGTH, &arraySize) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, arrayOfId, TI_GET_LENGTH, &baseSize))
    {
        return E_FAIL;
    }

    ULONG64 arrayOfBuilderId;
    IfFailedReturn(ImportTypeSymbol(arrayOfId, &arrayOfBuilderId));

    //
    // NOTE: If the base type of the array was already defined (in symbol builder symbols) and we resolve to
    //       that, the size of the array may be off what we indicate here.  This is the size of the array
    //       in the PDB (or whatever symbols DbgHelp is proxying).
    //      


    ComPtr<ArrayTypeSymbol> spArray;
    IfFailedReturn(MakeAndInitialize<ArrayTypeSymbol>(&spArray,
                                                      m_pOwningSet,
                                                      arrayOfBuilderId,
                                                      arraySize / baseSize));

    *pBuilderId = spArray->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportTypedef(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId)
{
    HRESULT hr = S_OK;

    WCHAR *pSymName;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pSymName))
    {
        return E_FAIL;
    }
    localstr_ptr spSymName(pSymName);

    ULONG typedefTypeId;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &typedefTypeId))
    {
        return E_FAIL;
    }

    ULONG64 typedefBuilderId;
    IfFailedReturn(ImportTypeSymbol(typedefTypeId, &typedefBuilderId));

    ComPtr<TypedefTypeSymbol> spTypedef;
    IfFailedReturn(MakeAndInitialize<TypedefTypeSymbol>(&spTypedef,
                                                        m_pOwningSet,
                                                        typedefBuilderId,
                                                        0,
                                                        pSymName,
                                                        nullptr));

    *pBuilderId = spTypedef->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportTypeSymbol(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId)
{
    ULONG tag;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMTAG, &tag))
    {
        return E_FAIL;
    }

    bool isNamed = (tag == SymTagUDT || tag == SymTagTypedef);
    if (isNamed)
    {
        WCHAR *pSymName;
        if (SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pSymName))
        {
            localstr_ptr spSymName(pSymName);

            //
            // Make sure there is *NOT* a naming conflict on this particular type.  Maybe someone already created
            // this type explicitly before we tried to import anything...
            //
            ULONG64 builderId;
            HRESULT hrFind = m_pOwningSet->FindTypeByName(pSymName, &builderId, nullptr, false);
            if (SUCCEEDED(hrFind))
            {
                //
                // At this point, we have a naming conflict.  We can either fail to import this or just point to the
                // existing type.  Here, we choose the latter.
                //
                *pBuilderId = builderId;
                return S_FALSE;
            }
        }
    }

    switch(tag)
    {
        case SymTagUDT:
            return ImportUDT(symIndex, pBuilderId);

        case SymTagBaseType:
            return ImportBaseType(symIndex, pBuilderId);

        case SymTagTypedef:
            return ImportTypedef(symIndex, pBuilderId);

        case SymTagPointerType:
            return ImportPointer(symIndex, pBuilderId);

        case SymTagArrayType:
            return ImportArray(symIndex, pBuilderId);

        case SymTagFunctionType:
            return E_NOTIMPL;

        default:
            return E_NOTIMPL;
    }
}

HRESULT SymbolImporter_DbgHelp::ImportSymbol(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId)
{
    HRESULT hr = S_OK;

    //
    // If we have already imported this particular symbol, just return the ID within the symbol builder
    // of the import and be done.  S_FALSE indicates this situation.  It's a "successful import" but we didn't
    // actually do anything.
    //
    // This might, for instance, be because some other import referenced it (e.g.: a field of a UDT being imported
    // refers to a type that's already been imported).
    //
    auto it = m_importedIndexMap.find(symIndex);
    if (it != m_importedIndexMap.end())
    {
        *pBuilderId = it->second;
        return S_FALSE;
    }

    ULONG tag;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMTAG, &tag))
    {
        return E_FAIL;
    }

    switch(tag)
    {
        case SymTagFunction:
            return ImportFunction(symIndex, pBuilderId);

        case SymTagData:
            return E_NOTIMPL; // return ImportGlobalData(symIndex, pBuilderId);

        case SymTagUDT:
        case SymTagBaseType:
        case SymTagTypedef:
        case SymTagFunctionType:
        case SymTagPointerType:
        case SymTagArrayType:
            return ImportTypeSymbol(symIndex, pBuilderId);

        case SymTagBaseClass:
            return E_NOTIMPL;

        default:
            return E_NOTIMPL;
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
        auto it = m_importedIndexMap.find(pSymInfo->Index);
        if (it != m_importedIndexMap.end())
        {
            return S_OK;
        }

        //
        // If we do not care about this symbol, skip it.
        //
        if (!TagMatchesSearchCriteria(pSymInfo->Tag, pQueryInfo->SearchKind))
        {
            return S_OK;
        }

        ULONG64 importedId;
        (void)ImportSymbol(pSymInfo, &importedId);

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
    //
    // **FOR NOW**: Do not allow a full import by name.  If there's a search for everything,
    //              we are *NOT* going to pull the entire contents across into the symbol builder.
    //              Yes...  that means you can do a query by name and see things that won't appear with
    //              a global query.  It prevents a number of huge performance pains around checking
    //              nested types or our lack of RegEx support.
    //
    if (pwszName == nullptr)
    {
        return E_NOTIMPL;
    }

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

        if (searchKind != SvcSymbolType)
        {
            if (!SymEnumSymbolsExW(m_symHandle,
                                   m_moduleBase,
                                   pwszName,
                                   &SymbolImporter_DbgHelp::LegacySymbolEnumerateBridge,
                                   reinterpret_cast<void *>(&info),
                                   SYMENUM_OPTIONS_DEFAULT))
            {
                return S_FALSE;
            }
        }

        if (searchKind == SvcSymbolType || searchKind == SvcSymbol)
        {
            if (!SymEnumTypesByNameW(m_symHandle,
                                     m_moduleBase,
                                     pwszName,
                                     &SymbolImporter_DbgHelp::LegacySymbolEnumerateBridge,
                                     reinterpret_cast<void *>(&info)))
            {
                return S_FALSE;
            }
        }

        if (pwszName != nullptr)
        {
            m_nameQueries.insert(searchName);
        }
        else
        {
            m_fullGlobalImport = true;
        }

        return S_OK;
    };
    return ConvertException(fn);
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

