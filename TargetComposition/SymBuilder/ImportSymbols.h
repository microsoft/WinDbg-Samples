//**************************************************************************
//
// ImportSymbols.h
//
// The header for our notion of importing symbols from another source on demand.  If we
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

#ifndef __IMPORTSYMBOLS_H__
#define __IMPORTSYMBOLS_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

//
// Forward Declarations:
//
class SymbolSet;

//*************************************************
// Generic Import:
//

// SymbolImporter:
//
// An abstract class which provides the interfaces necessary to import symbols from a secondary data source
// (e.g.: DbgHelp, DIA, another ISvcSymbolSet, etc...)
//
// Note that a symbol set *OWNS* the lifetime of a SymbolImporter.
//
class SymbolImporter
{
public:

    // SymbolImproter():
    //
    // Constructs a new symbol importer for a given symbol set.
    //
    SymbolImporter(_In_ SymbolSet *pOwningSet) :
        m_pOwningSet(pOwningSet)
    {
    }

    // ConnectToSource():
    //
    // Connects the importer to its underlying data source.  If this fails, the importer is not used.
    //
    virtual HRESULT ConnectToSource() =0;

    // DisconnectFromSource():
    //
    // Disconnects the importer from its underlying data source.  After this call, all Import* methods
    // should fail.
    //
    virtual void DisconnectFromSource() =0;

    // ImportForOffsetQuery():
    //
    // Imports the necessary symbols to handle an offset query.  If the necessary imports have already occurred,
    // this method may return S_FALSE and do nothing.
    //
    virtual HRESULT ImportForOffsetQuery(_In_ SvcSymbolKind searchKind,
                                         _In_ ULONG64 offset) =0;

    // ImportForNameQuery():
    //
    // Imports the necessary symbols to handle a name query.  If the necessary imports have already occurred,
    // this method may return S_FALSE and do nothing.  Note that if the name is not specified, this is considered
    // a *FULL* import of all symbols from the underlying source (which may take considerable time).
    //
    virtual HRESULT ImportForNameQuery(_In_ SvcSymbolKind searchKind,
                                       _In_opt_ PCWSTR pwszName) =0;

    // ImportForRegExQuery():
    //
    // Imports the necessary symbols to handle a regex query.  If the necessary imports have already occurred,
    // this method may return S_FALSE and do nothing. 
    //
    virtual HRESULT ImportForRegExQuery(_In_ SvcSymbolKind searchKind,
                                        _In_opt_ PCWSTR pwszRegEx) =0;

    // ImportFailure():
    //
    // Any failure from the import process as a result of import error (and not something like out
    // of memory) should call return ImportFailure(hr) rather than just return hr *AT THE POINT* where the import
    // failure originates.
    //
    virtual HRESULT ImportFailure(_In_ HRESULT hr, _In_opt_ PCWSTR pwszImportMsg = nullptr);

protected:

    // The symbol set which owns this importer.
    SymbolSet *m_pOwningSet;

};

//*************************************************
// DbgHelp Symbol Importer:
//

// SymbolImporter_DbgHelp:
//
// A class which provides the interfaces necessary to import symbols from DbgHelp via the Sym* APIs.
//
class SymbolImporter_DbgHelp : public SymbolImporter
{
public:

    // SymbolImporter_DbgHelp():
    //
    // Constructs a new importer for DbgHelp.
    //
    SymbolImporter_DbgHelp(_In_ SymbolSet *pOwningSet,
                           _In_opt_ PCWSTR pwszSearchPath) :
        SymbolImporter(pOwningSet),
        m_searchPath(pwszSearchPath),
        m_symHandle(NULL),
        m_fullGlobalImport(false)
    {
    }

    // ConnectToSource():
    //
    // Connects the importer to its underlying data source.  If this fails, the importer is not used.
    //
    virtual HRESULT ConnectToSource();

    // DisconnectFromSource():
    //
    // Disconnects the importer from its underlying data source.  After this call, all Import* methods
    // should fail.
    //
    virtual void DisconnectFromSource();

    // ImportForOffsetQuery():
    //
    // Imports the necessary symbols to handle an offset query.  If the necessary imports have already occurred,
    // this method may return S_FALSE and do nothing.
    //
    virtual HRESULT ImportForOffsetQuery(_In_ SvcSymbolKind searchKind,
                                         _In_ ULONG64 offset);

    // ImportForNameQuery():
    //
    // Imports the necessary symbols to handle a name query.  If the necessary imports have already occurred,
    // this method may return S_FALSE and do nothing.  Note that if the name is not specified, this is considered
    // a *FULL* import of all symbols from the underlying source (which may take considerable time).
    //
    virtual HRESULT ImportForNameQuery(_In_ SvcSymbolKind searchKind,
                                       _In_opt_ PCWSTR pwszName);

    // ImportForRegExQuery():
    //
    // Imports the necessary symbols to handle a regex query.  If the necessary imports have already occurred,
    // this method may return S_FALSE and do nothing. 
    //
    virtual HRESULT ImportForRegExQuery(_In_ SvcSymbolKind /*searchKind*/,
                                        _In_opt_ PCWSTR /*pwszRegEx*/)
    {
        return S_FALSE;
    }

private:

    //*************************************************
    // Internal Structures:
    //

    // SymbolQueryInformation:
    //
    // Data passed through DbgHelp to our enumerator callbacks about a particular symbol query.
    //
    struct SymbolQueryInformation
    {
        SvcSymbolKind SearchKind;
        PCWSTR SearchMask;
        bool MaskIsRegEx;
        ULONG64 QueryOffset;
    };

    // SymbolQueryCallbackInformation:
    //
    // The data structure passed to DbgHelp and our callback trampolines to get back into the context
    // of the appropriate importer.
    //
    struct SymbolQueryCallbackInformation
    {
        SymbolQueryInformation Query;
        SymbolImporter_DbgHelp *Importer;
    };

    //*************************************************
    // Internal Methods:
    //

    // InternalConnectToSource():
    //
    // A helper for ConnectToSource.  If this fails, the outer routine will immediately call DisconnectFromSource.
    //
    HRESULT InternalConnectToSource();

    //********************
    // General Symbol Import:
    //

    // ImportFunction():
    //
    // Imports the given function into the symbol builder.
    //
    HRESULT ImportFunction(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    // ImportSymbol():
    //
    // Import the given symbol into the symbol builder.  This is the *ONLY* method that should be called
    // recursively as it adds symbols to the appropriate lookup tables as part of the import process.
    //
    HRESULT ImportSymbol(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    HRESULT ImportSymbol(_In_ PSYMBOL_INFOW pSymInfo, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0)
    {
        return ImportSymbol(pSymInfo->Index, pBuilderId);
    }

    //********************
    // Type Symbol Import:
    //

    // ImportArray():
    //
    // Imports the given array into the symbol builder.
    //
    HRESULT ImportArray(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    // ImportPointer():
    //
    // Imports the given pointer into the symbol builder.
    //
    HRESULT ImportPointer(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    // ImportTypedef():
    //
    // Imports the given typedef into the symbol builder.
    //
    HRESULT ImportTypedef(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    // ImportFunctionType():
    //
    // Imports the given function type into the symbol builder.
    //
    HRESULT ImportFunctionType(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    // ImportUDT():
    //
    // Imports the given UDT into the symbol builder.
    //
    HRESULT ImportUDT(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    // ImportEnum():
    //
    // Imports the given enum into the symbol builder.
    //
    HRESULT ImportEnum(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    // ImportBaseType():
    //
    // Imports the given base type into the symbol builder.
    //
    HRESULT ImportBaseType(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    // ImportTypeSymbol():
    //
    // Imports the given type symbol into the symbol builder.
    //
    HRESULT ImportTypeSymbol(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId = 0);

    // ImportBaseClass():
    //
    // Imports the given base class symbol into the symbol builder.
    //
    HRESULT ImportBaseClass(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId);

    //********************
    // Data Symbol Import:
    //

    // ImportMemberData():
    //
    // Imports a data member (field) into the symbol builder.
    //
    HRESULT ImportMemberData(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId);

    // ImportConstantData():
    //
    // Imports constant data into the symbol builder.
    //
    HRESULT ImportConstantData(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId);

    // ImportDataSymbol():
    //
    // Imports the given data symbol into the symbol builder.
    //
    HRESULT ImportDataSymbol(_In_ ULONG symIndex, _Out_ ULONG64 *pBuilderId, _In_ ULONG64 parentId);

    //********************
    // Other Related:
    //

    // TagMatchesSearchCriteria():
    //
    // Check whether a symbol tag returned from DbgHelp matches the kind of symbol(s) we are looking for.
    //
    static bool TagMatchesSearchCriteria(_In_ ULONG tag, _In_ SvcSymbolKind searchKind);

    //*************************************************
    // DbgHelp Callback Handlers:
    //

    // LegacySymbolCallback():
    //
    // DbgHelp callbacks.
    //
    bool LegacySymbolCallback(_In_ HANDLE /*hProcess*/,
                              _In_ ULONG actionCode,
                              _In_ PVOID pCallbackData)
    {
        switch(actionCode)
        {
            default:
            case CBA_DEBUG_INFO:
            case CBA_DEFERRED_SYMBOL_LOAD_CANCEL:
            case CBA_DEFERRED_SYMBOL_LOAD_COMPLETE:
            case CBA_DEFERRED_SYMBOL_LOAD_FAILURE:
            case CBA_DUPLICATE_SYMBOL:
            case CBA_EVENT:
            case CBA_SET_OPTIONS:
            case CBA_SRCSRV_EVENT:
            case CBA_SYMBOLS_UNLOADED:
                return false;

            case CBA_READ_MEMORY:
            {
                IMAGEHLP_CBA_READ_MEMORY *pReadMemory = reinterpret_cast<IMAGEHLP_CBA_READ_MEMORY *>(pCallbackData);
                return LegacyReadMemory(pReadMemory);
            }
        }
    }

    // LegacySymbolEnumerate():
    //
    // DbgHelp callback from various SymEnum* APIs:
    //
    bool LegacySymbolEnumerate(_In_ SymbolQueryInformation *pQueryInfo,
                               _In_ PSYMBOL_INFOW pSymInfo,
                               _In_ ULONG symbolSize);

    // LegacyReadMemory():
    //
    // Handler for the CBA_READ_MEMORY callback from DbgHelp in trying to find the PDB for the module we
    // are "Symbol Builder Symbols" for.
    //
    bool LegacyReadMemory(_Inout_ IMAGEHLP_CBA_READ_MEMORY *pReadMemory);

    //*************************************************
    // Trampolines:
    //

    // LegacySymbolCallbackBridge():
    //
    // Trampoline callback from DbgHelp.
    //
    static BOOL CALLBACK LegacySymbolCallbackBridge(_In_ HANDLE hProcess,
                                                    _In_ ULONG actionCode,
                                                    _In_ ULONG64 callbackData,
                                                    _In_ ULONG64 userContext)
    {
        SymbolImporter_DbgHelp *pImporter = reinterpret_cast<SymbolImporter_DbgHelp *>(userContext);
        void *pCallbackData = reinterpret_cast<void *>(callbackData);
        return static_cast<BOOL>(pImporter->LegacySymbolCallback(hProcess, actionCode, pCallbackData));
    }

    // LegacySymbolEnumerateBridge():
    //
    // Trampoline callback from DbgHelp.
    //
    static BOOL CALLBACK LegacySymbolEnumerateBridge(_In_ PSYMBOL_INFOW pSymInfo,
                                                     _In_ ULONG symbolSize,
                                                     _In_opt_ PVOID userContext)
    {
        SymbolQueryCallbackInformation *pCallbackInfo = reinterpret_cast<SymbolQueryCallbackInformation *>(userContext);
        return static_cast<BOOL>(pCallbackInfo->Importer->LegacySymbolEnumerate(&(pCallbackInfo->Query),
                                                                                pSymInfo, 
                                                                                symbolSize));
    }

    //*************************************************
    // Data:
    //

    // The handle to DbgHelp for PDB location.
    HANDLE m_symHandle;

    // The search path for symbols
    std::wstring m_searchPath;

    // Information about our module
    ULONG64 m_moduleBase;
    ULONG64 m_moduleSize;

    static constexpr size_t MaxNameLen = 16384;
    static constexpr size_t SymInfoBufSize = sizeof(SYMBOL_INFOW) + MaxNameLen * sizeof(wchar_t);

    // Symbol information buffer
    std::unique_ptr<char[]> m_symInfoBuf;
    PSYMBOL_INFOW m_pSymInfo;

    bool m_fullGlobalImport;
    std::unordered_set<std::wstring> m_nameQueries;
    std::unordered_set<ULONG64> m_addressQueries;
    std::unordered_map<ULONG, ULONG64> m_importedIndexMap;

};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __IMPORTSYMBOLS_H__

