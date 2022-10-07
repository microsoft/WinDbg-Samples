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

//*************************************************
// Overall Symbol Set:
//

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
        ISvcDescription
        >
{
public:

    SymbolSet() :
        m_nextId(0),
        m_demandCreatePointerTypes(true),
        m_demandCreateArrayTypes(true)
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
                               _COM_Outptr_ ISvcSymbol **ppSymbol)
    {
        *ppSymbol = nullptr;
        if (symbolId >= m_symbols.size())
        {
            return E_INVALIDARG;
        }

        Microsoft::WRL::ComPtr<ISvcSymbol> spSymbol = m_symbols[static_cast<size_t>(symbolId)];
        *ppSymbol = spSymbol.Detach();
        return S_OK;
    }
	
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
    HRESULT AddNewSymbol(_In_ BaseSymbol *pBaseSymbol, _Out_ ULONG64 *pUniqueId);

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

    std::vector<Microsoft::WRL::ComPtr<ISvcSymbol>> const& InternalGetSymbols() { return m_symbols; }
    std::vector<ULONG64> const& InternalGetGlobalSymbols() const { return m_globalSymbols; }
    IDebugServiceManager* GetServiceManager() const;
    ISvcMachineArchitecture* GetArchInfo() const;

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

    // The master index of names -> global symbol IDs
    std::unordered_map<std::wstring, ULONG64> m_symbolNameMap;

    // The module for which we are the symbols
    Microsoft::WRL::ComPtr<ISvcModule> m_spModule;

    // Weak pointer back 
    SymbolBuilderProcess *m_pOwningProcess;

    // Tracks the address ranges associated with global symbols.
    SymbolRangeList m_symbolRanges;

    // Configuration options:
    bool m_demandCreatePointerTypes;
    bool m_demandCreateArrayTypes;
    
};

// GlobalSymbolEnumerator:
//
// An enumerator which enumerates all of the global symbols within a symbol set.
//
class GlobalEnumerator :
    public Microsoft::WRL::RuntimeClass<
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

        //
        // NOTE: There may be gaps in our id <-> symbol mapping because of deleted symbols or other
        //       unused IDs.  We cannot return nullptr.  Any such empty slot in our internal list
        //       needs to be skipped.
        //
        auto&& symbols = m_spSymbolSet->InternalGetSymbols();

        while (m_pos < symbols.size())
        {
            Microsoft::WRL::ComPtr<ISvcSymbol> spSymbol = symbols[m_pos];
            ++m_pos;
            if (spSymbol.Get() != nullptr)
            {
                *ppSymbol = spSymbol.Detach();
                return S_OK;
            }
        }

        return E_BOUNDS;
    }

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ SymbolSet *pSymbolSet)
    {
        m_spSymbolSet = pSymbolSet;
        return Reset();
    }

private:

    size_t m_pos;
    Microsoft::WRL::ComPtr<SymbolSet> m_spSymbolSet;

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
