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

HRESULT SymbolImporter::ImportFailure(_In_ HRESULT hr, _In_opt_ PCWSTR /*pwszImportMsg*/)
{
    return hr;
}

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

    auto fn = [&]()
    {
        std::wstring info;
        switch(modInfo.SymType)
        {
            case SymPdb:
                info += L"PDB: ";
                info += modInfo.LoadedPdbName;
                break;
            case SymExport:
                info += L"Export Symbols";
                break;
            default:
                info += L"Other";
                break;
        }
        m_importerInfo = info;
        return S_OK;
    };

    return ConvertException(fn);
}

void SymbolImporter_DbgHelp::DisconnectFromSource()
{
    if (m_symHandle != NULL)
    {
        SymCleanup(m_symHandle);
        m_symHandle = NULL;
        m_importerInfo.clear();
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

HRESULT SymbolImporter_DbgHelp::ImportBaseType(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 /*parentId*/)
{
    HRESULT hr = S_OK;

    ULONG64 size;
    ULONG baseType;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_BASETYPE, &baseType) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_LENGTH, &size))
    {
        return ImportFailure(E_FAIL);
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
            break;
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
            break;
        }

        case btHresult:
        {
            ik = SvcSymbolIntrinsicHRESULT;
            switch(size)
            {
                case 4:
                    pDefaultName = L"HRESULT";
                    break;
            }
            break;
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
        return ImportFailure(E_FAIL);
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
    hr = MakeAndInitialize<BasicTypeSymbol>(&spBasicTypeSymbol, 
                                            m_pOwningSet, 
                                            ik,  
                                            static_cast<ULONG>(size), 
                                            pDefaultName);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spBasicTypeSymbol->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportBaseClass(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    if (parentId == 0)
    {
        return ImportFailure(E_UNEXPECTED);
    }

    //
    // We do *NOT* handle virtual base class imports at present!
    //
    ULONG baseClassTypeId;
    ULONG baseClassOffset;

    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &baseClassTypeId) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_OFFSET, &baseClassOffset))
    {
        return ImportFailure(E_FAIL);
    }

    ULONG64 baseClassTypeBuilderId;
    IfFailedReturn(ImportSymbol(baseClassTypeId, &baseClassTypeBuilderId));

    ComPtr<BaseClassSymbol> spBaseClass;
    hr = MakeAndInitialize<BaseClassSymbol>(&spBaseClass,
                                            m_pOwningSet,
                                            parentId,
                                            baseClassOffset,
                                            baseClassTypeBuilderId);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spBaseClass->InternalGetId();
    return hr;
}

HRESULT SymbolImporter_DbgHelp::ImportMemberData(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    if (parentId == 0)
    {
        //
        // Member data cannot *NOT* have a parent.  It must be parented to a type...  and one which is a UDT!
        //
        return ImportFailure(E_UNEXPECTED);
    }

    ULONG childTI;
    WCHAR *pDataName;
    ULONG offset;

    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &childTI) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_OFFSET, &offset) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pDataName))
    {
        return ImportFailure(E_FAIL);
    }

    localstr_ptr spDataName(pDataName);

    ULONG64 memberBuilderType;
    IfFailedReturn(ImportSymbol(childTI, &memberBuilderType));

    ComPtr<FieldSymbol> spField;
    hr = MakeAndInitialize<FieldSymbol>(&spField,
                                        m_pOwningSet,
                                        parentId,
                                        offset,
                                        memberBuilderType,
                                        pDataName);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spField->InternalGetId();
    return hr;
}

HRESULT SymbolImporter_DbgHelp::ImportConstantData(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    //
    // Right now, we *ONLY* deal with enumerants (and not things like global constants)
    //
    if (parentId == 0)
    {
        return ImportFailure(E_NOTIMPL);
    }

    BaseSymbol *pParentSymbol = m_pOwningSet->InternalGetSymbol(parentId);
    if (pParentSymbol == nullptr || pParentSymbol->InternalGetKind() != SvcSymbolType)
    {
        return ImportFailure(E_UNEXPECTED);
    }

    //
    // We *ONLY* support enumerants right now.
    //
    BaseTypeSymbol *pParentType = static_cast<BaseTypeSymbol *>(pParentSymbol);
    if (pParentType->InternalGetTypeKind() != SvcSymbolTypeEnum)
    {
        return ImportFailure(E_NOTIMPL);
    }

    VARIANT vtVal;
    WCHAR *pDataName;

    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_VALUE, &vtVal) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pDataName))
    {
        return ImportFailure(E_FAIL);
    }

    localstr_ptr spDataName(pDataName);

    ComPtr<FieldSymbol> spField;
    hr = MakeAndInitialize<FieldSymbol>(&spField,
                                        m_pOwningSet,
                                        parentId,
                                        0,
                                        &vtVal,
                                        pDataName);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spField->InternalGetId();
    return hr;
}

HRESULT SymbolImporter_DbgHelp::ImportDataSymbol(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    enum DataKind dk;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_DATAKIND, &dk))
    {
        return ImportFailure(E_FAIL);
    }

    switch(dk)
    {
        case DataIsMember:
            return ImportMemberData(symIndex, pBuilderId, parentId);

        case DataIsConstant:
            return ImportConstantData(symIndex, pBuilderId, parentId);

        default:
            //
            // We do not **YET** support a number of symbols here.  Do **NOT** ImportFailure(...)
            //
            return E_NOTIMPL;
    }
}

HRESULT SymbolImporter_DbgHelp::ImportEnum(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    WCHAR *pSymName;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pSymName))
    {
        return ImportFailure(E_FAIL);
    }
    localstr_ptr spSymName(pSymName);

    ULONG childCount;
    ULONG enumBaseTypeIndex;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &enumBaseTypeIndex) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_CHILDRENCOUNT, &childCount))
    {
        return ImportFailure(E_FAIL);
    }

    ULONG64 enumBaseTypeBuilderId;
    IfFailedReturn(ImportSymbol(enumBaseTypeIndex, &enumBaseTypeBuilderId));

    //
    // Now that we have some basic information about the UDT, go and create the shell of it in the symbol
    // builder and then copy over base classes, fields, and any other data we wish to import one by one.
    //
    ComPtr<EnumTypeSymbol> spEnum;
    hr = MakeAndInitialize<EnumTypeSymbol>(&spEnum, m_pOwningSet, enumBaseTypeBuilderId, parentId, pSymName, nullptr);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    std::unique_ptr<char []> spBuf(new char[sizeof(TI_FINDCHILDREN_PARAMS) + sizeof(ULONG) * childCount]);
    TI_FINDCHILDREN_PARAMS *pChildQuery = reinterpret_cast<TI_FINDCHILDREN_PARAMS *>(spBuf.get());

    pChildQuery->Count = childCount;
    pChildQuery->Start = 0;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_FINDCHILDREN, pChildQuery))
    {
        return ImportFailure(E_FAIL);
    }

    enum SymTagEnum passTags[] = { SymTagData };
    for (size_t pass = 0; pass < ARRAYSIZE(passTags); ++pass)
    {
        for (ULONG i = 0; i < childCount; ++i)
        {
            ULONG childIndex = pChildQuery->ChildId[i];

            ULONG childTag;
            if (!SymGetTypeInfo(m_symHandle, m_moduleBase, childIndex, TI_GET_SYMTAG, &childTag))
            {
                return ImportFailure(E_FAIL);
            }

            if (childTag != static_cast<ULONG>(passTags[pass]))
            {
                continue;
            }

            ULONG64 childBuilderId;
            IfFailedReturn(ImportSymbol(childIndex, &childBuilderId, spEnum->InternalGetId()));
        }
    }

    *pBuilderId = spEnum->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportUDT(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    WCHAR *pSymName;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pSymName))
    {
        return ImportFailure(E_FAIL);
    }
    localstr_ptr spSymName(pSymName);

    ULONG64 udtSize;
    ULONG childCount;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_LENGTH, &udtSize) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_CHILDRENCOUNT, &childCount))
    {
        return ImportFailure(E_FAIL);
    }

    //
    // Now that we have some basic information about the UDT, go and create the shell of it in the symbol
    // builder and then copy over base classes, fields, and any other data we wish to import one by one.
    //
    ComPtr<UdtTypeSymbol> spUdt;
    hr = MakeAndInitialize<UdtTypeSymbol>(&spUdt, m_pOwningSet, parentId, pSymName, nullptr);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    //
    // A UDT may contain pointers to itself (or may contain a UDT or a pointer to a UDT which contains pointers
    // back to itself).  In order for those pointers to resolve correctly, we must have this UDT in the index
    // table already so that the linkages can be set up without causing errors or an infinite import chain.
    //
    IfFailedReturn(ConvertException([&]()
    {
        m_importedIndexMap.insert({ symIndex, spUdt->InternalGetId() });
        return S_OK;
    }));

    std::unique_ptr<char []> spBuf(new char[sizeof(TI_FINDCHILDREN_PARAMS) + sizeof(ULONG) * childCount]);
    TI_FINDCHILDREN_PARAMS *pChildQuery = reinterpret_cast<TI_FINDCHILDREN_PARAMS *>(spBuf.get());

    pChildQuery->Count = childCount;
    pChildQuery->Start = 0;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_FINDCHILDREN, pChildQuery))
    {
        return ImportFailure(E_FAIL);
    }

    enum SymTagEnum passTags[] = { SymTagBaseClass, SymTagData };
    for (size_t pass = 0; pass < ARRAYSIZE(passTags); ++pass)
    {
        for (ULONG i = 0; i < childCount; ++i)
        {
            ULONG childIndex = pChildQuery->ChildId[i];

            ULONG childTag;
            if (!SymGetTypeInfo(m_symHandle, m_moduleBase, childIndex, TI_GET_SYMTAG, &childTag))
            {
                return ImportFailure(E_FAIL);
            }

            if (childTag != static_cast<ULONG>(passTags[pass]))
            {
                continue;
            }

            ULONG64 childBuilderId;
            HRESULT hrChild = ImportSymbol(childIndex, &childBuilderId, spUdt->InternalGetId());
            if (FAILED(hrChild))
            {
                //
                // If there is a part of the type we cannot import (e.g.: because we do not support static fields
                // or something similar), we will move on and import the rest.
                //
                if (hrChild != E_NOTIMPL)
                {
                    return hrChild;
                }
            }
        }
    }

    *pBuilderId = spUdt->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportFunctionType(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    ULONG funcReturnTypeId;
    ULONG childCount;

    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &funcReturnTypeId) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_CHILDRENCOUNT, &childCount))
    {
        return ImportFailure(E_FAIL);
    }

    ULONG64 funcReturnTypeBuilderId;
    IfFailedReturn(ImportSymbol(funcReturnTypeId, &funcReturnTypeBuilderId));

    std::unique_ptr<char []> spBuf(new char[sizeof(TI_FINDCHILDREN_PARAMS) + sizeof(ULONG) * childCount]);
    TI_FINDCHILDREN_PARAMS *pChildQuery = reinterpret_cast<TI_FINDCHILDREN_PARAMS *>(spBuf.get());

    pChildQuery->Count = childCount;
    pChildQuery->Start = 0;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_FINDCHILDREN, pChildQuery))
    {
        return ImportFailure(E_FAIL);
    }

    //
    // Allocate enough of an ID array to guarantee that we can fill in parameters.  This can be no longer
    // than the number of children.
    //
    std::unique_ptr<ULONG64 []> spParams;
    if (childCount != 0)
    {
        spParams.reset(new ULONG64[childCount]);
    }
    ULONG paramCount = 0;

    enum SymTagEnum passTags[] = { SymTagFunctionArgType };
    for (size_t pass = 0; pass < ARRAYSIZE(passTags); ++pass)
    {
        for (ULONG i = 0; i < childCount; ++i)
        {
            ULONG childIndex = pChildQuery->ChildId[i];

            ULONG childTag;
            if (!SymGetTypeInfo(m_symHandle, m_moduleBase, childIndex, TI_GET_SYMTAG, &childTag))
            {
                return ImportFailure(E_FAIL);
            }

            if (childTag != static_cast<ULONG>(passTags[pass]))
            {
                continue;
            }

            ULONG childTypeId;
            if (!SymGetTypeInfo(m_symHandle, m_moduleBase, childIndex, TI_GET_TYPEID, &childTypeId))
            {
                return ImportFailure(E_FAIL);
            }

            ULONG64 childTypeBuilderId;
            IfFailedReturn(ImportSymbol(childTypeId, &childTypeBuilderId));

            spParams[paramCount] = childTypeBuilderId;
            ++paramCount;
        }
    }

    ComPtr<FunctionTypeSymbol> spFunctionType;
    hr = MakeAndInitialize<FunctionTypeSymbol>(&spFunctionType,
                                               m_pOwningSet,
                                               funcReturnTypeBuilderId,
                                               paramCount,
                                               paramCount == 0 ? nullptr : spParams.get());
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spFunctionType->InternalGetId();
    return hr;
}

HRESULT SymbolImporter_DbgHelp::ImportPublicSymbol(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    //
    // Public symbols are just global sym->addr mappings.  There better not be a parent symbol!
    //
    if (parentId != 0)
    {
        return ImportFailure(E_UNEXPECTED);
    }

    WCHAR *pSymName;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pSymName))
    {
        return ImportFailure(E_FAIL);
    }
    localstr_ptr spSymName(pSymName);

    ULONG64 publicAddr;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_ADDRESS, &publicAddr))
    {
        return ImportFailure(E_FAIL);
    }

    ComPtr<PublicSymbol> spPublic;
    hr = MakeAndInitialize<PublicSymbol>(&spPublic,
                                         m_pOwningSet,
                                         publicAddr - m_moduleBase,
                                         pSymName,
                                         nullptr);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spPublic->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportFunction(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    WCHAR *pSymName;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pSymName))
    {
        return ImportFailure(E_FAIL);
    }
    localstr_ptr spSymName(pSymName);

    ULONG64 funcAddr;
    ULONG64 funcSize;
    ULONG funcTypeId;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_ADDRESS, &funcAddr) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_LENGTH, &funcSize) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &funcTypeId))
    {
        return ImportFailure(E_FAIL);
    }

    //
    // On stripped public symbols, it is entirely possible to find "functions" for which we cannot get
    // any return type.  In this case, lie and say it's void.
    //
    ULONG funcReturnTypeId;
    bool hasFuncReturnType = !!SymGetTypeInfo(m_symHandle, m_moduleBase, funcTypeId, TI_GET_TYPEID, &funcReturnTypeId);

    ULONG64 funcReturnTypeBuilderId;
    if (hasFuncReturnType)
    {
        IfFailedReturn(ImportSymbol(funcReturnTypeId, &funcReturnTypeBuilderId));
    }
    else
    {
        auto fn = [&]()
        {
            std::wstring fakeReturnType = L"void";
            IfFailedReturn(m_pOwningSet->FindTypeByName(fakeReturnType, &funcReturnTypeBuilderId, nullptr, false));
            return S_OK;
        };
        IfFailedReturn(ConvertException(fn));
    }

    ComPtr<FunctionSymbol> spFunction;
    hr = MakeAndInitialize<FunctionSymbol>(&spFunction,
                                           m_pOwningSet,
                                           parentId,
                                           funcReturnTypeBuilderId,
                                           funcAddr - m_moduleBase,
                                           funcSize,
                                           pSymName,
                                           nullptr);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spFunction->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportPointer(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 /*parentId*/)
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
        return ImportFailure(E_FAIL);
    }

    //
    // If we see an "X *" before having seen the "X" *AND* the "X" happens to contain an "X *", the
    // recursive call to ImportSymbol(pointerToId, ...) will reimport the same symIndex.  *BOTH* of these
    // need to resolve to the same type at the end of the day.  We do *NOT* want to create two different
    // pointer symbols that the same "symIndex" happens to refer to. 
    //
    // In order to have even gotten to this point, the levels above us will have seen no mapping of
    // symIndex -> any symbol builder ID.  
    //
    // Should the above mentioned situation occur, the inner recursion will have seen "X" as a definition 
    // even though it is, as yet, not fully imported and will create the pointer.  When we get to the outer
    // ImportPointer, check if there is a mapping of symIndex -> symbol builder Id and immediately bail
    // if that is the case!
    //
    ULONG64 pointerToBuilderId;
    IfFailedReturn(ImportSymbol(pointerToId, &pointerToBuilderId));

    auto it = m_importedIndexMap.find(symIndex);
    if (it != m_importedIndexMap.end())
    {
        //
        // It had better be a pointer-to type!
        //
        BaseSymbol *pSymbol = m_pOwningSet->InternalGetSymbol(it->second);
        if (pSymbol->InternalGetKind() != SvcSymbolType ||
            static_cast<BaseTypeSymbol *>(pSymbol)->InternalGetTypeKind() != SvcSymbolTypePointer)
        {
            //
            // This is catastrophic.  Something went horribly awry in the recursive import.
            //
            return ImportFailure(E_UNEXPECTED);
        }

        *pBuilderId = it->second;
        return S_FALSE;
    }

    ComPtr<PointerTypeSymbol> spPointer;
    hr = MakeAndInitialize<PointerTypeSymbol>(&spPointer,
                                              m_pOwningSet,
                                              pointerToBuilderId,
                                              isReference == 0 ? SvcSymbolPointerStandard
                                                               : SvcSymbolPointerReference);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spPointer->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportArray(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 /*parentId*/)
{
    HRESULT hr = S_OK;

    ULONG arrayOfId;
    ULONG64 arraySize;
    ULONG64 baseSize;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &arrayOfId) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_LENGTH, &arraySize) ||
        !SymGetTypeInfo(m_symHandle, m_moduleBase, arrayOfId, TI_GET_LENGTH, &baseSize))
    {
        return ImportFailure(E_FAIL);
    }

    ULONG64 arrayOfBuilderId;
    IfFailedReturn(ImportSymbol(arrayOfId, &arrayOfBuilderId));

    //
    // NOTE: If the base type of the array was already defined (in symbol builder symbols) and we resolve to
    //       that, the size of the array may be off what we indicate here.  This is the size of the array
    //       in the PDB (or whatever symbols DbgHelp is proxying).
    //      


    ComPtr<ArrayTypeSymbol> spArray;
    hr = MakeAndInitialize<ArrayTypeSymbol>(&spArray,
                                            m_pOwningSet,
                                            arrayOfBuilderId,
                                            arraySize / baseSize);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spArray->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportTypedef(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    HRESULT hr = S_OK;

    WCHAR *pSymName;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMNAME, &pSymName))
    {
        return ImportFailure(E_FAIL);
    }
    localstr_ptr spSymName(pSymName);

    ULONG typedefTypeId;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_TYPEID, &typedefTypeId))
    {
        return ImportFailure(E_FAIL);
    }

    ULONG64 typedefBuilderId;
    IfFailedReturn(ImportSymbol(typedefTypeId, &typedefBuilderId));

    ComPtr<TypedefTypeSymbol> spTypedef;
    hr = MakeAndInitialize<TypedefTypeSymbol>(&spTypedef,
                                              m_pOwningSet,
                                              typedefBuilderId,
                                              parentId,
                                              pSymName,
                                              nullptr);
    if (FAILED(hr))
    {
        return ImportFailure(hr);
    }

    *pBuilderId = spTypedef->InternalGetId();
    return S_OK;
}

HRESULT SymbolImporter_DbgHelp::ImportTypeSymbol(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
{
    ULONG tag;
    if (!SymGetTypeInfo(m_symHandle, m_moduleBase, symIndex, TI_GET_SYMTAG, &tag))
    {
        return ImportFailure(E_FAIL);
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
            return ImportUDT(symIndex, pBuilderId, parentId);

        case SymTagBaseType:
            return ImportBaseType(symIndex, pBuilderId, parentId);

        case SymTagTypedef:
            return ImportTypedef(symIndex, pBuilderId, parentId);

        case SymTagPointerType:
            return ImportPointer(symIndex, pBuilderId, parentId);

        case SymTagArrayType:
            return ImportArray(symIndex, pBuilderId, parentId);

        case SymTagEnum:
            return ImportEnum(symIndex, pBuilderId, parentId);

        case SymTagFunctionType:
            return ImportFunctionType(symIndex, pBuilderId, parentId);

        default:
            return ImportFailure(E_NOTIMPL);
    }
}

HRESULT SymbolImporter_DbgHelp::ImportSymbol(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId)
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
        return ImportFailure(E_FAIL);
    }

    auto fn = [&]()
    {
        ULONG64 builderId;

        switch(tag)
        {
            case SymTagFunction:
                hr = ImportFunction(symIndex, &builderId, parentId);
                break;

            case SymTagData:
                hr = ImportDataSymbol(symIndex, pBuilderId, parentId);
                break;

            case SymTagUDT:
            case SymTagBaseType:
            case SymTagTypedef:
            case SymTagFunctionType:
            case SymTagPointerType:
            case SymTagArrayType:
            case SymTagEnum:
                hr = ImportTypeSymbol(symIndex, &builderId, parentId);
                break;

            case SymTagBaseClass:
                hr = ImportBaseClass(symIndex, pBuilderId, parentId);
                break;

            case SymTagPublicSymbol:
                hr = ImportPublicSymbol(symIndex, pBuilderId, parentId);
                break;

            default:
                hr = E_NOTIMPL;
                break;
        }

        //
        // If it hasn't already been inserted into the table, do so now.  Some types must do this immediately
        // (e.g.: UDTs) because they may contain pointers to themselves and the like and we need to be able to set
        // up the linkages early.
        //
        if (SUCCEEDED(hr))
        {
            auto itpost = m_importedIndexMap.find(symIndex);
            if (itpost == m_importedIndexMap.end())
            {
                m_importedIndexMap.insert( { symIndex, builderId });
            }
            else
            {
                if (itpost->second != builderId)
                {
                    //
                    // This is catastrophic!  It should never happen!  Someone inserted the import into the table
                    // with the wrong ID!
                    //
                    return ImportFailure(E_UNEXPECTED);
                }
            }

            *pBuilderId = builderId;
        }

        return hr;
    };
    return ConvertException(fn);
}

bool SymbolImporter_DbgHelp::LegacyReadMemory(_Inout_ IMAGEHLP_CBA_READ_MEMORY *pReadMemory)
{
    auto pOwningProcess = m_pOwningSet->GetOwningProcess();
    auto pVirtualMemory = pOwningProcess->GetVirtualMemory();
    bool isKernel = pOwningProcess->IsKernel();
    ULONG64 processKey = pOwningProcess->GetProcessKey();

    //
    // If we have a generalized view of the kernel and not a specific "process context", we can go and ask
    // for the generalized kernel address context in which to perform any memory reads.
    //
    ComPtr<ISvcAddressContext> spAddrCtx;
    if (isKernel && processKey == 0)
    {
        if (FAILED(pOwningProcess->GetSymbolBuilderManager()->GetKernelAddressContext(&spAddrCtx)))
        {
            return false;
        }
    }
    else
    {
        ComPtr<ISvcProcess> spProcess;
        if (FAILED(pOwningProcess->GetSymbolBuilderManager()->ProcessKeyToProcess(processKey, &spProcess)) ||
            FAILED(spProcess.As(&spAddrCtx)))
        {
            return false;
        }
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
    //
    // This is happening at type query time as part of the *TARGET COMPOSITION* layer.  We 
    // *ABSOLUTELY CANNOT* send a cache invalidation at this time.  To do so might flush caches that
    // are in the middle of use!
    //
    m_pOwningSet->SetCacheInvalidationDisable(true);

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

        ULONG64 importedId;
        (void)ImportSymbol(m_pSymInfo, &importedId);

        m_addressQueries.insert(offset);

        return S_OK;
    };
    HRESULT hr = ConvertException(fn);

    m_pOwningSet->SetCacheInvalidationDisable(false);
    return hr;
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

    //
    // This is happening at type query time as part of the *TARGET COMPOSITION* layer.  We 
    // *ABSOLUTELY CANNOT* send a cache invalidation at this time.  To do so might flush caches that
    // are in the middle of use!
    //
    m_pOwningSet->SetCacheInvalidationDisable(true);

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
    HRESULT hr = ConvertException(fn);

    m_pOwningSet->SetCacheInvalidationDisable(false);
    return hr;
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

