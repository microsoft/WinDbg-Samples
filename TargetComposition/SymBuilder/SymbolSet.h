//**************************************************************************
//
// SymbolSet.h
//
// The header for our implementation of a "symbol set".  A "symbol set" is an
// abstraction for the available symbols for a given module.  It is a set of
// "stacked" interfaces which implements progressively more functionality depending
// on the complexity of the symbol implementation.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __SYMBOLSET_H__
#define __SYMBOLSET_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

class SymbolBuilderManager;

//*************************************************
// Overall Symbol Set:
//

// PublicList:
//
// Provides a list of addresses in sorted order which can be binary searched for the "nearest" symbol(s) to
// a given address.
//
class PublicList
{
public:

    using SymbolList = std::vector<ULONG64>;

    // PublicList():
    //
    // Creates a new public symbol list.  Initially, there are no symbols in the list.
    //
    PublicList()
    {
    }

    // FindNearestSymbols():
    //
    // Find the list of symbols which are closest to a given address.  If such can be found, true is returned and
    // an output pointer to the list of symbol ids is passed in 'pSymbolList'
    //
    bool FindNearestSymbols(_In_ ULONG64 address, _Out_ SymbolList const** pSymbolList);

    // AddSymbol():
    //
    // Adds a public symbol to the list.
    //
    HRESULT AddSymbol(_In_ ULONG64 address, _In_ ULONG64 symbol);

    // RemoveSymbol():
    //
    // Removes a symbol from the list.
    //
    HRESULT RemoveSymbol(_In_ ULONG64 address, _In_ ULONG64 symbol);

private:
    
    struct Address
    {
        ULONG64 Addr;
        SymbolList Symbols;
    };

    // RemoveSymbolFromList():
    //
    // Removes a symbol from the given list.
    //
    void RemoveSymbolFromList(_Inout_ SymbolList& list, _In_ ULONG64 symbol)
    {
        for (auto it = list.begin(); it != list.end(); ++it)
        {
            if (*it == symbol)
            {
                list.erase(it);
                break;
            }
        }
    }

    std::vector<Address> m_addresses;

};

// SymbolRangeList:
//
// Provides a list of address ranges in sorted order which can be binary searched for a given symbol or set
// of symbols.
//
class SymbolRangeList
{
public:

    using SymbolList = std::vector<ULONG64>;

    // SymbolRangeList():
    //
    // Creates a new symbol range list covering the half-open set [start, end).  Initially, there
    // are no symbols in the list.
    //
    SymbolRangeList()
    {
    }

    // FindSymbols():
    //
    // Find the list of symbols which overlap a given address.  If such can be found, true is returned and
    // an output pointer to the list of symbol ids is passed in 'pSymbolList'
    //
    bool FindSymbols(_In_ ULONG64 address, _Out_ SymbolList const** pSymbolList);

    // AddSymbol():
    //
    // Adds a symbol to the range list.  The symbol's address range is given by the half-open set 
    // [start, end).  
    //
    HRESULT AddSymbol(_In_ ULONG64 start, _In_ ULONG64 end, _In_ ULONG64 symbol);

    // RemoveSymbol():
    //
    // Removes a symbol from the range list.
    //
    HRESULT RemoveSymbol(_In_ ULONG64 start, _In_ ULONG64 end, _In_ ULONG64 symbol);

private:
    
    struct AddressRange
    {
        ULONG64 Start;
        ULONG64 End;
        SymbolList Symbols;
    };

    // RemoveSymbolFromList():
    //
    // Removes a symbol from the given list.
    //
    void RemoveSymbolFromList(_Inout_ SymbolList& list, _In_ ULONG64 symbol)
    {
        for (auto it = list.begin(); it != list.end(); ++it)
        {
            if (*it == symbol)
            {
                list.erase(it);
                break;
            }
        }
    }

    std::vector<AddressRange> m_ranges;

};

// SymbolSet:
//
// Our representation for our "in memory constructed" symbols for a given module within a given 
// process context.
//
class SymbolSet : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcSymbolSet,
        ISvcSymbolSetSimpleNameResolution,
        ISvcSymbolSetScopeResolution,
        ISvcDescription
        >
{
public:

    //
    // ScopeBoundIdFlag: Indicates that the ID is a scope binding and not an index
    //                   into our master list of symbols.
    //
    static constexpr ULONG64 ScopeBoundIdFlag = (1ull << 63);

    SymbolSet() :
        m_nextId(0),
        m_demandCreatePointerTypes(true),
        m_demandCreateArrayTypes(true),
        m_cacheInvalidationDisabled(false)
    {
    }

    //*************************************************
    // ISvcSymbolSet:
    //

    //
    // GetSymbolById():
    //
    // Returns the symbol for a given symbol ID (returned by ISvcSymbol::GetId)
    //
    IFACEMETHOD(GetSymbolById)(_In_ ULONG64 symbolId,
                               _COM_Outptr_ ISvcSymbol **ppSymbol);
	
    //
    // EnumerateAllSymbols():
    //
    // Enumerates all symbols in the set.
    //
    IFACEMETHOD(EnumerateAllSymbols)(_COM_Outptr_ ISvcSymbolSetEnumerator **ppEnumerator);

    //*************************************************
    // ISvcSymbolSetSimpleNameResolution:
    //

    // FindSymbolByName():
    //
    // Finds symbolic information for a given name.  The method fails if the symbol cannot be located.
    //
    IFACEMETHOD(FindSymbolByName)(_In_ PCWSTR symbolName, _COM_Outptr_ ISvcSymbol **ppSymbol);

    // FindSymbolByOffset():
    //
    // Finds symbolic information for a given offset.  If the "exactMatchOnly" parameter is true, this will only return
    // a symbol which is exactly at the offset given.  If the "exactMatchOnly" parameter is false, this will return the 
    // closest symbol before the given offset.  If no such symbol can be found, the method fails.
    //
    // Note that if a given symbol (e.g.: a function) has multiple disjoint address ranges and one of those address
    // ranges has been moved to *BELOW* the base address of the symbol, the returned "symbolOffset" may be
    // interpreted as a signed value (and S_FALSE should be returned in such a case).  This can be confirmed
    // by querying the symbol for its address ranges.
    //
    IFACEMETHOD(FindSymbolByOffset)(_In_ ULONG64 moduleOffset,
                                    _In_ bool exactMatchOnly,
                                    _COM_Outptr_ ISvcSymbol **ppSymbol,
                                    _Out_ ULONG64 *pSymbolOffset);

    //*************************************************
    // ISvcSymbolSetScopeResolution:
    //

    // GetGlobalScope():
    //
    // Returns a scope representing the global scope of the module the symbol set represents.  This 
    // may be an aggregation of other symbols one could discover through fully enumerating the symbol
    // set.
    //
    IFACEMETHOD(GetGlobalScope)(_COM_Outptr_ ISvcSymbolSetScope **ppScope);

    // FindScopeByOffset():
    //
    // Finds a scope by an offset within the image (which is assumed to be an offset within
    // a function or other code area)
    //
    IFACEMETHOD(FindScopeByOffset)(_In_ ULONG64 moduleOffset,
                                   _COM_Outptr_ ISvcSymbolSetScope **ppScope);

    // FindScopeFrame():
    //
    // Finds a scope by the unwound context record for a stack frame.
    //
    IFACEMETHOD(FindScopeFrame)(_In_ ISvcProcess *pProcess,
                                _In_ ISvcRegisterContext *pRegisterContext,
                                _COM_Outptr_ ISvcSymbolSetScopeFrame **ppScopeFrame);

    //*************************************************
    // ISvcDescription:
    //

    // GetDescription():
    //
    // Gets a description of the object on which the interface exists.  This is intended for short textual
    // display in some UI element.
    //
    IFACEMETHOD(GetDescription)(_Out_ BSTR *pObjectDescription)
    {
        //
        // Give the symbol set a description so that commands in the debugger (e.g.: lm) can show something
        // useful for what kind of symbols are loaded.
        //
        *pObjectDescription = SysAllocString(L"Symbol Builder Symbols");
        return (*pObjectDescription == nullptr ? E_OUTOFMEMORY : S_OK);
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initialize a new symbol set.
    //
    HRESULT RuntimeClassInitialize(_In_ ISvcModule *pModule, 
                                   _In_ SymbolBuilderProcess *pOwningProcess,
                                   _In_ bool addBasicCTypes = true)
    {
        HRESULT hr = S_OK;

        m_spModule = pModule;
        m_pOwningProcess = pOwningProcess;

        if (addBasicCTypes)
        {
            hr = AddBasicCTypes();
        }

        return hr;
    }

    // AddBasicCTypes():
    //
    // Called to add a basic set of C defined types to the symbol set.
    //
    HRESULT AddBasicCTypes();

    // AddNewSymbol():
    //
    // Called to add a new symbol to our management lists and assign it a unique id.
    //
    HRESULT AddNewSymbol(_In_ BaseSymbol *pBaseSymbol, _Out_ ULONG64 *pUniqueId, _In_ ULONG64 reservedId = 0);

    // DeleteExistingSymbol():
    //
    // Called to delete a symbol from our management lists.  If anyone still has a handle to the unique id
    // associated with that symbol, their symbol becomes invalid.  The symbol will no longer resolve.
    //
    HRESULT DeleteExistingSymbol(_In_ ULONG64 uniqueId);

    // InvalidateExternalCaches():
    //
    // Fires an event notification to any listeners indicating that their caching of symbols from this
    // set should be invalidated.  This is required *ANY TIME* we *CHANGE* the underlying types, fields, etc...
    // that we project upwards.
    //
    HRESULT InvalidateExternalCaches();

    // FindTypeByName():
    //
    // Finds a type by name.  If the symbol set is allowed to demand create pointer and array types and
    // 'allowAutoCreations' is true, this will do so if need be.
    //
    HRESULT FindTypeByName(_In_ std::wstring const& typeName,
                           _Out_ ULONG64 *pTypeId,
                           _COM_Outptr_opt_ BaseTypeSymbol **ppTypeSymbol,
                           _In_ bool allowAutoCreations = true);

    // GetScopeBindingId():
    //
    // Gets a new ID for a scope binding.
    //
    HRESULT GetScopeBindingId(_In_ ULONG64 variableId,
                              _In_ ULONG64 moduleOffset,
                              _Out_ ULONG64 *pId)
    {
        auto fn = [&]()
        {
            m_scopeBindings.push_back( std::pair<ULONG64, ULONG64> { variableId, moduleOffset } );
            *pId = ScopeBoundIdFlag | (m_scopeBindings.size() - 1);
            return S_OK;
        };
        return ConvertException(fn);
    }
	
    // SetImporter():
    //
    // Sets an "on demand" importer to use for this symbol set.
    //
    void SetImporter(_In_ std::unique_ptr<SymbolImporter>&& importer)
    {
        m_spImporter = std::move(importer);
    }	

    // SetCacheInvalidationDisable():
    //
    // Turns on / off the ability to send cache invalidation notifications.
    //
    void SetCacheInvalidationDisable(_In_ bool disable)
    {
        m_cacheInvalidationDisabled = disable;
    }

    //*************************************************
    // Internal Accessors:
    //

    // InternalGetSymbol():
    //
    // Gets a symbol by its unique ID without adding any reference count.  The caller must be extremely
    // careful in its usage of this symbol.
    //
    BaseSymbol *InternalGetSymbol(_In_ ULONG64 uniqueId) const
    {
        if (uniqueId >= m_symbols.size())
        {
            return nullptr;
        }

        ISvcSymbol *pSymbol = m_symbols[static_cast<size_t>(uniqueId)].Get();
        return static_cast<BaseSymbol *>(pSymbol);
    }

    // InternalGetSymbolByName():
    //
    // Finds a symbol by its fully qualified name.  Returns 0 as a symbol id if no such symbol can be found.
    //
    ULONG64 InternalGetSymbolIdByName(_In_ std::wstring const& symbolName)
    {
        auto it = m_symbolNameMap.find(symbolName);
        if (it == m_symbolNameMap.end())
        {
            return 0;
        }
        return it->second;
    }

    // InternalAddSymbolRange():
    //
    // Adds a mapping of [start, end) as a half-open address range to the existing symbol.
    //
    HRESULT InternalAddSymbolRange(_In_ ULONG64 start, _In_ ULONG64 end, _In_ ULONG64 symbol)
    {
        return m_symbolRanges.AddSymbol(start, end, symbol);
    }

    // InternalRemoveSymbolRange():
    //
    // Removes a mapping of [start, end) as a half-open address range from the existing symbol.
    //
    HRESULT InternalRemoveSymbolRange(_In_ ULONG64 start, _In_ ULONG64 end, _In_ ULONG64 symbol)
    {
        return m_symbolRanges.RemoveSymbol(start, end, symbol);
    }

    // InternalAddPublicSymbol():
    //
    // Adds a mapping of [address] to the existing symbol.
    //
    HRESULT InternalAddPublicSymbol(_In_ ULONG64 address, _In_ ULONG64 symbol)
    {
        return m_publicAddresses.AddSymbol(address, symbol);
    }

    // InternalRemovePublicSymbol():
    //
    // Removes a mapping of [address] from the existing symbol.
    //
    HRESULT InternalRemovePublicSymbol(_In_ ULONG64 address, _In_ ULONG64 symbol)
    {
        return m_publicAddresses.RemoveSymbol(address, symbol);
    }

    std::vector<Microsoft::WRL::ComPtr<ISvcSymbol>> const& InternalGetSymbols() { return m_symbols; }
    std::vector<ULONG64> const& InternalGetGlobalSymbols() const { return m_globalSymbols; }
    IDebugServiceManager* GetServiceManager() const;
    ISvcMachineArchitecture* GetArchInfo() const;
    SymbolBuilderManager* GetSymbolBuilderManager() const;
    ISvcModule* GetModule() const;
    SymbolBuilderProcess* GetOwningProcess() const { return m_pOwningProcess; }

    // HasImporter/GetImporter():
    //
    // Indicates whether or not we have an underlying symbol importer / gets it.
    //
    bool HasImporter() const { return m_spImporter.get() != nullptr; }
    SymbolImporter *GetImporter() const { return m_spImporter.get(); }
    ULONG64 ReserveUniqueId() { return GetUniqueId(); }

private:

    ULONG64 GetUniqueId() 
    {
        return ++m_nextId;
    }

    // The next "unique id" that we will hand out when a new symbol is constructed
    ULONG64 m_nextId;

    // The master index of all symbols by their assigned unique id.
    std::vector<Microsoft::WRL::ComPtr<ISvcSymbol>> m_symbols;

    // The master index of "global" symbols
    std::vector<ULONG64> m_globalSymbols;

    // Scope bindings: pair< variable id, moduleOffset > 
    std::vector<std::pair<ULONG64, ULONG64>> m_scopeBindings;

    // The master index of names -> global symbol IDs
    std::unordered_map<std::wstring, ULONG64> m_symbolNameMap;

    // The module for which we are the symbols
    Microsoft::WRL::ComPtr<ISvcModule> m_spModule;

    // Weak pointer back 
    SymbolBuilderProcess *m_pOwningProcess;

    // Tracks the address ranges associated with global symbols.
    SymbolRangeList m_symbolRanges;

    // Tracks the addresses associated with public symbols.
    PublicList m_publicAddresses;

    // If we have an importer that will automatically pull in underlying symbols, this points
    // to it. 
    std::unique_ptr<SymbolImporter> m_spImporter;

    // An indication of whether cache invalidation is disabled or not.
    bool m_cacheInvalidationDisabled;

    // Configuration options:
    bool m_demandCreatePointerTypes;
    bool m_demandCreateArrayTypes;
    
};

// BaseSymbolEnumerator:
//
// A base class for symbol enumeration which provides certain helpers.
//
class BaseSymbolEnumerator :
    public Microsoft::WRL::Implements<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcSymbolSetEnumerator
        >
{
public:

    //*************************************************
    // ISvcSymbolSetEnumerator:
    //

    // Reset():
    //
    // Resets the position of the enumerator.
    //
    IFACEMETHOD(Reset)()
    {
        m_pos = 0;
        return S_OK;
    }

    // GetNext():
    //
    // Gets the next symbol from the enumerator.
    //
    IFACEMETHOD(GetNext)(_COM_Outptr_ ISvcSymbol **ppSymbol)
    {
        *ppSymbol = nullptr;
        return E_BOUNDS;
    }

    //*************************************************
    // Internal APIs:
    //

    SymbolSet *InternalGetSymbolSet() const { return m_spSymbolSet.Get(); }

protected:

    //*************************************************
    // Internal APIs:
    //

    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet)
    {
        m_spSymbolSet = pSymbolSet;
        m_searchKind = SvcSymbol;
        m_pSearchInfo = nullptr;
        return Reset();
    }

    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet,
                           _In_ SvcSymbolKind symKind,
                           _In_opt_ PCWSTR pwszName,
                           _In_opt_ SvcSymbolSearchInfo *pSearchInfo)
    {
        auto fn = [&]()
        {
            m_spSymbolSet = pSymbolSet;
            m_searchKind = symKind;
            m_pSearchInfo = nullptr;
            if (pwszName != nullptr)
            {
                m_searchName = pwszName;
            }
            if (pSearchInfo != nullptr)
            {
                size_t dataSize = pSearchInfo->HeaderSize + pSearchInfo->InfoSize;
                m_searchData.reset(new(std::nothrow) unsigned char[dataSize]);
                if (m_searchData.get() == nullptr)
                {
                    return E_OUTOFMEMORY;
                }
                m_pSearchInfo = reinterpret_cast<SvcSymbolSearchInfo *>(m_searchData.get());
                memcpy(m_pSearchInfo, pSearchInfo, dataSize);
            }
            return Reset();
        };
        return ConvertException(fn);
    }

    // SymbolMatchesSearchCriteria():
    //
    // Checks whether a given symbol from the scope matches other search criteria (name, kind,
    // type kind, etc...)
    //
    bool SymbolMatchesSearchCriteria(_In_ BaseSymbol *pSymbol) const
    {
        if (m_searchKind != SvcSymbol && pSymbol->InternalGetKind() != m_searchKind)
        {
            return false;
        }

        if (!m_searchName.empty())
        {
            wchar_t const *pMatchName = 
                (m_pSearchInfo != nullptr && (m_pSearchInfo->SearchOptions & 
                                              SvcSymbolSearchQualifiedName) != 0) ?
                    pSymbol->InternalGetQualifiedName().c_str() :
                    pSymbol->InternalGetName().c_str();

            if (!pMatchName || wcscmp(pMatchName, m_searchName.c_str()) != 0)
            {
                return false;
            }
        }

        if (m_searchKind == SvcSymbolType && m_pSearchInfo != nullptr &&
            m_pSearchInfo->InfoSize >= FIELD_OFFSET(SvcTypeSearchInfo, SearchType) + 
                                       sizeof (SvcSymbolTypeKind))
        {
            SvcTypeSearchInfo *pTypeSearchInfo = reinterpret_cast<SvcTypeSearchInfo *>(
                reinterpret_cast<char *>(m_pSearchInfo) + m_pSearchInfo->HeaderSize
                );

            BaseTypeSymbol *pTypeSymbol = static_cast<BaseTypeSymbol *>(pSymbol);
            if (pTypeSymbol->InternalGetTypeKind() != pTypeSearchInfo->SearchType)
            {
                return false;
            }
        }

        return true;
    }

    size_t m_pos;
    Microsoft::WRL::ComPtr<SymbolSet> m_spSymbolSet;

    SvcSymbolKind m_searchKind;
    std::wstring m_searchName;
    SvcSymbolSearchInfo *m_pSearchInfo;
    std::unique_ptr<unsigned char[]> m_searchData;
};

// GlobalSymbolEnumerator:
//
// An enumerator which enumerates all of the global symbols within a symbol set.
//
class GlobalEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseSymbolEnumerator
        >
{
public:

    //*************************************************
    // ISvcSymbolSetEnumerator:
    //

    // GetNext():
    //
    // Gets the next symbol from the enumerator.
    //
    IFACEMETHOD(GetNext)(_COM_Outptr_ ISvcSymbol **ppSymbol)
    {
        *ppSymbol = nullptr;

        //
        // NOTE: There may be gaps in our id <-> symbol mapping because of deleted symbols or other
        //       unused IDs.  We cannot return nullptr.  Any such empty slot in our internal list
        //       needs to be skipped.
        //
        auto&& symbols = m_spSymbolSet->InternalGetSymbols();

        while (m_pos < symbols.size())
        {
            ISvcSymbol *pSymbol = symbols[m_pos].Get();
            ++m_pos;
            if (pSymbol != nullptr)
            {
                BaseSymbol *pBaseSymbol = static_cast<BaseSymbol *>(pSymbol);
                if (SymbolMatchesSearchCriteria(pBaseSymbol))
                {
                    Microsoft::WRL::ComPtr<ISvcSymbol> spSymbol = pSymbol;
                    *ppSymbol = spSymbol.Detach();
                    return S_OK;
                }
            }
        }

        return E_BOUNDS;
    }

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet)
    {
        return BaseInitialize(pSymbolSet);
    }

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ SvcSymbolKind symKind,
                                   _In_opt_ PCWSTR pwszName,
                                   _In_opt_ SvcSymbolSearchInfo *pSearchInfo)
    {
        return BaseInitialize(pSymbolSet, symKind, pwszName, pSearchInfo);
    }
};

// GlobalScope:
//
// A representation of the global scope.
//
class GlobalScope :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcSymbolSetScope,
        ISvcSymbolChildren
        >
{
public:

    //*************************************************
    // ISvcSymbolSetScope:

    // EnumerateArguments():
    //
    // If the scope is a function scope (or is a lexical sub-scope of a function), this enumerates
    // the arguments of the function.  
    //
    // This will fail for a scope for which arguments are inappropriate.
    //
    IFACEMETHOD(EnumerateArguments)(_COM_Outptr_ ISvcSymbolSetEnumerator **ppEnum)
    {
        //
        // There are no "arguments" in the global scope.
        //
        *ppEnum = nullptr;
        return E_FAIL;
    }

    // EnumerateLocals():
    //
    // Enumerates the locals within the scope.  
    //
    IFACEMETHOD(EnumerateLocals)(_COM_Outptr_ ISvcSymbolSetEnumerator **ppEnum)
    {
        //
        // There are no "local variables" in the global scope.
        //
        *ppEnum = nullptr;
        return E_FAIL;
    }

    //*************************************************
    // ISvcSymbolSetChildren:
    //

    // EnumerateChildren():
    //
    // Enumerates all children of the given symbol.
    //
    IFACEMETHOD(EnumerateChildren)(_In_ SvcSymbolKind kind,
                                   _In_opt_z_ PCWSTR pwszName,
                                   _In_opt_ SvcSymbolSearchInfo *pSearchInfo,
                                   _COM_Outptr_ ISvcSymbolSetEnumerator **ppChildEnum);

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet)
    {
        m_spSymbolSet = pSymbolSet;
        return S_OK;
    }

private:

    Microsoft::WRL::ComPtr<SymbolSet> m_spSymbolSet;

};

// BaseScope:
//
// The base of a scope or a scope frame for a function.  Note that we only support function scopes.
// It is entirely possible to have a scope which represents a deeply nested lexical scope, etc...
// This base class implements all necessary scope functionality outside of that required 
// specifically for a scope frame.  It also provides some helpers for a scope frame.
//
class BaseScope :
    public Microsoft::WRL::Implements<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcSymbolSetScope,
        ISvcSymbolChildren
        >
{
public:

    //*************************************************
    // ISvcSymbolSetScope:

    // EnumerateArguments():
    //
    // If the scope is a function scope (or is a lexical sub-scope of a function), this enumerates
    // the arguments of the function.  
    //
    // This will fail for a scope for which arguments are inappropriate.
    //
    IFACEMETHOD(EnumerateArguments)(_COM_Outptr_ ISvcSymbolSetEnumerator **ppEnum);

    // EnumerateLocals():
    //
    // Enumerates the locals within the scope.  
    //
    IFACEMETHOD(EnumerateLocals)(_COM_Outptr_ ISvcSymbolSetEnumerator **ppEnum);

    //*************************************************
    // ISvcSymbolSetChildren:
    //

    // EnumerateChildren():
    //
    // Enumerates all children of the given symbol.
    //
    IFACEMETHOD(EnumerateChildren)(_In_ SvcSymbolKind kind,
                                   _In_opt_z_ PCWSTR pwszName,
                                   _In_opt_ SvcSymbolSearchInfo *pSearchInfo,
                                   _COM_Outptr_ ISvcSymbolSetEnumerator **ppChildEnum);

    //*************************************************
    // Internal APIs:
    //

    SymbolSet *InternalGetSymbolSet() const { return m_spSymbolSet.Get(); }
    FunctionSymbol *InternalGetFunction() const { return m_spFunction.Get(); }
    ULONG64 InternalGetFunctionOffset() const { return m_srelOffset; }
    ISvcProcess *InternalGetScopeFrameProcess() const { return m_spFrameProcess.Get(); }
    ISvcRegisterContext *InternalGetScopeFrameContext() const { return m_spFrameContext.Get(); }

protected:

    //*************************************************
    // Internal APIs:
    //

    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet,
                           _In_ FunctionSymbol *pFunction,
                           _In_ ULONG64 srelOffset,
                           _In_opt_ ISvcProcess *pFrameProcess = nullptr,
                           _In_opt_ ISvcRegisterContext *pFrameContext = nullptr)
    {
        HRESULT hr = S_OK;

        m_spSymbolSet = pSymbolSet;
        m_spFunction = pFunction;
        m_srelOffset = srelOffset;
        m_spFrameProcess = pFrameProcess;

        if (pFrameContext != nullptr)
        {
            IfFailedReturn(pFrameContext->Duplicate(&m_spFrameContext));
        }

        return hr;
    }

    // Our owning symbol set
    Microsoft::WRL::ComPtr<SymbolSet> m_spSymbolSet;

    // The function for which we are a scope.
    Microsoft::WRL::ComPtr<FunctionSymbol> m_spFunction;

    // The offset relative to the base of m_spFunction for the @pc of this scope
    ULONG64 m_srelOffset;

    // Information for a scope frame.
    Microsoft::WRL::ComPtr<ISvcProcess> m_spFrameProcess;
    Microsoft::WRL::ComPtr<ISvcRegisterContext> m_spFrameContext;

};

// Scope:
//
// Represents a scope (detached from a particular register context)
//
class Scope :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseScope
        >
{
public:

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ FunctionSymbol *pFunction,
                                   _In_ ULONG64 srelOffset)
    {
        return BaseInitialize(pSymbolSet, pFunction, srelOffset);
    }
};

// ScopeFrame:
//
// Represents a scope (attached to a particular register context; e.g.: from a stack frame)
//
class ScopeFrame :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseScope,
        ISvcSymbolSetScopeFrame
        >
{
public:

    //*************************************************
    // ISvcSymbolSetScopeFrame:
    //

    // GetContext():
    //
    // Gets the context for the scope frame.
    //
    IFACEMETHOD(GetContext)(_In_ SvcContextFlags /*contextFlags*/,
                            _COM_Outptr_ ISvcRegisterContext **ppRegisterContext)
    {
        *ppRegisterContext = nullptr;

        if (InternalGetScopeFrameContext() == nullptr)
        {
            return E_FAIL;
        }

        Microsoft::WRL::ComPtr<ISvcRegisterContext> spRegisterContext = InternalGetScopeFrameContext();
        *ppRegisterContext = spRegisterContext.Detach();
        return S_OK;
    }

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet,
                                   _In_ FunctionSymbol *pFunction,
                                   _In_ ULONG64 srelOffset,
                                   _In_opt_ ISvcProcess *pFrameProcess,
                                   _In_ ISvcRegisterContext *pFrameContext)
    {
        return BaseInitialize(pSymbolSet, pFunction, srelOffset, pFrameProcess, pFrameContext);
    }
};

// ScopeEnumerator:
//
// A symbol enumerator for a scope.
//
class ScopeEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        BaseSymbolEnumerator
        >
{
public:

    //*************************************************
    // ISvcSymbolSetEnumerator:
    //

    // GetNext():
    //
    // Gets the next symbol from the enumerator.
    //
    IFACEMETHOD(GetNext)(_COM_Outptr_ ISvcSymbol **ppSymbol);

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ BaseScope *pScope)
    {
        m_spScope = pScope;
        return BaseInitialize(pScope->InternalGetSymbolSet());
    }

    HRESULT RuntimeClassInitialize(_In_ BaseScope *pScope,
                                   _In_ SvcSymbolKind symKind,
                                   _In_opt_ PCWSTR pwszName,
                                   _In_opt_ SvcSymbolSearchInfo *pSearchInfo)
    {
        m_spScope = pScope;
        return BaseInitialize(pScope->InternalGetSymbolSet(), symKind, pwszName, pSearchInfo);
    }

private:

    Microsoft::WRL::ComPtr<ISvcSymbolSetScope> m_spScope;

};

// SymbolCacheInvalidateArguments:
//
// An event arguments class for DEBUG_SVCEVENT_SYMBOLCACHEINVALIDATE.
//
class SymbolCacheInvalidateArguments :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcEventArgumentsSymbolCacheInvalidate
        >
{
public:

    //*************************************************
    // ISvcEventArgumentsSymbolCacheInvalidate:
    //

    // GetSymbolsInformation():
    //
    // Gets information about the module and symbol set for which cache invalidation should occur.
    //
    IFACEMETHOD(GetSymbolsInformation)(_COM_Outptr_ ISvcModule **ppModule,
                                       _COM_Outptr_ ISvcSymbolSet **ppSymbolSet)
    {
        Microsoft::WRL::ComPtr<ISvcModule> spModule = m_spModule;
        Microsoft::WRL::ComPtr<ISvcSymbolSet> spSymbolSet = m_spSymbolSet;

        *ppModule = spModule.Detach();
        *ppSymbolSet = spSymbolSet.Detach();
        return S_OK;
    }

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ ISvcModule *pModule,
                                   _In_ ISvcSymbolSet *pSymbolSet)
    {
        m_spModule = pModule;
        m_spSymbolSet = pSymbolSet;
        return S_OK;
    }


private:

    Microsoft::WRL::ComPtr<ISvcModule> m_spModule;
    Microsoft::WRL::ComPtr<ISvcSymbolSet> m_spSymbolSet;
};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger


#endif // __SYMBOLSET_H__
