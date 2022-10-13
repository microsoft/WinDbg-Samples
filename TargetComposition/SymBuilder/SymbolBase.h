//**************************************************************************
//
// SymbolBase.h
//
// The header for our base implementation of symbols within a "symbol set".  A "symbol set" is an
// abstraction for the available symbols for a given module.  It is a set of
// "stacked" interfaces which implements progressively more functionality depending
// on the complexity of the symbol implementation.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __SYMBOLBASE_H__
#define __SYMBOLBASE_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

class SymbolSet;                        // Forward declaration from SymbolSet.h
class SymbolBuilderProcess;             // Forward declaration from SymManager.h

//*************************************************
// Base Symbols:
//

// BaseSymbol:
//
// Our "base class" for all symbols.
//
class BaseSymbol :
    public Microsoft::WRL::Implements<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcSymbol,
        ISvcSymbolInfo,
        ISvcSymbolChildren
        >
{
public:

    //*************************************************
    // ISvcSymbol:
    //

    // GetSymbolKind():
    //
    // Gets the kind of symbol that this is (e.g.: a field, a base class, a type, etc...)
    //
    IFACEMETHOD(GetSymbolKind)(_Out_ SvcSymbolKind *pKind)
    {
        *pKind = m_kind;
        return S_OK;
    }

    // GetName():
    //
    // Gets the name of the symbol (e.g.: MyMethod)
    //
    IFACEMETHOD(GetName)(_Out_ BSTR *pSymbolName)
    {
        *pSymbolName = nullptr;
        if (!m_name.empty())
        {
            *pSymbolName = SysAllocString(m_name.c_str());
            return (*pSymbolName == nullptr ? E_OUTOFMEMORY : S_OK);
        }
        return E_NOT_SET;
    }

    // GetQualifiedName():
    //
    // Gets the qualified name of the symbol (e.g.: MyNamespace::MyClass::MyMethod)
    //
    IFACEMETHOD(GetQualifiedName)(_Out_ BSTR *pQualifiedName)
    {
        *pQualifiedName = nullptr;
        if (!m_qualifiedName.empty())
        {
            *pQualifiedName = SysAllocString(m_qualifiedName.c_str());
            return (*pQualifiedName == nullptr ? E_OUTOFMEMORY : S_OK);
        }
        return GetName(pQualifiedName);
    }

    // GetId():
    //
    // Gets an identifier for the symbol which can be used to retrieve the same symbol again.  The
    // identifier is opaque and has semantics only to the underlying symbol set.
    //
    IFACEMETHOD(GetId)(_Out_ ULONG64 *pId)
    {
        *pId = m_id;
        return S_OK;
    }

    // GetOffset():
    //
    // Gets the offset of the symbol (if said symbol has such).  Note that if the symbol has multiple
    // disjoint address ranges associated with it, this method may return S_FALSE to indicate that the symbol
    // does not necessarily have a simple "base address" for an offset.
    //
    IFACEMETHOD(GetOffset)(_Out_ ULONG64 * /*pSymbolOffset*/)
    {
        //
        // Some derived class is expected to override this!
        //
        return E_NOT_SET;
    }

    //*************************************************
    // ISvcSymbolInfo:
    //

    // GetType():
    //
    // Gets the type of the symbol.
    //
    IFACEMETHOD(GetType)(_COM_Outptr_ ISvcSymbol **ppSymbolType)
    {
        //
        // Some derived class may need to override this.
        //
        *ppSymbolType = nullptr;
        return E_NOTIMPL;
    }

    // GetLocation()
    //
    // Gets the location of the symbol.
    //
    IFACEMETHOD(GetLocation)(_Out_ SvcSymbolLocation * /*pLocation*/)
    {
        //
        // Some derived class may need to override this.
        //
        return E_NOTIMPL;
    }

    // GetValue()
    //
    // Gets the value of a constant value symbol.  GetLocation will return an indication that the symbol
    // has a constant value.
    //
    // If this method is called on a symbol without a constant value, it will fail.
    //
    IFACEMETHOD(GetValue)(_Out_ VARIANT * /*pValue*/)
    {
        //
        // Some derived class may need to override this.
        //
        return E_NOTIMPL;
    }

    // GetAttribute():
    //
    // Gets a simple attribute of the symbol.  The type of a given attribute is defined by the attribute
    // itself.   If the symbol cannot logically provide a value for the attribute, E_NOT_SET should be returned.
    // If the provider does not implement the attribute for any symbol, E_NOTIMPL should be returned.
    //
    IFACEMETHOD(GetAttribute)(_In_ SvcSymbolAttribute /*attr*/,
                              _Out_ VARIANT * /*pAttributeValue*/)
    {
        //
        // Some derived class may need to override this.
        //
        return E_NOTIMPL;
    }

    //*************************************************
    // ISvcSymbolChildren:
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

    HRESULT BaseInitialize(_In_ SymbolSet *pSymbolSet,
                           _In_ SvcSymbolKind kind,
                           _In_ ULONG64 parentId,
                           _In_opt_ PCWSTR pwszSymbolName = nullptr,
                           _In_opt_ PCWSTR pwszQualifiedName = nullptr,
                           _In_ bool newSymbol = true,
                           _In_ ULONG64 id = 0)
    {
        //
        // We cannot let a C++ exception escape.
        //
        auto fn = [&]()
        {
            m_pSymbolSet = pSymbolSet;
            m_parentId = parentId;
            m_kind = kind;
            if (pwszSymbolName != nullptr)
            {
                m_name = pwszSymbolName;
            }
            if (pwszQualifiedName != nullptr)
            {
                m_qualifiedName = pwszQualifiedName;
            }
            if (newSymbol)
            {
                return InitializeNewSymbol(id);
            }
            else
            {
                if (id == 0)
                {
                    return E_INVALIDARG;
                }
                m_id = id;
                return S_OK;
            }
        };
        return ConvertException(fn);
    }

    // NotifyDependentChange():
    //
    // Called when something this symbol is dependent upon changes (e.g.: layout, etc...).  Derived classes should
    // override this method, provide a behavior, and then call the base class.
    //
    virtual HRESULT NotifyDependentChange();

    // IsGlobal():
    //
    // Returns whether or not the symbol is "global".  A global symbol will be indexed by name.  Child symbols
    // will not.
    //
    virtual bool IsGlobal() const
    {
        switch(InternalGetKind())
        {
            case SvcSymbolType:
            case SvcSymbolData:
            case SvcSymbolFunction:
                return true;

            default:
                return false;
        }
    }

    // Delete():
    //
    // Deletes the symbol.  This does *NOT* guarantee that this OBJECT disappears...  only that it is no longer
    // linked to the symbol set.  If there are still other symbols which refer to this symbol, they may become
    // "zombie" symbols.  They will still be there but will not function correctly.
    //
    virtual HRESULT Delete();

    // AddChild():
    //
    // Adds a symbol as a child of this symbol.
    //
    HRESULT AddChild(_In_ ULONG64 uniqueId)
    {
        //
        // We cannot let a C++ exception escape.
        //
        auto fn = [&]()
        {
            m_children.push_back(uniqueId);
            return NotifyDependentChange();
        };
        return ConvertException(fn);
    }

    // RemoveChild():
    //
    // Removes a symbol as a child of this symbol.
    //
    HRESULT RemoveChild(_In_ ULONG64 uniqueId);

    // GetChildPosition():
    //
    // Gets the position of a child symbol within this parent.
    //
    HRESULT GetChildPosition(_In_ ULONG64 childId, _Out_ ULONG64 *pChildPosition)
    {
        size_t idx = 0;
        while (idx < m_children.size())
        {
            if (m_children[idx] == childId)
            {
                break;
            }

            ++idx;
        }

        if (idx >= m_children.size())
        {
            return E_INVALIDARG;
        }

        *pChildPosition = static_cast<ULONG64>(idx);
        return S_OK;
    }

    // MoveChildBefore():
    //
    // Moves a child to before another index.  The specified index can either be absolute or can be relative
    // to a particular symbol kind.
    //
    HRESULT MoveChildBefore(_In_ ULONG64 childId, 
                            _In_ ULONG64 pos,
                            _In_ SvcSymbolKind relativeTo = SvcSymbol);

    // AddDependentNotify():
    //
    // Adds a symbol as a dependent notify symbol.  In other words, if something on our symbol
    // changes (e.g.: type layout), we must notify any symbol for which this method has been called.
    //
    HRESULT AddDependentNotify(_In_ ULONG64 uniqueId)
    {
        //
        // We cannot let a C++ exception escape.
        //
        auto fn = [&]()
        {
            auto it = m_dependentNotifySymbols.find(uniqueId);
            if (it == m_dependentNotifySymbols.end())
            {
                m_dependentNotifySymbols.insert( { uniqueId, 1 });
            }
            else
            {
                //
                // Increment the dependency count so that we know how many dependencies are on this unique
                // id.  This allows us to track, for instance, something like:
                //
                // struct foo
                // {
                //     struct bar a;
                //     struct bar b;
                // }
                //
                // There will be a single "dependency" entry within bar for "foo" as a unique ID.  It will have
                // a two dependency count.  If "a" or "b" are ever removed from foo, that dependency count would
                // drop to one.
                //
                ++(it->second);
            }

            return S_OK;
        };
        return ConvertException(fn);
    }

    // RemoveDependentNotify():
    //
    // Removes a symbol as a dependent notify symbol.  This effectively undoes a call to AddDependentNotify.
    //
    HRESULT RemoveDependentNotify(_In_ ULONG64 uniqueId)
    {
        //
        // We cannot let a C++ exception escape.
        //
        auto fn = [&]()
        {
            auto it = m_dependentNotifySymbols.find(uniqueId);
            if (it != m_dependentNotifySymbols.end())
            {
                if (it->second == 1)
                {
                    m_dependentNotifySymbols.erase(it);
                }
                else
                {
                    //
                    // Decrement the dependency count.  See AddDependentNotify for details.
                    //
                    --(it->second);
                }
            }
            return S_OK;
        };
        return ConvertException(fn);
    }

    //*************************************************
    // Internal Accessors:
    //

    SymbolSet *InternalGetSymbolSet() const { return m_pSymbolSet; }
    std::wstring const& InternalGetName() const { return m_name; }
    std::wstring const& InternalGetQualifiedName() const
    {
        return m_qualifiedName.empty() ? m_name : m_qualifiedName;
    }
    SvcSymbolKind InternalGetKind() const { return m_kind; }
    ULONG64 InternalGetId() const { return m_id; }
    ULONG64 InternalGetParentId() const { return m_parentId; }
    std::vector<ULONG64> const& InternalGetChildren() { return m_children; }
    bool InternalSetName(_In_opt_ PCWSTR pwszName)
    {
        return SUCCEEDED(ConvertException([&](){
            m_name = pwszName;
            return S_OK;
        }));
    }

protected:

    // Assigned unique id for the symbol
    ULONG64 m_id;
    
    // Assigned unique id for the parent of this symbol
    ULONG64 m_parentId;

    // The kind of this symbol
    SvcSymbolKind m_kind;

    // The names of this symbol
    std::wstring m_name;
    std::wstring m_qualifiedName;

    // Index of children of this symbol
    std::vector<ULONG64> m_children;

    // Index of all symbols which are dependent upon this symbol.  If the layout of a type is modified,
    // everything which includes that type must be "laid out again".  This is the list of symbols which
    // must receive that notification.
    //
    // Note that this is a map from "unique id" -> "dependency count"
    //
    std::unordered_map<ULONG64, ULONG64> m_dependentNotifySymbols;

    // Weak back pointer to our owning symbol set.
    SymbolSet *m_pSymbolSet;

private:

    // InitializeNewSymbol():
    //
    // Called to initialize a new symbol.  This adds it to the symbol set's list, assigns a unique id,
    // etc...
    //
    HRESULT InitializeNewSymbol(_In_ ULONG64 reservedId = 0);
};

// ChildEnumerator:
//
// An enumerator which enumerates children of a given symbol.
//
class ChildEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcSymbolSetEnumerator
        >
{
public:

    //*************************************************
    // ISvcSymbolSetEnumerator:
    //

    IFACEMETHOD(Reset)()
    {
        m_pos = 0;
        return S_OK;
    }

    IFACEMETHOD(GetNext)(_COM_Outptr_ ISvcSymbol **ppSymbol);

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ BaseSymbol *pSymbol,
                                   _In_ SvcSymbolKind kind = SvcSymbol,
                                   _In_opt_ PCWSTR pwszName = nullptr)
    {
        //
        // We cannot let a C++ exception escape.
        //
        auto fn = [&]()
        {
            m_kind = kind;
            if (pwszName != nullptr)
            {
                m_name = pwszName;
            }
            m_spSymbol = pSymbol;
            m_pSymbol = pSymbol;
            return Reset();
        };
        return ConvertException(fn);
    }

private:

    Microsoft::WRL::ComPtr<ISvcSymbol> m_spSymbol;
    BaseSymbol *m_pSymbol;
    SvcSymbolKind m_kind;
    std::wstring m_name;
    size_t m_pos;
};


} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __SYMBOLBASE_H__
