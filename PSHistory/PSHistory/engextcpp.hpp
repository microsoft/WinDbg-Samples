//----------------------------------------------------------------------------
//
// C++ dbgeng extension framework.
//
// The framework makes it easy to write dbgeng extension
// DLLs by wrapping the inconvenient parts of the extension API.
// Boilerplate code is provided as base implementations,
// removing the need to put in empty or skeleton code.
// Error handling is done via exceptions, removing most
// error path code.
//
// The framework assumes async exception handling compilation.
//
// Copyright (C) Microsoft Corporation, 2005-2009.
//
//----------------------------------------------------------------------------

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef __ENGEXTCPP_HPP__
#define __ENGEXTCPP_HPP__

#ifndef __cplusplus
#error engextcpp.hpp requires C++.
#endif

#include <windows.h>
#include <dbgeng.h>
#define KDEXT_64BIT
#include <wdbgexts.h>

#include <pshpack8.h>

#pragma comment(lib,"dbgeng.lib")

#if _MSC_VER >= 800
#pragma warning(disable:4121)
#endif
      
// This will be an engine extension DLL so the wdbgexts
// APIs are not appropriate.
#undef DECLARE_API
#undef DECLARE_API32
#undef DECLARE_API64

//
// If you need to see DllMain-style notifications in
// your extension DLL code you can set this global
// function pointer and the DllMain provided by EngExtCpp
// will pass on all calls it receives.  Declaring a global
// ExtSetDllMain class instance will set the pointer
// prior to the C runtime calling DllMain.
//
// If you are writing a hybrid dbgeng/EngExtCpp extension
// and you want to override the global dbgeng extension
// functions like DebugExtensionInitialize you can do
// so by using .def file renaming of the exports.  For
// example, instead of
//
// EXPORTS
//     DebugEngineInitialize
//
// use
//
// EXPORTS
//     DebugEngineInitialize=MyDebugEngineInitialize
//
// The export will then refer to MyDebugEngineInitialize
// instead of the EngExtCpp-provided DebugEngineInitialize.
// If you do override the provided DebugEngineInitialize you
// must call ExtExtension::BaseInitialize exactly once on
// your EngExtCpp class instance before any use of EngExtCpp
// functionality.
//

typedef BOOL (WINAPI *PEXT_DLL_MAIN)
    (HANDLE Instance, ULONG Reason, PVOID Reserved);

extern PEXT_DLL_MAIN g_ExtDllMain;

class ExtSetDllMain
{
public:
    WINAPI ExtSetDllMain(_In_ PEXT_DLL_MAIN Func)
    {
        g_ExtDllMain = Func;
    }
};

//----------------------------------------------------------------------------
//
// Basic utilities needed later.
//
//----------------------------------------------------------------------------

#define EXT_RELEASE(_Unk) \
    ((_Unk) != NULL ? ((_Unk)->Release(), (void)((_Unk) = NULL)) : (void)NULL)

#define EXT_DIMAT(_Array, _EltType) (sizeof(_Array) / sizeof(_EltType))
#define EXT_DIMA(_Array) EXT_DIMAT(_Array, (_Array)[0])

class ExtExtension;
class ExtCommandDesc;

//----------------------------------------------------------------------------
//
// All errors from this framework are handled by exceptions.
// The exception hierarchy allows various conditions to
// be handled separately, but generally extensions should
// not need to do any exception handling.  The framework
// automatically wraps extensions with try/catch to absorb
// errors properly.
//
//----------------------------------------------------------------------------

class ExtException
{
public:
    ExtException(_In_ HRESULT Status,
                 _In_opt_ PCSTR Message)
    {
        m_Status = Status;
        m_Message = Message;
    }

    HRESULT GetStatus(void) const
    {
        return m_Status;
    }
    HRESULT SetStatus(_In_ HRESULT Status)
    {
        m_Status = Status;
        return Status;
    }
    
    PCSTR GetMessage(void) const
    {
        return m_Message;
    }
    void SetMessage(_In_opt_ PCSTR Message)
    {
        m_Message = Message;
    }
    
    void WINAPI PrintMessageVa(_In_reads_(BufferChars) PSTR Buffer,
                        _In_ ULONG BufferChars,
                        _In_ PCSTR Format,
                        _In_ va_list Args);
    void WINAPIV PrintMessage(_In_reads_(BufferChars) PSTR Buffer,
                              _In_ ULONG BufferChars,
                              _In_ PCSTR Format,
                              ...);
    
protected:
    HRESULT m_Status;
    PCSTR m_Message;
};

class ExtRemoteException : public ExtException
{
public:
    ExtRemoteException(_In_ HRESULT Status,
                       _In_ PCSTR Message)
        : ExtException(Status, Message) { }
};

class ExtStatusException : public ExtException
{
public:
    ExtStatusException(_In_ HRESULT Status,
                       _In_opt_ PCSTR Message = NULL)
        : ExtException(Status, Message) { }
};

class ExtInterruptException : public ExtException
{
public:
    ExtInterruptException(void)
        : ExtException(HRESULT_FROM_NT(STATUS_CONTROL_C_EXIT),
                       "Operation interrupted by request") { }
};

class ExtCheckedPointerException : public ExtException
{
public:
    ExtCheckedPointerException(_In_ PCSTR Message)
        : ExtException(E_INVALIDARG, Message) { }
};

class ExtInvalidArgumentException : public ExtException
{
public:
    ExtInvalidArgumentException(_In_ PCSTR Message)
        : ExtException(E_INVALIDARG, Message) { }
};

//----------------------------------------------------------------------------
//
// A checked pointer ensures that its value is non-NULL.
// This kind of wrapper is used for engine interface pointers
// so that extensions can simply use whatever interface they
// prefer with soft failure against engines that don't support
// the desired interfaces.
//
//----------------------------------------------------------------------------

template<typename _T>
class ExtCheckedPointer
{
public:
    ExtCheckedPointer(_In_ PCSTR Message)
    {
        m_Message = Message;
        m_Ptr = NULL;
    }

    bool IsSet(void) const
    {
        return m_Ptr != NULL;
    }
    void Throw(void) const throw(...)
    {
        if (!m_Ptr)
        {
            throw ExtCheckedPointerException(m_Message);
        }
    }
    _T* Get(void) const throw(...)
    {
        Throw();
        return m_Ptr;
    }
    void Set(_In_opt_ _T* Ptr)
    {
        m_Ptr = Ptr;
    }

    bool operator==(const _T* Ptr) const
    {
        return m_Ptr == Ptr;
    }
    bool operator!=(const _T* Ptr) const
    {
        return !(*this == Ptr);
    }

    operator _T*(void) throw(...)
    {
        return Get();
    }
    operator const _T*(void) const throw(...)
    {
        return Get();
    }
    _T* operator->(void) const throw(...)
    {
        return Get();
    }
    _T** operator&(void)
    {
        return &m_Ptr;
    }
    ExtCheckedPointer<_T>& operator=(ExtCheckedPointer<_T>& Ptr)
    {
        Set(Ptr.m_Ptr);
        return *this;
    }
    ExtCheckedPointer<_T>& operator=(_In_opt_ _T* Ptr)
    {
        Set(Ptr);
        return *this;
    }

protected:
    PCSTR m_Message;
    _T* m_Ptr;
};

//----------------------------------------------------------------------------
//
// An unknown holder is a safe pointer for an IUnknown.
// It automatically checks for NULL usage and calls
// Release on destruction.
//
//----------------------------------------------------------------------------

template<typename _T>
class ExtUnknownHolder
{
public:
    ExtUnknownHolder(void)
    {
        m_Unk = NULL;
    }
    ~ExtUnknownHolder(void)
    {
        EXT_RELEASE(m_Unk);
    }
    
    _T* Get(void) const throw(...)
    {
        if (!m_Unk)
        {
            throw ExtStatusException(E_NOINTERFACE,
                                     "ExtUnknownHolder NULL reference");
        }
        return m_Unk;
    }
    void Set(_In_opt_ _T* Unk)
    {
        EXT_RELEASE(m_Unk);
        m_Unk = Unk;
    }
    _T* Relinquish(void)
    {
        _T* Ret = m_Unk;
        m_Unk = NULL;
        return m_Unk;
    }

    bool operator==(const _T* Unk) const
    {
        return m_Unk == Unk;
    }
    bool operator!=(const _T* Unk) const
    {
        return !(*this == Unk);
    }
    
    operator _T*(void) throw(...)
    {
        return Get();
    }
    _T* operator->(void) throw(...)
    {
        return Get();
    }
    _T** operator&(void)
    {
        if (m_Unk)
        {
            throw ExtStatusException(E_NOINTERFACE,
                                     "ExtUnknownHolder non-NULL & reference");
        }
        return &m_Unk;
    }
    ExtUnknownHolder<_T>& operator=(ExtUnknownHolder<_T>& Unk)
    {
        if (Unk.m_Unk)
        {
            Unk.m_Unk->AddRef();
        }
        Set(Unk.m_Unk);
        return *this;
    }
    ExtUnknownHolder<_T>& operator=(_T* Unk)
    {
        Set(Unk);
        return *this;
    }

protected:
    _T* m_Unk;
};

//----------------------------------------------------------------------------
//
// A delete holder is a safe pointer for a dynamic object.
// It automatically checks for NULL usage and calls
// delete on destruction.
//
//----------------------------------------------------------------------------

template<typename _T, bool _Vector = false>
class ExtDeleteHolder
{
public:
    ExtDeleteHolder(void)
    {
        m_Ptr = NULL;
    }
    ~ExtDeleteHolder(void)
    {
        Delete();
    }

    _T* New(void) throw(...)
    {
        if (_Vector)
        {
            throw ExtInvalidArgumentException
                ("Scalar New used on vector ExtDeleteHolder");
        }
        _T* Ptr = new _T;
        if (!Ptr)
        {
            throw ExtStatusException(E_OUTOFMEMORY);
        }
        Set(Ptr);
        return Ptr;
    }
    _T* New(_In_ ULONG Elts) throw(...)
    {
        if (Elts > (ULONG_PTR)-1 / sizeof(_T))
        {
            throw ExtStatusException
                (HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
                 "ExtDeleteHolder::New count overflow");
        }
        if (!_Vector)
        {
            throw ExtInvalidArgumentException
                ("Vector New used on scalar ExtDeleteHolder");
        }
        _T* Ptr = new _T[Elts];
        if (!Ptr)
        {
            throw ExtStatusException(E_OUTOFMEMORY);
        }
        Set(Ptr);
        return Ptr;
    }
    void Delete(void)
    {
        if (_Vector)
        {
            delete [] m_Ptr;
        }
        else
        {
            delete m_Ptr;
        }
        m_Ptr = NULL;
    }
        
    _T* Get(void) const throw(...)
    {
        if (!m_Ptr)
        {
            throw ExtStatusException(E_INVALIDARG,
                                     "ExtDeleteHolder NULL reference");
        }
        return m_Ptr;
    }
    void Set(_In_opt_ _T* Ptr)
    {
        Delete();
        m_Ptr = Ptr;
    }
    _T* Relinquish(void)
    {
        _T* Ret = m_Ptr;
        m_Ptr = NULL;
        return Ret;
    }

    bool operator==(_In_opt_ const _T* Ptr) const
    {
        return m_Ptr == Ptr;
    }
    bool operator!=(_In_opt_ const _T* Ptr) const
    {
        return !(*this == Ptr);
    }
    
    operator _T*(void) throw(...)
    {
        return Get();
    }
    operator const _T*(void) const throw(...)
    {
        return Get();
    }
    _T* operator->(void) const throw(...)
    {
        return Get();
    }
    _T** operator&(void)
    {
        if (m_Ptr)
        {
            throw ExtStatusException(E_INVALIDARG,
                                     "ExtDeleteHolder non-NULL & reference");
        }
        return &m_Ptr;
    }
    ExtDeleteHolder<_T, _Vector>& operator=(_In_ ExtDeleteHolder<_T, _Vector>& Ptr)
    {
        Set(Ptr.Relinquish());
        return *this;
    }
    ExtDeleteHolder<_T, _Vector>& operator=(_In_opt_ _T* Ptr)
    {
        Set(Ptr);
        return *this;
    }

protected:
    _T* m_Ptr;
};

//----------------------------------------------------------------------------
//
// A current-thread holder is an auto-cleanup holder
// for restoring the debugger's current thread.
//
//----------------------------------------------------------------------------

class ExtCurrentThreadHolder
{
public:
    ExtCurrentThreadHolder(void)
    {
        m_ThreadId = DEBUG_ANY_ID;
    }
    ExtCurrentThreadHolder(_In_ ULONG Id)
    {
        m_ThreadId = Id;
    }
    ExtCurrentThreadHolder(_In_ bool DoRefresh)
    {
        if (DoRefresh)
        {
            Refresh();
        }
    }
    ~ExtCurrentThreadHolder(void)
    {
        Restore();
    }

    void WINAPI Refresh(void) throw(...);
    void WINAPI Restore(void);

    ULONG m_ThreadId;
};

//----------------------------------------------------------------------------
//
// A current-process holder is an auto-cleanup holder
// for restoring the debugger's current process.
//
//----------------------------------------------------------------------------

class ExtCurrentProcessHolder
{
public:
    ExtCurrentProcessHolder(void)
    {
        m_ProcessId = DEBUG_ANY_ID;
    }
    ExtCurrentProcessHolder(_In_ ULONG Id)
    {
        m_ProcessId = Id;
    }
    ExtCurrentProcessHolder(_In_ bool DoRefresh)
    {
        if (DoRefresh)
        {
            Refresh();
        }
    }
    ~ExtCurrentProcessHolder(void)
    {
        Restore();
    }

    void WINAPI Refresh(void) throw(...);
    void WINAPI Restore(void);

    ULONG m_ProcessId;
};

//----------------------------------------------------------------------------
//
// An effective-processor-type holder is an auto-cleanup holder
// for restoring the debugger's effective processor type.
//
//----------------------------------------------------------------------------

class ExtEffectiveProcessorTypeHolder
{
public:
    ExtEffectiveProcessorTypeHolder(void)
    {
        m_ProcType = DEBUG_ANY_ID;
    }
    ExtEffectiveProcessorTypeHolder(_In_ ULONG Type)
    {
        m_ProcType = Type;
    }
    ExtEffectiveProcessorTypeHolder(_In_ bool DoRefresh)
    {
        if (DoRefresh)
        {
            Refresh();
        }
    }
    ~ExtEffectiveProcessorTypeHolder(void)
    {
        Restore();
    }

    void WINAPI Refresh(void) throw(...);
    void WINAPI Restore(void);

    bool IsHolding(void)
    {
        return m_ProcType != DEBUG_ANY_ID;
    }
    
    ULONG m_ProcType;
};

//----------------------------------------------------------------------------
//
// A radix holder is an auto-cleanup holder
// for restoring the debugger's current radix.
//
//----------------------------------------------------------------------------

class ExtRadixHolder
{
public:
    ExtRadixHolder(void)
    {
        m_Radix = DEBUG_ANY_ID;
    }
    ExtRadixHolder(_In_ ULONG Radix)
    {
        m_Radix = Radix;
    }
    ExtRadixHolder(_In_ bool DoRefresh)
    {
        if (DoRefresh)
        {
            Refresh();
        }
    }
    ~ExtRadixHolder(void)
    {
        Restore();
    }

    void WINAPI Refresh(void) throw(...);
    void WINAPI Restore(void);

    ULONG m_Radix;
};

//----------------------------------------------------------------------------
//
// Simple dynamic buffers.  These are primarily intended to
// make it easy to come up with arrays for out parameters
// and aren't intended to be general dynamic vector classes.
//
//----------------------------------------------------------------------------

template<typename _T>
class ExtBuffer
{
public:
    ExtBuffer(void)
    {
        Clear();
    }
    ExtBuffer(_In_reads_(Elts) _T* Ptr,
              _In_ ULONG Elts,
              _In_ bool Owned,
              _In_ ULONG Used)
    {
        Clear();
        Set(Ptr, Elts, Owned, Used);
    }
    ~ExtBuffer(void)
    {
        Delete();
    }

    void Set(_In_reads_(Elts) _T* Ptr,
             _In_ ULONG Elts,
             _In_ bool Owned,
             _In_ ULONG Used)
    {
        Delete();
        m_Ptr = Ptr;
        m_EltsAlloc = Elts;
        m_Owned = Owned;
        m_EltsUsed = Used;
    }
    void SetUsed(_In_reads_(Elts) _T* Ptr,
                 _In_ ULONG Elts,
                 _In_ bool Owned)
    {
        Set(Ptr, Elts, Owned, Elts);
    }
    void SetUnused(_In_reads_(Elts) _T* Ptr,
                   _In_ ULONG Elts,
                   _In_ bool Owned)
    {
        Set(Ptr, Elts, Owned, 0);
    }
    void SetEltsUsed(_In_ ULONG Elts) throw(...)
    {
        if (Elts > m_EltsAlloc)
        {
            throw ExtStatusException(E_INVALIDARG,
                                     "ExtBuffer::SetEltsUsed "
                                     "illegal elt count");
        }

        m_EltsUsed = Elts;
    }

    void Resize(_In_ ULONG Elts) throw(...)
    {
        if (Elts > (ULONG_PTR)-1 / sizeof(_T))
        {
            throw ExtStatusException
                (HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
                 "ExtBuffer::Resize count overflow");
        }
        
        _T* Ptr = new _T[Elts];
        if (!Ptr)
        {
            throw ExtStatusException(E_OUTOFMEMORY);
        }

        for (ULONG i = 0; i < Elts; i++) {

            Ptr[i] = 0;
        }
        
        ULONG Used = m_EltsUsed;
        if (Elts < Used)
        {
            Used = Elts;
        }

        if (m_Ptr)
        {
            for (ULONG i = 0; i < Used; i++)
            {
                Ptr[i] = m_Ptr[i];
            }
        }
        
        Set(Ptr, Elts, true, Used);
    }

    // The 'Extra' parameter is just a convenience for
    // adding to a count so that the integer overflow checks
    // can be done for the caller here.  The request
    // is for Elts+Extra slots to be available for use.
    void Require(_In_ ULONG Elts,
                 _In_ ULONG Extra = 0) throw(...)
    {
        if (Elts > (ULONG)-1 - Extra)
        {
            throw ExtStatusException
                (HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
                 "ExtBuffer::Require count overflow");
        }
        Elts += Extra;

        if (Elts > m_EltsAlloc)
        {
            Resize(Elts);
        }
    }
    void RequireRounded(_In_ ULONG Elts,
                        _In_ ULONG Round) throw(...)
    {
        if (Round < 2 ||
            Elts > (ULONG)-1 - Round)
        {
            throw ExtStatusException
                (HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
                 "ExtBuffer::RequireRounded count overflow");
        }
        Elts += Round - 1;
        Elts -= Elts % Round;

        Require(Elts);
    }

    void Delete(void)
    {
        if (m_Owned)
        {
            delete [] m_Ptr;
        }
        Clear();
    }
    _T* Relinquish(void)
    {
        _T* Ret = m_Ptr;
        Clear();
        return Ret;
    }
    void Empty(void)
    {
        m_EltsUsed = 0;
    }
    void Clear(void)
    {
        m_Ptr = NULL;
        m_EltsAlloc = 0;
        m_EltsUsed = 0;
        m_Owned = false;
    }

    _T* Get(_In_ ULONG Use) throw(...)
    {
        Require(Use);
        m_EltsUsed = Use;
        return m_Ptr;
    }
    _T* GetBuffer(void) const throw(...)
    {
        if (!m_Ptr)
        {
            throw ExtStatusException(E_INVALIDARG,
                                     "ExtBuffer NULL reference");
        }
        return m_Ptr;
    }
    _T* GetRawBuffer(void) const
    {
        return m_Ptr;
    }
    operator _T*(void) throw(...)
    {
        return GetBuffer();
    }
    operator const _T*(void) const throw(...)
    {
        return GetBuffer();
    }

    _T* Copy(_In_reads_(Elts) const _T* Src,
             _In_ ULONG Elts = 1)
    {
        _T* Dst = Get(Elts);
        for (ULONG i = 0; i < Elts; i++)
        {
            Dst[i] = Src[i];
        }
        return Dst;
    }
    _T* Copy(_In_ const ExtBuffer* Other)
    {
        return Copy(Other->GetRawBuffer(), Other->GetEltsUsed());
    }

    _T* Append(_In_reads_(Elts) const _T* Src,
               _In_ ULONG Elts = 1)
    {
        Require(m_EltsUsed, Elts);
        for (ULONG i = 0; i < Elts; i++)
        {
            m_Ptr[m_EltsUsed++] = Src[i];
        }
        return m_Ptr;
    }
    _T* Append(_In_ const ExtBuffer* Other)
    {
        return Append(Other->GetRawBuffer(), Other->GetEltsUsed());
    }
    
    ULONG GetEltsUsed(void) const
    {
        return m_EltsUsed;
    }
    ULONG GetEltsAlloc(void) const
    {
        return m_EltsAlloc;
    }
    bool GetOwned(void) const
    {
        return m_Owned;
    }
    
    ExtBuffer<_T>& operator=(_In_ ExtBuffer<_T>& Ptr)
    {
        Set(Ptr.m_Ptr, Ptr.m_EltsAlloc, Ptr.m_Owned, Ptr.m_EltsUsed);
        Ptr.Clear();
        return *this;
    }
    ExtBuffer<_T>& operator=(_In_opt_ _T* Ptr)
    {
        throw ExtStatusException(E_INVALIDARG,
                                 "ExtBuffer can't be assigned "
                                 "an unsized pointer");
        return *this;
    }

protected:
    _T* m_Ptr;
    ULONG m_EltsUsed;
    ULONG m_EltsAlloc;
    bool m_Owned;
};

// Variant which adds an initial amount of locally-declared storage space.
template<typename _T, size_t _DeclElts>
class ExtDeclBuffer : public ExtBuffer<_T>
{
public:
    ExtDeclBuffer(void) :
        ExtBuffer(m_Decl, _DeclElts, false, 0)
    {
    };

    ExtDeclBuffer& operator=(_In_ ExtDeclBuffer& Ptr)
    {
        throw ExtStatusException(E_INVALIDARG,
                                 "ExtDeclBuffer can't be assigned a buffer");
        return *this;
    }
    ExtDeclBuffer& operator=(_In_opt_ _T* Ptr)
    {
        throw ExtStatusException(E_INVALIDARG,
                                 "ExtDeclBuffer can't be assigned a buffer");
        return *this;
    }

protected:
    _T m_Decl[_DeclElts];
};

// Variant which adds an initial amount of locally-declared storage space,
// but in this case the declaration is always done with a 64-bit
// buffer so that you can assume 64-bit alignment.
// This is useful for cases where you're allocating a buffer of
// mixed data as a BYTE buffer, but you need to ensure that
// the data buffer will have alignment large enough for any
// of the mixed data elements.
template<typename _T, size_t _DeclElts>
class ExtDeclAlignedBuffer : public ExtBuffer<_T>
{
public:
    ExtDeclAlignedBuffer(void) :
        ExtBuffer((_T*)m_Decl, _DeclElts, false, 0)
    {
    };

    ExtDeclAlignedBuffer& operator=(_In_ ExtDeclAlignedBuffer& Ptr)
    {
        throw ExtStatusException(E_INVALIDARG,
                                 "ExtDeclAlignedBuffer "
                                 "can't be assigned a buffer");
        return *this;
    }
    ExtDeclAlignedBuffer& operator=(_In_opt_ _T* Ptr)
    {
        throw ExtStatusException(E_INVALIDARG,
                                 "ExtDeclAlignedBuffer "
                                 "can't be assigned a buffer");
        return *this;
    }

protected:
    ULONG64 m_Decl[(_DeclElts * sizeof(_T) + sizeof(ULONG64) - 1) /
                   sizeof(ULONG64)];
};

//----------------------------------------------------------------------------
//
// Descriptive information kept for all extension commands.
// Automatic help and parameter parsing are built on top
// of this descriptive info.
//
// The argument format is described below with EXT_COMMAND.
//
//----------------------------------------------------------------------------

typedef void (__thiscall ExtExtension::*ExtCommandMethod)(void);
typedef HRESULT (ExtExtension::*ExtRawMethod)(_In_opt_ PVOID Context);
typedef HRESULT (CALLBACK *ExtRawFunction)(_In_opt_ PVOID Context);

class ExtCommandDesc
{
public:
    WINAPI ExtCommandDesc(_In_ PCSTR Name,
                   _In_ ExtCommandMethod Method,
                   _In_ PCSTR Desc,
                   _In_opt_ PCSTR Args);
    WINAPI ~ExtCommandDesc(void);

    ExtExtension* m_Ext;
    ExtCommandDesc* m_Next;
    PCSTR m_Name;
    ExtCommandMethod m_Method;
    PCSTR m_Desc;
    PCSTR m_ArgDescStr;
    bool m_ArgsInitialized;

    //
    // Derived by parsing the argument description string.
    //

    struct ArgDesc
    {
        PCSTR Name;
        PCSTR DescShort;
        PCSTR DescLong;
        PCSTR Default;
        PCSTR ExpressionEvaluator;
        ULONG Boolean:1;
        ULONG Expression:1;
        ULONG ExpressionSigned:1;
        ULONG ExpressionDelimited:1;
        ULONG String:1;
        ULONG StringRemainder:1;
        ULONG Required:1;
        ULONG Present:1;
        ULONG DefaultSilent:1;
        ULONG ExpressionBits;
        ULONG ExpressionRadix;

        bool NeedsOptionsOutput(void)
        {
            return
                (Default && !DefaultSilent) ||
                (Expression &&
                 (ExpressionSigned ||
                  ExpressionDelimited ||
                  ExpressionBits != 64 ||
                  ExpressionRadix != 0 ||
                  ExpressionEvaluator != NULL)) ||
                (String &&
                 StringRemainder);
        }
    };

    bool m_CustomArgParsing;
    PSTR m_CustomArgDescShort;
    PSTR m_CustomArgDescLong;
    PSTR m_OptionChars;
    PSTR m_ArgStrings;
    ULONG m_NumArgs;
    ULONG m_NumUnnamedArgs;
    ArgDesc* m_Args;

    void WINAPI ClearArgs(void);
    void WINAPI DeleteArgs(void);
    PSTR WINAPI ParseDirective(_In_ PSTR Scan) throw(...);
    void WINAPI ParseArgDesc(void) throw(...);
    void WINAPI ExInitialize(_In_ ExtExtension* Ext) throw(...);

    ArgDesc* WINAPI FindArg(_In_ PCSTR Name);
    ArgDesc* WINAPI FindUnnamedArg(_In_ ULONG Index);
    
    static void WINAPI Transfer(_Out_ ExtCommandDesc** Commands,
                                _Out_ PULONG LongestName);

    static ExtCommandDesc* s_Commands;
    static ULONG s_LongestCommandName;
};

//----------------------------------------------------------------------------
//
// Known-struct formatting support.
// In order to automatically advertise known structs for
// formatting an extension should point ExtExtension::m_KnownStructs
// at an array of descriptors.  Callbacks will then be sent
// automatically to the formatting methods when necessary.
//
// The final array entry should have TypeName == NULL.
//
//----------------------------------------------------------------------------

// Data formatting callback for known structs.
// On entry the append buffer will be set to the target buffer.
typedef void (ExtExtension::*ExtKnownStructMethod)
    (_In_ PCSTR TypeName,
     _In_ ULONG Flags,
     _In_ ULONG64 Offset);

struct ExtKnownStruct
{
    PCSTR TypeName;
    ExtKnownStructMethod Method;
    bool SuppressesTypeName;
};

//----------------------------------------------------------------------------
//
// Pseudo-register value provider support.
// In order to automatically advertise extended values
// an extension should point ExtExtension::m_ProvidedValues
// at an array of descriptors.  Callbacks will then be sent
// automatically to the provider methods when necessary.
//
// The final array entry should have ValueName == NULL.
//
//----------------------------------------------------------------------------

// Value retrieval callback.
typedef void (ExtExtension::*ExtProvideValueMethod)
    (_In_ ULONG Flags,
     _In_ PCWSTR ValueName,
     _Out_ PULONG64 Value,
     _Out_ PULONG64 TypeModBase,
     _Out_ PULONG TypeId,
     _Out_ PULONG TypeFlags);

struct ExtProvidedValue
{
    PCWSTR ValueName;
    ExtProvideValueMethod Method;
};

//----------------------------------------------------------------------------
//
// Base class for all extensions.  An extension DLL will
// have a single instance of a derivation of this class.
// The instance global is automatically declared by macros.
// As the instance is a global the initialization and uninitialization
// is explicit instead of driven through construction and destruction.
//
//----------------------------------------------------------------------------

class ExtExtension
{
public:
    WINAPI ExtExtension(void);

    //
    // Initialization and uninitialization.
    //

    // BaseInitialize does one-time initialization
    // for EngExtCpp.  EngExtCpp's DebugExtensionInitialize
    // calls this, so if you are doing a hybrid dbgeng/EngExtCpp
    // extension and using your own DebugExtensionInitialize you
    // can call this to get EngExtCpp's initial state set up.
    HRESULT WINAPI BaseInitialize(_In_ HMODULE ExtDllModule,
                                  _Out_ PULONG Version,
                                  _Out_ PULONG Flags);
    
    virtual HRESULT __thiscall Initialize(void);
    virtual void __thiscall Uninitialize(void);

    //
    // Notifications.
    //

    virtual void __thiscall OnSessionActive(_In_ ULONG64 Argument);
    virtual void __thiscall OnSessionInactive(_In_ ULONG64 Argument);
    virtual void __thiscall OnSessionAccessible(_In_ ULONG64 Argument);
    virtual void __thiscall OnSessionInaccessible(_In_ ULONG64 Argument);

    //
    // Overridable initialization state.
    //
    
    USHORT m_ExtMajorVersion;
    USHORT m_ExtMinorVersion;
    ULONG m_ExtInitFlags;

    ExtKnownStruct* m_KnownStructs;
    ExtProvidedValue* m_ProvidedValues;
    
    //
    // Interface and callback pointers.  These
    // interfaces are retrieved on entry to an extension.
    //

    ExtCheckedPointer<IDebugAdvanced> m_Advanced;
    ExtCheckedPointer<IDebugClient> m_Client;
    ExtCheckedPointer<IDebugControl> m_Control;
    ExtCheckedPointer<IDebugDataSpaces> m_Data;
    ExtCheckedPointer<IDebugRegisters> m_Registers;
    ExtCheckedPointer<IDebugSymbols> m_Symbols;
    ExtCheckedPointer<IDebugSystemObjects> m_System;

    // These derived interfaces may be NULL on
    // older engines which do not support them.
    // The checked pointers will automatically
    // protect access.
    ExtCheckedPointer<IDebugAdvanced2> m_Advanced2;
    ExtCheckedPointer<IDebugAdvanced3> m_Advanced3;
    ExtCheckedPointer<IDebugClient2> m_Client2;
    ExtCheckedPointer<IDebugClient3> m_Client3;
    ExtCheckedPointer<IDebugClient4> m_Client4;
    ExtCheckedPointer<IDebugClient5> m_Client5;
    ExtCheckedPointer<IDebugControl2> m_Control2;
    ExtCheckedPointer<IDebugControl3> m_Control3;
    ExtCheckedPointer<IDebugControl4> m_Control4;
    ExtCheckedPointer<IDebugControl5> m_Control5;
    ExtCheckedPointer<IDebugControl6> m_Control6;
    ExtCheckedPointer<IDebugDataSpaces2> m_Data2;
    ExtCheckedPointer<IDebugDataSpaces3> m_Data3;
    ExtCheckedPointer<IDebugDataSpaces4> m_Data4;
    ExtCheckedPointer<IDebugRegisters2> m_Registers2;
    ExtCheckedPointer<IDebugSymbols2> m_Symbols2;
    ExtCheckedPointer<IDebugSymbols3> m_Symbols3;
    ExtCheckedPointer<IDebugSystemObjects2> m_System2;
    ExtCheckedPointer<IDebugSystemObjects3> m_System3;
    ExtCheckedPointer<IDebugSystemObjects4> m_System4;

    //
    // Interesting information about the session.
    // These values are retrieved on entry to an extension.
    //

    ULONG m_OutputWidth;
    
    // Actual processor type.
    ULONG m_ActualMachine;

    // Current machine mode values, not actual
    // machine mode values.  Generally these are
    // the ones you want to look at.
    // If you care about mixed CPU code, such as WOW64,
    // you may need to also get the actual values.
    ULONG m_Machine;
    ULONG m_PageSize;
    ULONG m_PtrSize;
    ULONG m_NumProcessors;
    ULONG64 m_OffsetMask;

    ULONG m_PlatformId;
    ULONG m_Major;
    ULONG m_Minor;
    ULONG m_MajorVersion;
    ULONG m_MinorVersion;
    ULONG m_ServicePackNumber;
    ULONG m_ServicePackMajor;
    ULONG m_ServicePackMinor;
    USHORT m_SystemVersion;
    ULONG m_ProductType;
    ULONG m_SuiteMask;

    ULONG m_NumberOfPhysicalPages;

    LARGE_INTEGER m_SystemTime;

    //
    // Queries about the current debuggee information available.
    // The type and qualifier are automatically retrieved.
    //
    
    ULONG m_DebuggeeClass;
    ULONG m_DebuggeeQual;
    ULONG m_DumpFormatFlags;

    bool m_IsRemote;
    bool m_OutCallbacksDmlAware;
    
    bool IsUserMode(void)
    {
        return m_DebuggeeClass == DEBUG_CLASS_USER_WINDOWS;
    }
    bool IsKernelMode(void)
    {
        return m_DebuggeeClass == DEBUG_CLASS_KERNEL;
    }
    bool IsLiveLocalUser(void)
    {
        return
            m_DebuggeeClass == DEBUG_CLASS_USER_WINDOWS &&
            m_DebuggeeQual == DEBUG_USER_WINDOWS_PROCESS;
    }
    bool IsMachine32(_In_ ULONG Machine)
    {
        return
            Machine == IMAGE_FILE_MACHINE_I386 ||
            Machine == IMAGE_FILE_MACHINE_ARM ||
            Machine == IMAGE_FILE_MACHINE_THUMB ||
            Machine == IMAGE_FILE_MACHINE_ARMNT;
    }
    bool IsCurMachine32(void)
    {
        return IsMachine32(m_Machine);
    }
    bool IsMachine64(_In_ ULONG Machine)
    {
        return
            Machine == IMAGE_FILE_MACHINE_AMD64 ||
            Machine == IMAGE_FILE_MACHINE_IA64;
    }
    bool IsCurMachine64(void)
    {
        return IsMachine64(m_Machine);
    }
    bool Is32On64(void)
    {
        return IsCurMachine32() && IsMachine64(m_ActualMachine);
    }
    bool CanQueryVirtual(void)
    {
        return
            m_DebuggeeClass == DEBUG_CLASS_USER_WINDOWS ||
            m_DebuggeeClass == DEBUG_CLASS_IMAGE_FILE;
    }
    bool HasFullMemBasic(void)
    {
        return
            m_DebuggeeClass == DEBUG_CLASS_USER_WINDOWS &&
            (m_DebuggeeQual == DEBUG_USER_WINDOWS_PROCESS ||
             m_DebuggeeQual == DEBUG_USER_WINDOWS_PROCESS_SERVER ||
             m_DebuggeeQual == DEBUG_USER_WINDOWS_DUMP ||
             (m_DebuggeeQual == DEBUG_USER_WINDOWS_SMALL_DUMP &&
              (m_DumpFormatFlags &
               DEBUG_FORMAT_USER_SMALL_FULL_MEMORY_INFO) != 0));
    }
    bool IsExtensionRemote(void)
    {
        return m_IsRemote;
    }
    bool AreOutputCallbacksDmlAware(void)
    {
        // Applies to callbacks present in client
        // at the start of the extension command.
        // If the extension changes the output callbacks
        // the value does not automatically update.
        // RefreshOutputCallbackFlags can be used
        // to update this flag after unknown output
        // callbacks are installed.
        return m_OutCallbacksDmlAware;
    }

    //
    // Common mode checks which throw on mismatches.
    //

    void RequireUserMode(void)
    {
        if (!IsUserMode())
        {
            throw ExtStatusException(S_OK, "user-mode only");
        }
    }
    void RequireKernelMode(void)
    {
        if (!IsKernelMode())
        {
            throw ExtStatusException(S_OK, "kernel-mode only");
        }
    }
    
    //
    // Output through m_Control.
    //

    // Defaults to DEBUG_OUTPUT_NORMAL, but can
    // be overridden to produce different output.
    // Warn, Err and Verb are convenience routines for
    // the warning, error and verbose cases.
    ULONG m_OutMask;
    
    void WINAPIV Out(_In_ PCSTR Format,
                     ...);
    void WINAPIV Warn(_In_ PCSTR Format,
                      ...);
    void WINAPIV Err(_In_ PCSTR Format,
                     ...);
    void WINAPIV Verb(_In_ PCSTR Format,
                      ...);
    void WINAPIV Out(_In_ PCWSTR Format,
                     ...);
    void WINAPIV Warn(_In_ PCWSTR Format,
                      ...);
    void WINAPIV Err(_In_ PCWSTR Format,
                     ...);
    void WINAPIV Verb(_In_ PCWSTR Format,
                      ...);

    void WINAPIV Dml(_In_ PCSTR Format,
                     ...);
    void WINAPIV DmlWarn(_In_ PCSTR Format,
                         ...);
    void WINAPIV DmlErr(_In_ PCSTR Format,
                        ...);
    void WINAPIV DmlVerb(_In_ PCSTR Format,
                         ...);
    void WINAPIV Dml(_In_ PCWSTR Format,
                     ...);
    void WINAPIV DmlWarn(_In_ PCWSTR Format,
                         ...);
    void WINAPIV DmlErr(_In_ PCWSTR Format,
                        ...);
    void WINAPIV DmlVerb(_In_ PCWSTR Format,
                         ...);

    void DmlCmdLink(_In_ PCSTR Text,
                    _In_ PCSTR Cmd)
    {
        Dml("<link cmd=\"%s\">%s</link>", Cmd, Text);
    }
    void DmlCmdExec(_In_ PCSTR Text,
                    _In_ PCSTR Cmd)
    {
        Dml("<exec cmd=\"%s\">%s</exec>", Cmd, Text);
    }

    void RefreshOutputCallbackFlags(void)
    {
        m_OutCallbacksDmlAware = false;
        if (m_Advanced2.IsSet() &&
            m_Advanced2->
            Request(DEBUG_REQUEST_CURRENT_OUTPUT_CALLBACKS_ARE_DML_AWARE,
                    NULL, 0, NULL, 0, NULL) == S_OK)
        {
            m_OutCallbacksDmlAware = true;
        }
    }

    //
    // Wrapped text output support.
    //

    ULONG m_CurChar;
    ULONG m_LeftIndent;
    bool m_AllowWrap;
    bool m_TestWrap;
    ULONG m_TestWrapChars;
    // m_OutputWidth is also used.
    
    // OutWrap takes the given string and displays it
    // wrapped in the appropriate space.  It doesn't
    // account for tabs, backspaces, internal returns, etc.
    // Uses all wrapping state and updates m_CurChar.
    void WINAPI WrapLine(void);
    void WINAPI OutWrapStr(_In_ PCSTR String);
    void WINAPIV OutWrapVa(_In_ PCSTR Format,
                           _In_ va_list Args);
    void WINAPIV OutWrap(_In_ PCSTR Format,
                         ...);

    void ClearWrap(void)
    {
        m_LeftIndent = 0;
        m_CurChar = 0;
    }
    
    void MarkWrapPoint(void)
    {
        m_LeftIndent = m_CurChar;
    }
    
    // Wraps if the given number of characters wouldn't
    // fit on the current line.
    bool DemandWrap(_In_ ULONG Chars)
    {
        if (m_CurChar + Chars >= m_OutputWidth)
        {
            WrapLine();
            return true;
        }

        return false;
    }
    
    // Wrapping can be suppressed to allow blocks of
    // output to be unsplit but to still get cur char
    // tracking.
    void AllowWrap(_In_ bool Allow)
    {
        m_AllowWrap = Allow;
    }

    // Output can be suppressed, allowing collection
    // of character counts as a way to pre-test whether
    // a set of output will wrap.
    void TestWrap(_In_ bool Test)
    {
        m_TestWrap = Test;
        if (Test)
        {
            m_TestWrapChars = 0;
        }
    }

    //
    // A circular string buffer is available for
    // handing out multiple static strings.
    //

    PSTR WINAPI RequestCircleString(_In_ ULONG Chars) throw(...);
    PSTR WINAPI CopyCircleString(_In_ PCSTR Str) throw(...);
    PSTR WINAPI PrintCircleStringVa(_In_ PCSTR Format,
                             _In_ va_list Args) throw(...);
    PSTR WINAPIV PrintCircleString(_In_ PCSTR Format,
                                   ...) throw(...);

    //
    // String buffer with append support.
    // Throws on buffer overflow.
    //

    PSTR m_AppendBuffer;
    ULONG m_AppendBufferChars;
    PSTR m_AppendAt;
    
    void WINAPI SetAppendBuffer(_In_reads_(BufferChars) PSTR Buffer,
                         _In_ ULONG BufferChars);
    void WINAPI AppendBufferString(_In_ PCSTR Str) throw(...);
    void WINAPI AppendStringVa(_In_ PCSTR Format,
                        _In_ va_list Args) throw(...);
    void WINAPIV AppendString(_In_ PCSTR Format,
                              ...) throw(...);

    bool IsAppendStart(void)
    {
        return m_AppendAt == m_AppendBuffer;
    }
    
    //
    // Set the return status for an extension call
    // if a specific non-S_OK status needs to be returned.
    //

    void WINAPI SetCallStatus(_In_ HRESULT Status);

    //
    // Change the effective processor type.
    // This will refresh the cached ExtExtension machine info
    // and optionally initialize an effective processor holder.
    //

    ULONG WINAPI GetEffectiveProcessor(void) throw(...);
    void WINAPI SetEffectiveProcessor(_In_ ULONG ProcType,
                                      _Inout_opt_ ExtEffectiveProcessorTypeHolder* Holder = NULL) throw(...);

    //
    // Cached symbol info.  The cache is
    // automatically flushed when the backing
    // symbol info changes.
    //

    ULONG WINAPI GetCachedSymbolTypeId(_Inout_ PULONG64 Cookie,
                                _In_ PCSTR Symbol,
                                _Out_ PULONG64 ModBase);
    ULONG WINAPI GetCachedFieldOffset(_Inout_ PULONG64 Cookie,
                               _In_ PCSTR Type,
                               _In_ PCSTR Field,
                               _Out_opt_ PULONG64 ModBase = NULL,
                               _Out_opt_ PULONG TypeId = NULL);
    bool WINAPI GetCachedSymbolInfo(_In_ ULONG64 Cookie,
                             _Out_ PDEBUG_CACHED_SYMBOL_INFO Info);
    bool WINAPI AddCachedSymbolInfo(_In_ PDEBUG_CACHED_SYMBOL_INFO Info,
                             _In_ bool ThrowFailure,
                             _Out_ PULONG64 Cookie);

    //
    // Symbol helpers.
    //

    void WINAPI FindSymMatchStringA(void) throw(...);
    
    // Matches patterns using the same code as dbgeng/dbghelp.
    bool MatchPattern(_In_ PCSTR ArbitraryString,
                      _In_ PCSTR Pattern,
                      _In_ bool CaseSensitive = false)
    {
        if (!m_SymMatchStringA)
        {
            FindSymMatchStringA();
        }
        return m_SymMatchStringA(ArbitraryString, Pattern,
                                 CaseSensitive) != FALSE;
    }
    
    bool GetSymbolOffset(_In_ PCSTR Symbol,
                         _In_ bool RetZero,
                         _Out_ ULONG64* Offs)
    {
        HRESULT Status;

        if ((Status = m_Symbols->GetOffsetByName(Symbol, Offs)) != S_OK)
        {
            if (!RetZero)
            {
                if (Status == S_FALSE)
                {
                    ThrowInvalidArg("'%s' has multiple offsets", Symbol);
                }
                else
                {
                    ThrowStatus(Status, "Unable to resolve '%s'", Symbol);
                }
            }
            else
            {
                *Offs = 0;
            }

            return false;
        }

        return true;
    }
    bool CanResolveSymbol(_In_ PCSTR Symbol)
    {
        ULONG64 Offs;
        return GetSymbolOffset(Symbol, true, &Offs);
    }

    // Note that if you're just retrieving the symbol
    // name to output it it's easier to use %y
    // or IDebugSymbols3::OutputSymbolByOffset.
    bool WINAPI GetOffsetSymbol(_In_ ULONG64 Offs,
                               _Inout_ ExtBuffer<char>* Name,
                               _Out_opt_ PULONG64 Displacement = NULL,
                               _In_ bool AddDisp = false) throw(...);
    
    // Returns index of the first module whose name
    // matches the given pattern.  The scan starts
    // at the given module list index and only
    // looks at loaded modules.
    ULONG WINAPI FindFirstModule(_In_ PCSTR Pattern,
                                 _Inout_opt_ ExtBuffer<char>* Name = NULL,
                                 _In_ ULONG StartIndex = 0) throw(...);

    //
    // Module information helpers.
    //

    void WINAPI GetModuleImagehlpInfo(_In_ ULONG64 ModBase,
                               _Out_ struct _IMAGEHLP_MODULEW64* Info);
    bool WINAPI ModuleHasGlobalSymbols(_In_ ULONG64 ModBase);
    bool WINAPI ModuleHasTypeInfo(_In_ ULONG64 ModBase);

    //
    // Command execution helpers.
    //

    // Uses a circle string.
    void ExecuteVa(_In_ ULONG OutCtl,
                   _In_ ULONG ExecFlags,
                   _In_ PCSTR Format,
                   _In_ va_list Args) throw(...)
    {
        HRESULT Status;
        PSTR Cmd = PrintCircleStringVa(Format, Args);
        
        if (FAILED(Status = m_Control->
                   Execute(OutCtl, Cmd, ExecFlags)))
        {
            ThrowStatus(Status, "Unable to execute '%s'", Cmd);
        }
    }
    void Execute(_In_ ULONG OutCtl,
                 _In_ ULONG ExecFlags,
                 _In_ PCSTR Format,
                 ...) throw(...)
    {
        va_list Args;

        va_start(Args, Format);
        ExecuteVa(OutCtl, ExecFlags, Format, Args);
        va_end(Args);
    }
    void Execute(_In_ PCSTR Format,
                 ...) throw(...)
    {
        va_list Args;

        va_start(Args, Format);
        ExecuteVa(DEBUG_OUTCTL_AMBIENT, DEBUG_EXECUTE_DEFAULT, Format, Args);
        va_end(Args);
    }
    void ExecuteSilent(_In_ PCSTR Format,
                       ...) throw(...)
    {
        va_list Args;

        va_start(Args, Format);
        ExecuteVa(DEBUG_OUTCTL_IGNORE,
                  DEBUG_EXECUTE_NOT_LOGGED |
                  DEBUG_EXECUTE_NO_REPEAT,
                  Format,
                  Args);
        va_end(Args);
    }

    //
    // Invoke a routine in the debuggee.  This is a wrapper
    // for the debugger's .call command.
    // The return value is the raw 64-bit value from @$callret,
    // but you can access richer information by constructing
    // an ExtRemoteTyped on "@$callret".
    //
    // CAUTION: .call hijacks the current thread for the invocation
    // and thus can be unsafe if the invoked code does things
    // which requires a particular thread or program state.
    //
    // CAUTION on EXECUTION: Calling code in the debuggee requires
    // that the debuggee run so using these routines will result
    // in the debuggee running for some period of time.
    // It also means that this will fail on non-executable targets.
    //

    ULONG64 WINAPI CallDebuggeeBase(_In_ PCSTR CommandString,
                                   _In_ ULONG TimeoutMilliseconds);
    // Uses a circle string.
    ULONG64 CallDebuggeeVa(_In_ PCSTR Format,
                           _In_ va_list Args,
                           _In_ ULONG TimeoutMilliseconds = 60000)
    {
        return CallDebuggeeBase(PrintCircleStringVa(Format, Args),
                                TimeoutMilliseconds);
    }
    ULONG64 CallDebuggee(_In_ PCSTR Format,
                         ...)
    {
        va_list Args;
        ULONG64 Ret;

        va_start(Args, Format);
        Ret = CallDebuggeeVa(Format, Args);
        va_end(Args);
        return Ret;
    }

    //
    // Register and pseudo-register access helpers.
    // If an index cache is used it should be initialized
    // to DEBUG_ANY_ID.
    //

    ULONG WINAPI FindRegister(_In_ PCSTR Name,
                       _Inout_opt_ PULONG IndexCache = NULL);
    ULONG64 WINAPI GetRegisterU64(_In_ PCSTR Name,
                           _Inout_opt_ PULONG IndexCache = NULL);
    void WINAPI SetRegisterU64(_In_ PCSTR Name,
                        _In_ ULONG64 Val,
                        _Inout_opt_ PULONG IndexCache = NULL);
    
    ULONG WINAPI FindPseudoRegister(_In_ PCSTR Name,
                             _Inout_opt_ PULONG IndexCache = NULL);
    ULONG64 WINAPI GetPseudoRegisterU64(_In_ PCSTR Name,
                                 _Inout_opt_ PULONG IndexCache = NULL);
    void WINAPI SetPseudoRegisterU64(_In_ PCSTR Name,
                              _In_ ULONG64 Val,
                              _Inout_opt_ PULONG IndexCache = NULL);

    ULONG64 GetExtRetU64(void)
    {
        return GetPseudoRegisterU64("$extret", &m_ExtRetIndex);
    }
    void SetExtRetU64(_In_ ULONG64 Val)
    {
        return SetPseudoRegisterU64("$extret", Val, &m_ExtRetIndex);
    }

    PSTR GetTempRegName(_In_ ULONG Index,
                        _Out_writes_(NameChars) PSTR Name,
                        _In_ ULONG NameChars)
    {
        if (NameChars < 5)
        {
            ThrowInvalidArg("Insufficient temp register name buffer");
        }
        
        Name[0] = '$';
        Name[1] = 't';
        if (Index < 10)
        {
            Name[2] = (char)('0' + Index);
            Name[3] = 0;
        }
        else if (Index < EXT_DIMA(m_TempRegIndex))
        {
            Name[2] = (char)('0' + (Index / 10));
            Name[3] = (char)('0' + (Index % 10));
            Name[4] = 0;
        }
        else
        {
            ThrowInvalidArg("Invalid temp register index %u", Index);
        }
        
        return Name;
    }
    ULONG64 GetTempRegU64(_In_ ULONG Index)
    {
        char Name[5];

        GetTempRegName(Index, Name, EXT_DIMA(Name));
        return GetPseudoRegisterU64(Name, &m_TempRegIndex[Index]);
    }
    void SetTempRegU64(_In_ ULONG Index,
                       _In_ ULONG64 Val)
    {
        char Name[5];

        GetTempRegName(Index, Name, EXT_DIMA(Name));
        return SetPseudoRegisterU64(Name, Val, &m_TempRegIndex[Index]);
    }

    //
    // Incoming argument parsing results.
    // Results are guaranteed to obey the form
    // of the argument description for a command.
    // Mismatched usage, such as a string retrieval
    // for a numeric argument, will result in an exception.
    //

    ULONG GetNumUnnamedArgs(void)
    {
        return m_NumUnnamedArgs;
    }
    
    PCSTR WINAPI GetUnnamedArgStr(_In_ ULONG Index) throw(...);
    ULONG64 WINAPI GetUnnamedArgU64(_In_ ULONG Index) throw(...);
    bool HasUnnamedArg(_In_ ULONG Index)
    {
        return Index < m_NumUnnamedArgs;
    }

    PCSTR WINAPI GetArgStr(_In_ PCSTR Name,
                    _In_ bool Required = true) throw(...);
    ULONG64 WINAPI GetArgU64(_In_ PCSTR Name,
                      _In_ bool Required = true) throw(...);
    bool HasArg(_In_ PCSTR Name)
    {
        return FindArg(Name, false) != NULL;
    }
    bool HasCharArg(_In_ CHAR Name)
    {
        CHAR NameStr[2] = {Name, 0};
        return FindArg(NameStr, false) != NULL;
    }

    bool WINAPI SetUnnamedArg(_In_ ULONG Index,
                              _In_opt_ PCSTR StrArg,
                              _In_ ULONG64 NumArg,
                              _In_ bool OnlyIfUnset = false) throw(...);
    bool SetUnnamedArgStr(_In_ ULONG Index,
                          _In_ PCSTR Arg,
                          _In_ bool OnlyIfUnset = false) throw(...)
    {
        return SetUnnamedArg(Index, Arg, 0, OnlyIfUnset);
    }
    bool SetUnnamedArgU64(_In_ ULONG Index,
                          _In_ ULONG64 Arg,
                          _In_ bool OnlyIfUnset = false) throw(...)
    {
        return SetUnnamedArg(Index, NULL, Arg, OnlyIfUnset);
    }

    bool WINAPI SetArg(_In_ PCSTR Name,
                       _In_opt_ PCSTR StrArg,
                       _In_ ULONG64 NumArg,
                       _In_ bool OnlyIfUnset = false) throw(...);
    bool SetArgStr(_In_ PCSTR Name,
                   _In_ PCSTR Arg,
                   _In_ bool OnlyIfUnset = false) throw(...)
    {
        return SetArg(Name, Arg, 0, OnlyIfUnset);
    }
    bool SetArgU64(_In_ PCSTR Name,
                   _In_ ULONG64 Arg,
                   _In_ bool OnlyIfUnset = false) throw(...)
    {
        return SetArg(Name, NULL, Arg, OnlyIfUnset);
    }

    PCSTR GetRawArgStr(void)
    {
        return m_RawArgStr;
    }
    PSTR GetRawArgCopy(void)
    {
        // This string may be chopped up if
        // the default argument parsing occurred.
        return m_ArgCopy;
    }
    
    PCSTR WINAPI GetExpr64(_In_ PCSTR Str,
                           _In_ bool Signed,
                           _In_ ULONG64 Limit,
                           _Out_ PULONG64 Val) throw(...);
    PCSTR GetExprU64(_In_ PCSTR Str,
                     _In_ ULONG64 Limit,
                     _Out_ PULONG64 Val) throw(...)
    {
        return GetExpr64(Str, false, Limit, Val);
    }
    PCSTR GetExprS64(_In_ PCSTR Str,
                     _In_ LONG64 Limit,
                     _Out_ PLONG64 Val) throw(...)
    {
        return GetExpr64(Str, true, (ULONG64)Limit, (PULONG64)Val);
    }

    ULONG64 EvalExprU64(_In_ PCSTR Str)
    {
        HRESULT Status;
        DEBUG_VALUE FullVal;
        
        if ((Status = m_Control->
             Evaluate(Str, DEBUG_VALUE_INT64, &FullVal, NULL)) != S_OK)
        {
            ThrowStatus(Status, "Unable to evaluate '%s'", Str);
        }

        return FullVal.I64;
    }

    void DECLSPEC_NORETURN ThrowCommandHelp(void) throw(...)
    {
        if (m_CurCommand)
        {
            HelpCommand(m_CurCommand);
        }
        throw ExtStatusException(E_INVALIDARG);
    }
    void ThrowInterrupt(void) throw(...)
    {
        if (m_Control->GetInterrupt() == S_OK)
        {
            throw ExtInterruptException();
        }
    }
    void DECLSPEC_NORETURN ThrowOutOfMemory(void) throw(...)
    {
        throw ExtStatusException(E_OUTOFMEMORY);
    }
    void DECLSPEC_NORETURN ThrowContinueSearch(void) throw(...)
    {
        throw ExtStatusException(DEBUG_EXTENSION_CONTINUE_SEARCH);
    }
    void DECLSPEC_NORETURN ThrowReloadExtension(void) throw(...)
    {
        throw ExtStatusException(DEBUG_EXTENSION_RELOAD_EXTENSION);
    }
    void DECLSPEC_NORETURN WINAPIV ThrowInvalidArg(_In_ PCSTR Format,
                                                    ...) throw(...);
    void DECLSPEC_NORETURN WINAPIV ThrowRemote(_In_ HRESULT Status,
                                               _In_ PCSTR Format,
                                               ...) throw(...);
    void DECLSPEC_NORETURN WINAPIV ThrowStatus(_In_ HRESULT Status,
                                               _In_ PCSTR Format,
                                               ...) throw(...);
    void DECLSPEC_NORETURN WINAPIV
        ThrowLastError(_In_opt_ PCSTR Message = NULL) throw(...)
        {
            ExtStatusException Ex(HRESULT_FROM_WIN32(GetLastError()),
                                  Message);
            throw Ex;
        }

    // Given a full EngExtCpp command method this
    // invokes the method with appropriate argument
    // parsing, Query/Release calls and exception handling.
    HRESULT CallCommand(_In_ ExtCommandDesc* Desc,
                        _In_ PDEBUG_CLIENT Client,
                        _In_opt_ PCSTR Args)
    {
        return CallExtCodeSEH(Desc, Client, Args,
                              NULL, NULL, NULL, NULL);
    }
    // If you're doing a hybrid dbgeng/EngExtCpp extension
    // and you have plain dbgeng code that wants to
    // call an EXT_CLASS method to do some work
    // this will invoke the method with appropriate
    // Query/Release calls and exception handling.
    // No argument parsing is done.
    // If a name is provided then normal failure/exception
    // error messages will be produced, just as
    // is done for a full extension method.
    HRESULT CallRawMethod(_In_ PDEBUG_CLIENT Client,
                          _In_ ExtRawMethod Method,
                          _In_opt_ PVOID Context,
                          _In_opt_ PCSTR Name = NULL)
    {
        return CallExtCodeSEH(NULL, Client, NULL,
                              Method, NULL, Context, Name);
    }
    // Similar to CallRawMethod except that the
    // code invoked is a plain function.
    HRESULT CallRawFunction(_In_ PDEBUG_CLIENT Client,
                            _In_ ExtRawFunction Function,
                            _In_opt_ PVOID Context,
                            _In_opt_ PCSTR Name = NULL)
    {
        return CallExtCodeSEH(NULL, Client, NULL,
                              NULL, Function, Context, Name);
    }

    //
    // Internal data.
    //

    static HMODULE s_Module;
    static char s_String[2000];
    static char s_CircleStringBuffer[2000];
    static char* s_CircleString;
    
    ExtCommandDesc* m_Commands;
    ULONG m_LongestCommandName;
    HRESULT m_CallStatus;
    HRESULT m_MacroStatus;

    typedef BOOL (WINAPI *PFN_SymMatchStringA)(_In_ PCSTR string,
                                               _In_ PCSTR expression,
                                               _In_ BOOL fCase);
    HMODULE m_DbgHelp;
    PFN_SymMatchStringA m_SymMatchStringA;
    
    struct ArgVal
    {
        PCSTR Name;
        PCSTR StrVal;
        ULONG64 NumVal;
    };
    static const ULONG s_MaxArgs = 64;

    ExtCommandDesc* m_CurCommand;
    PCSTR m_RawArgStr;
    PSTR m_ArgCopy;
    ULONG m_NumArgs;
    ULONG m_NumNamedArgs;
    ULONG m_NumUnnamedArgs;
    ULONG m_FirstNamedArg;
    // Unnamed args are packed in the front.
    ArgVal m_Args[s_MaxArgs];

    // Register index caches are cleared in QueryMachineInfo.
    ULONG m_ExtRetIndex;
    ULONG m_TempRegIndex[20];
    
    bool m_ExInitialized;
    
    void WINAPI ExInitialize(void) throw(...);

    HRESULT WINAPI QueryMachineInfo(void);
    HRESULT WINAPI Query(_In_ PDEBUG_CLIENT Start);
    void WINAPI Release(void);

    HRESULT WINAPI CallExtCodeCEH(_In_opt_ ExtCommandDesc* Desc,
                                  _In_opt_ PCSTR Args,
                                  _In_opt_ ExtRawMethod RawMethod,
                                  _In_opt_ ExtRawFunction RawFunction,
                                  _In_opt_ PVOID Context,
                                  _In_opt_ PCSTR RawName);
    HRESULT WINAPI CallExtCodeSEH(_In_opt_ ExtCommandDesc* Desc,
                                  _In_ PDEBUG_CLIENT Client,
                                  _In_opt_ PCSTR Args,
                                  _In_opt_ ExtRawMethod RawMethod,
                                  _In_opt_ ExtRawFunction RawFunction,
                                  _In_opt_ PVOID Context,
                                  _In_opt_ PCSTR RawName);
    
    HRESULT WINAPI CallKnownStructMethod(_In_ ExtKnownStruct* Struct,
                                         _In_ ULONG Flags,
                                         _In_ ULONG64 Offset,
                                         _Out_writes_(*BufferChars) PSTR Buffer,
                                         _Inout_ PULONG BufferChars);
    HRESULT WINAPI CallKnownStruct(_In_ PDEBUG_CLIENT Client,
                                   _In_ ExtKnownStruct* Struct,
                                   _In_ ULONG Flags,
                                   _In_ ULONG64 Offset,
                                   _Out_writes_(*BufferChars) PSTR Buffer,
                                   _Inout_ PULONG BufferChars);
    HRESULT WINAPI HandleKnownStruct(_In_ PDEBUG_CLIENT Client,
                                     _In_ ULONG Flags,
                                     _In_ ULONG64 Offset,
                                     _In_ PCSTR TypeName,
                                     _Out_writes_opt_(*BufferChars) PSTR Buffer,
                                     _Inout_ PULONG BufferChars);

    HRESULT WINAPI HandleQueryValueNames(_In_ PDEBUG_CLIENT Client,
                                         _In_ ULONG Flags,
                                         _Out_writes_(BufferChars) PWSTR Buffer,
                                         _In_ ULONG BufferChars,
                                         _Out_ PULONG BufferNeeded);
    HRESULT WINAPI CallProvideValueMethod(_In_ ExtProvidedValue* ExtVal,
                                          _In_ ULONG Flags,
                                          _Out_ PULONG64 Value,
                                          _Out_ PULONG64 TypeModBase,
                                          _Out_ PULONG TypeId,
                                          _Out_ PULONG TypeFlags);
    HRESULT WINAPI HandleProvideValue(_In_ PDEBUG_CLIENT Client,
                                      _In_ ULONG Flags,
                                      _In_ PCWSTR Name,
                                      _Out_ PULONG64 Value,
                                      _Out_ PULONG64 TypeModBase,
                                      _Out_ PULONG TypeId,
                                      _Out_ PULONG TypeFlags);

    ArgVal* WINAPI FindArg(_In_ PCSTR Name,
                           _In_ bool Required) throw(...);
    PCSTR WINAPI SetRawArgVal(_In_ ExtCommandDesc::ArgDesc* Check,
                              _In_opt_ ArgVal* Val,
                              _In_ bool ExplicitVal,
                              _In_opt_ PCSTR StrVal,
                              _In_ bool StrWritable,
                              _In_ ULONG64 NumVal) throw(...);
    void WINAPI ParseArgs(_In_ ExtCommandDesc* Desc,
                          _In_opt_ PCSTR Args) throw(...);

    void WINAPI OutCommandArg(_In_ ExtCommandDesc::ArgDesc* Arg,
                              _In_ bool Separate);
    void WINAPI HelpCommandArgsSummary(_In_ ExtCommandDesc* Desc);
    void WINAPI OutArgDescOptions(_In_ ExtCommandDesc::ArgDesc* Arg);
    void WINAPI HelpCommand(_In_ ExtCommandDesc* Desc);
    void WINAPI HelpCommandName(_In_ PCSTR Name);
    void WINAPI HelpAll(void);
    void __thiscall help(void);
};

//----------------------------------------------------------------------------
//
// Global forwarders for common methods.
//
//----------------------------------------------------------------------------

#if !defined(EXT_NO_OUTPUT_FUNCTIONS)

void WINAPIV ExtOut(_In_ PCSTR Format, ...);
void WINAPIV ExtWarn(_In_ PCSTR Format, ...);
void WINAPIV ExtErr(_In_ PCSTR Format, ...);
void WINAPIV ExtVerb(_In_ PCSTR Format, ...);

#endif // #if !defined(EXT_NO_OUTPUT_FUNCTIONS)

//----------------------------------------------------------------------------
//
// Supporting macros and utilities.
//
//----------------------------------------------------------------------------

// If you wish to override the class name that is used
// as the derivation from ExtExtension define it
// before including this file.  Otherwise the class
// will be named 'Extension'.
#ifndef EXT_CLASS
#define EXT_CLASS Extension
#endif

extern ExtCheckedPointer<ExtExtension> g_Ext;
extern ExtExtension* g_ExtInstancePtr;

// Put a single use of this macro in one source file.
#define EXT_DECLARE_GLOBALS() \
EXT_CLASS g_ExtInstance; \
ExtExtension* g_ExtInstancePtr = &g_ExtInstance

// Use this macro to forward-declare a command method in your class
// declaration.
#define EXT_COMMAND_METHOD(_Name) \
void _Name(void)

//----------------------------------------------------------------------------
//
// Use this macro to declare an extension command implementation.  It
// will declare the base function that will be exported and
// will start a method on your class for the command
// implementation.
//
// The description string given will automatically be wrapped to
// fit the space it is being displayed in.  Newlines can be embedded
// to force a new line but are not necessary for formatting.
//
// The argument string describes the arguments expected by the
// command.  It is a sequence of the following two major components.
//
// Directives: {{<directive>}}
//
// Indicates a special non-argument directive.  Directives are:
//   custom - Extension does its own argument parsing.
//            Default parsing is disabled.
//   l:<str> - Custom long argument description.  The
//             long argument description is a full description
//             for each argument.
//   opt:<str> - Defines the option prefix characters for
//               commands that don't want to use the default
//               / and -.
//   s:<str> - Custom short argument description.  The
//             short argument description is the argument summary
//             shown with the command name.
//
// Examples:
//
//   {{custom}}{{s:<arg1> <arg2>}}{{l:arg1 - Argument 1\narg2 - Argument 2}}
//
// This defines an extension command that parses its own arguments.
// Such a command should give custom help strings so that the automatic
// !help support has something to display, such as the short and
// long descriptions given here.
//
//   {{opt:+:}}
//
// This changes the argument option prefix characters to + and :,
// so that +arg and :arg can be used instead of /arg and -arg.
//
// Arguments: {[<optname>];[<type>[,<flags>]];[<argname>];[<argdesc>]}
//
// Defines an argument for the extension.  An argument
// has several parts.
//
//   <optname> - Gives the argument's option name that is given
//               in an argument string to pass the argument.
//               Arguments can be unnamed if they are going
//               to be handed positionally.  Unnamed arguments
//               are processed in the order given.
//
//   <type> - Indicates the type of the argument.  The possibilities are:
//            b - Boolean (present/not-present) argument, for flags.
//            e[d][n=(<radix>)][s][v=(<eval>)][<bits>] -
//                Expression argument for getting numeric values.
//                d - Indicates that the expression should be limited
//                    to the next space-delimited subset of the overall
//                    argument string.  This prevents accidental evaluation
//                    of other data following the expression and so
//                    can avoid otherwise unnecessary symbol resolution.
//                n=(<radix>) - Gives a default radix for
//                              expression evaluation.
//                s - Indicates the value is signed and a
//                    bit-size limit can be given for values
//                    that are less than 64-bit.
//                v=(<eval>) - Names an expression evaluator to use
//                             for the expression.
//            s - Space-delimited string argument.
//            x - String-to-end-of-args string argument.
//
//   <flags> - Modifies argument behavior.
//             d=<expr> - Sets default value for argument.
//             ds - Indicates that the default value should not be
//                  displayed in an argument description.
//             o - Argument is optional (default for named arguments).
//             r - Argument is required (default for unnamed arguments).
//
//   <argname> - Argument name to show for the value in help output.
//               This is separate from the option name for non-boolean
//               arguments since they can have both a name and a value.
//               For boolean arguments the argument name is not used.
//
//   <argdesc> - Long argument description to show in help output.
//
// Examples:
//
//   {;en=(10)32,o,d=0x100;flags;Flags to control command}
//
// This defines a command with a single optional expression argument.  The
// argument value will be interpreted in base 10 and must fit in 32 bits.
// If the argument isn't specified the default value of 0x100 will be used.
//
//   {v;b;;Verbose mode}{;s;name;Name of object}
//
// This defines a command with an optional boolean /v and a required
// unnamed string argument.
//
//   {oname;e;expr;Address of object}{eol;x;str;Commands to use}
//
// This defines a command which has an optional expression argument
// /oname <expr> and an optional end-of-string argument /eol <str>.
// If /eol is present it will get the remainder of the command string
// and no further arguments will be parsed.
// 
// /? is automatically provided for all commands unless custom
// argument parsing is indicated.
//
// A NULL or empty argument string indicates no arguments.
// Commands are currently limited to a maximum of 64 arguments.
//
//----------------------------------------------------------------------------

// If your extension has direct dbgeng-style extensions that
// do not use the EngExtCpp entry wrappers you can still create command
// descriptions for them so that the auto-help implementation
// can display help for them along with EngExtCpp methods.
// These descs must always be global.
#define EXT_EXPLICIT_COMMAND_DESC(_Name, _Desc, _Args)                        \
ExtCommandDesc g_##_Name##Desc(#_Name,                                        \
                               NULL,                                          \
                               _Desc,                                         \
                               _Args)

#define EXT_CLASS_COMMAND(_Class, _Name, _Desc, _Args)                        \
ExtCommandDesc g_##_Name##Desc(#_Name,                                        \
                               (ExtCommandMethod)&_Class::_Name,              \
                               _Desc,                                         \
                               _Args);                                        \
EXTERN_C HRESULT CALLBACK                                                     \
_Name(_In_ PDEBUG_CLIENT Client,                                              \
      _In_opt_ PCSTR Args)                                                    \
{                                                                             \
    if (!g_Ext.IsSet())                                                       \
    {                                                                         \
        return E_UNEXPECTED;                                                  \
    }                                                                         \
    return g_Ext->CallCommand(&g_##_Name##Desc, Client, Args);                \
}                                                                             \
void _Class::_Name(void)
#define EXT_COMMAND(_Name, _Desc, _Args) \
    EXT_CLASS_COMMAND(EXT_CLASS, _Name, _Desc, _Args)

// Checks for success and throws an exception for failure.
#define EXT_STATUS(_Expr)                                                     \
    if (FAILED(m_MacroStatus = (_Expr)))                                      \
    {                                                                         \
        throw ExtStatusException(m_MacroStatus);                              \
    } else 0
#define EXT_STATUS_MSG(_Expr, _Msg)                                           \
    if (FAILED(m_MacroStatus = (_Expr)))                                      \
    {                                                                         \
        throw ExtStatusException(m_MacroStatus, _Msg);                        \
    } else 0
#define EXT_STATUS_EMSG(_Expr)                                                \
    if (FAILED(m_MacroStatus = (_Expr)))                                      \
    {                                                                         \
        throw ExtStatusException(m_MacroStatus, #_Expr);                      \
    } else 0

//----------------------------------------------------------------------------
//
// ExtRemoteData is a simple wrapper for a piece of debuggee memory.
// It automatically retrieves small data items and wraps
// other common requests with throwing methods.
//
// Data can be named for more meaningful error messages.
//
//----------------------------------------------------------------------------

class ExtRemoteData
{
public:
    ExtRemoteData(void)
    {
        Clear();
    }
    ExtRemoteData(_In_ ULONG64 Offset,
                  _In_ ULONG Bytes) throw(...)
    {
        Clear();
        Set(Offset, Bytes);
    }
    ExtRemoteData(_In_opt_ PCSTR Name,
                  _In_ ULONG64 Offset,
                  _In_ ULONG Bytes) throw(...)
    {
        Clear();
        m_Name = Name;
        Set(Offset, Bytes);
    }
    
    void Set(_In_ ULONG64 Offset,
             _In_ ULONG Bytes) throw(...)
    {
        m_Offset = Offset;
        m_ValidOffset = true;
        m_Bytes = Bytes;
        if (Bytes <= sizeof(m_Data))
        {
            Read();
        }
        else
        {
            m_ValidData = false;
            m_Data = 0;
        }
    }
    void WINAPI Set(_In_ const DEBUG_TYPED_DATA* Typed);

    void WINAPI Read(void) throw(...);
    void WINAPI Write(void) throw(...);

    ULONG64 WINAPI GetData(_In_ ULONG Request) throw(...);
    void WINAPI SetData(_In_ ULONG64 Data,
                        _In_ ULONG Request,
                        _In_ bool NoWrite = false) throw(...);

    //
    // Fixed-size primitive type accesses.
    // Accesses are validated against the known data size.
    //
    
    CHAR GetChar(void) throw(...)
    {
        return (CHAR)GetData(sizeof(CHAR));
    }
    UCHAR GetUchar(void) throw(...)
    {
        return (UCHAR)GetData(sizeof(UCHAR));
    }
    BOOLEAN GetBoolean(void) throw(...)
    {
        return (BOOLEAN)GetData(sizeof(BOOLEAN));
    }
    bool GetStdBool(void) throw(...)
    {
        return GetData(sizeof(bool)) != 0;
    }
    BOOL GetW32Bool(void) throw(...)
    {
        return (BOOL)GetData(sizeof(BOOL));
    }
    SHORT GetShort(void) throw(...)
    {
        return (SHORT)GetData(sizeof(SHORT));
    }
    USHORT GetUshort(void) throw(...)
    {
        return (USHORT)GetData(sizeof(USHORT));
    }
    LONG GetLong(void) throw(...)
    {
        return (LONG)GetData(sizeof(LONG));
    }
    ULONG GetUlong(void) throw(...)
    {
        return (ULONG)GetData(sizeof(ULONG));
    }
    LONG64 GetLong64(void) throw(...)
    {
        return (LONG64)GetData(sizeof(LONG64));
    }
    ULONG64 GetUlong64(void) throw(...)
    {
        return (ULONG64)GetData(sizeof(ULONG64));
    }
    float GetFloat(void) throw(...)
    {
        GetData(sizeof(float));
        return *(float *)&m_Data;
    }
    double GetDouble(void) throw(...)
    {
        GetData(sizeof(double));
        return *(double *)&m_Data;
    }
    
    void SetChar(_In_ CHAR Data) throw(...)
    {
        SetData(Data, sizeof(CHAR));
    }
    void SetUchar(_In_ UCHAR Data) throw(...)
    {
        SetData(Data, sizeof(UCHAR));
    }
    void SetBoolean(_In_ BOOLEAN Data) throw(...)
    {
        SetData(Data, sizeof(BOOLEAN));
    }
    void SetStdBool(_In_ bool Data) throw(...)
    {
        SetData(Data, sizeof(bool));
    }
    void SetW32Bool(_In_ BOOL Data) throw(...)
    {
        SetData(Data, sizeof(BOOL));
    }
    void SetShort(_In_ SHORT Data) throw(...)
    {
        SetData(Data, sizeof(SHORT));
    }
    void SetUshort(_In_ USHORT Data) throw(...)
    {
        SetData(Data, sizeof(USHORT));
    }
    void SetLong(_In_ LONG Data) throw(...)
    {
        SetData(Data, sizeof(LONG));
    }
    void SetUlong(_In_ ULONG Data) throw(...)
    {
        SetData(Data, sizeof(ULONG));
    }
    void SetLong64(_In_ LONG64 Data) throw(...)
    {
        SetData(Data, sizeof(LONG64));
    }
    void SetUlong64(_In_ ULONG64 Data) throw(...)
    {
        SetData(Data, sizeof(ULONG64));
    }
    void SetFloat(_In_ float Data) throw(...)
    {
        SetData(*(ULONG*)&Data, sizeof(float));
    }
    void SetDouble(_In_ double Data) throw(...)
    {
        SetData(*(ULONG64*)&Data, sizeof(double));
    }
    
    //
    // Pointer-size primitive type queries.
    // The data is always promoted to the largest size.
    // Accesses are validated against the known data size.
    //
    
    LONG64 GetLongPtr(void) throw(...)
    {
        return g_Ext->m_PtrSize == 8 ?
            (LONG64)GetData(g_Ext->m_PtrSize) :
            (LONG)GetData(g_Ext->m_PtrSize);
    }
    ULONG64 GetUlongPtr(void) throw(...)
    {
        return (ULONG64)GetData(g_Ext->m_PtrSize);
    }

    //
    // Pointer-size primitive type wries.
    // The data is always written with the current pointer size.
    // Accesses are validated against the known data size.
    //
    
    void SetLongPtr(_In_ LONG64 Data) throw(...)
    {
        SetData(Data, g_Ext->m_PtrSize);
    }
    void SetUlongPtr(_In_ ULONG64 Data) throw(...)
    {
        SetData(Data, g_Ext->m_PtrSize);
    }

    //
    // Pointer data read, with automatic sign extension.
    //
    
    ULONG64 GetPtr(void) throw(...)
    {
        return g_Ext->m_PtrSize == 8 ?
            GetData(g_Ext->m_PtrSize) :
            (LONG)GetData(g_Ext->m_PtrSize);
    }

    //
    // Pointer data write, using the current pointer size.
    //
    
    void SetPtr(_In_ ULONG64 Data) throw(...)
    {
        SetData(Data, g_Ext->m_PtrSize);
    }

    //
    // Buffer reads for larger data.
    //

    ULONG WINAPI ReadBuffer(_Out_writes_bytes_(Bytes) PVOID Buffer,
                            _In_ ULONG Bytes,
                            _In_ bool MustReadAll = true) throw(...);
    ULONG WINAPI WriteBuffer(_In_reads_bytes_(Bytes) PVOID Buffer,
                             _In_ ULONG Bytes,
                             _In_ bool MustWriteAll = true) throw(...);
    
    //
    // String reads.
    // If you are only reading the string in order
    // to output it it's easier to use %ma/%mu in
    // your Out() call so that dbgeng handles the
    // string read for you.
    //

    PSTR WINAPI GetString(_Out_writes_opt_(BufferChars) PSTR Buffer,
                          _In_ ULONG BufferChars,
                          _In_ ULONG MaxChars = 1024,
                          _In_ bool MustFit = false,
                          _Out_opt_ PULONG NeedChars = NULL) throw(...);
    PWSTR WINAPI GetString(_Out_writes_opt_(BufferChars) PWSTR Buffer,
                           _In_ ULONG BufferChars,
                           _In_ ULONG MaxChars = 1024,
                           _In_ bool MustFit = false,
                           _Out_opt_ PULONG NeedChars = NULL) throw(...);
    PSTR WINAPI GetString(_Inout_ ExtBuffer<char>* Buffer,
                          _In_ ULONG MaxChars = 1024) throw(...);
    PWSTR WINAPI GetString(_Inout_ ExtBuffer<WCHAR>* Buffer,
                           _In_ ULONG MaxChars = 1024) throw(...);
    
    PCSTR m_Name;
    ULONG64 m_Offset;
    bool m_ValidOffset;
    ULONG m_Bytes;
    ULONG64 m_Data;
    bool m_ValidData;
    bool m_Physical;
    ULONG m_SpaceFlags;

protected:
    void Clear(void)
    {
        m_Name = NULL;
        m_Offset = 0;
        m_ValidOffset = false;
        m_Bytes = 0;
        m_Data = 0;
        m_ValidData = false;
        m_Physical = false;
        m_SpaceFlags = 0;
    }
};

//----------------------------------------------------------------------------
//
// ExtRemoteTyped is an enhanced remote data object that understands
// data typed with type information from symbols.  It is initialized
// to a particular object by symbol or cast, after which it can
// be used like an object of the given type.
//
// All expressions are C++ syntax by default.
//
//----------------------------------------------------------------------------

class ExtRemoteTyped : public ExtRemoteData
{
public:
    ExtRemoteTyped(void)
    {
        Clear();
    }
    ExtRemoteTyped(_In_ PCSTR Expr) throw(...)
    {
        m_Release = false;
        Set(Expr);
    }
    ExtRemoteTyped(_In_ const DEBUG_TYPED_DATA* Typed) throw(...)
    {
        m_Release = false;
        Copy(Typed);
    }
    ExtRemoteTyped(_In_ const ExtRemoteTyped& Typed) throw(...)
    {
        m_Release = false;
        Copy(Typed);
    }
    ExtRemoteTyped(_In_ PCSTR Expr,
                   _In_ ULONG64 Offset) throw(...)
    {
        m_Release = false;
        Set(Expr, Offset);
    }
    ExtRemoteTyped(_In_ PCSTR Type,
                   _In_ ULONG64 Offset,
                   _In_ bool PtrTo,
                   _Inout_opt_ PULONG64 CacheCookie = NULL,
                   _In_opt_ PCSTR LinkField = NULL) throw(...)
    {
        m_Release = false;
        Set(Type, Offset, PtrTo, CacheCookie, LinkField);
    }
    ~ExtRemoteTyped(void)
    {
        Release();
    }

    ExtRemoteTyped& operator=(_In_ const DEBUG_TYPED_DATA* Typed) throw(...)
    {
        Copy(Typed);
        return *this;
    }
    ExtRemoteTyped& operator=(_In_ const ExtRemoteTyped& Typed) throw(...)
    {
        Copy(Typed);
        return *this;
    }
    
    void WINAPI Copy(_In_ const DEBUG_TYPED_DATA* Typed) throw(...);
    void Copy(_In_ const ExtRemoteTyped& Typed) throw(...)
    {
        if (Typed.m_Release)
        {
            Copy(&Typed.m_Typed);
        }
        else
        {
            Clear();
        }
    }
    
    void WINAPI Set(_In_ PCSTR Expr) throw(...);
    void WINAPI Set(_In_ PCSTR Expr,
                    _In_ ULONG64 Offset) throw(...);
    void WINAPI Set(_In_ bool PtrTo,
                    _In_ ULONG64 TypeModBase,
                    _In_ ULONG TypeId,
                    _In_ ULONG64 Offset) throw(...);
    void WINAPI Set(_In_ PCSTR Type,
                    _In_ ULONG64 Offset,
                    _In_ bool PtrTo,
                    _Inout_opt_ PULONG64 CacheCookie = NULL,
                    _In_opt_ PCSTR LinkField = NULL) throw(...);

    // Uses a circle string.
    void WINAPIV SetPrint(_In_ PCSTR Format,
                          ...) throw(...);

    bool HasField(_In_ PCSTR Field)
    {
        return ErtIoctl("HasField",
                        EXT_TDOP_HAS_FIELD,
                        ErtIn | ErtIgnoreError,
                        Field) == S_OK;
    }

    ULONG GetTypeSize(void) throw(...)
    {
        ULONG Size;
        
        ErtIoctl("GetTypeSize", EXT_TDOP_GET_TYPE_SIZE, ErtIn,
                 NULL, 0, NULL, NULL, 0, &Size);
        return Size;
    }
    
    ULONG WINAPI GetFieldOffset(_In_ PCSTR Field) throw(...);
    
    ExtRemoteTyped WINAPI Field(_In_ PCSTR Field) throw(...);
    ExtRemoteTyped WINAPI ArrayElement(_In_ LONG64 Index) throw(...);
    ExtRemoteTyped WINAPI Dereference(void) throw(...);
    ExtRemoteTyped WINAPI GetPointerTo(void) throw(...);
    ExtRemoteTyped WINAPI Eval(_In_ PCSTR Expr) throw(...);

    ExtRemoteTyped operator[](_In_ LONG Index)
    {
        return ArrayElement(Index);
    }
    ExtRemoteTyped operator[](_In_ ULONG Index)
    {
        return ArrayElement((LONG64)Index);
    }
    ExtRemoteTyped operator[](_In_ LONG64 Index)
    {
        return ArrayElement(Index);
    }
    ExtRemoteTyped operator[](_In_ ULONG64 Index)
    {
        if (Index > 0x7fffffffffffffffUI64)
        {
            g_Ext->ThrowRemote
                (HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
                 "Array index too large");
        }
        return ArrayElement((LONG64)Index);
    }
    ExtRemoteTyped operator*(void)
    {
        return Dereference();
    }
    
    // Uses the circular string buffer.
    PSTR WINAPI GetTypeName(void) throw(...);
    // Uses the circular string buffer.
    PSTR WINAPI GetSimpleValue(void) throw(...);
    
    void OutTypeName(void) throw(...)
    {
        ErtIoctl("OutTypeName", EXT_TDOP_OUTPUT_TYPE_NAME, ErtIn);
    }
    void OutSimpleValue(void) throw(...)
    {
        ErtIoctl("OutSimpleValue", EXT_TDOP_OUTPUT_SIMPLE_VALUE, ErtIn);
    }
    void OutFullValue(void) throw(...)
    {
        ErtIoctl("OutFullValue", EXT_TDOP_OUTPUT_FULL_VALUE, ErtIn);
    }
    void OutTypeDefinition(void) throw(...)
    {
        ErtIoctl("OutTypeDefinition", EXT_TDOP_OUTPUT_TYPE_DEFINITION, ErtIn);
    }
    
    void Release(void)
    {
        if (m_Release)
        {
            ErtIoctl("Release", EXT_TDOP_RELEASE, ErtIn | ErtIgnoreError);
            Clear();
        }
    }

    static ULONG WINAPI GetTypeFieldOffset(_In_ PCSTR Type,
                                           _In_ PCSTR Field) throw(...);

    DEBUG_TYPED_DATA m_Typed;
    bool m_Release;

protected:
    static const ULONG ErtIn          = 0x00000001;
    static const ULONG ErtOut         = 0x00000002;
    static const ULONG ErtUncheckedIn = 0x00000004;
    static const ULONG ErtIgnoreError = 0x00000008;
    
    HRESULT WINAPI ErtIoctl(_In_ PCSTR Message,
                            _In_ EXT_TDOP Op,
                            _In_ ULONG Flags,
                            _In_opt_ PCSTR InStr = NULL,
                            _In_ ULONG64 In64 = 0,
                            _Out_opt_ ExtRemoteTyped* Ret = NULL,
                            _Out_writes_opt_(StrBufferChars) PSTR StrBuffer = NULL,
                            _In_ ULONG StrBufferChars = 0,
                            _Out_opt_ PULONG Out32 = NULL);
    void WINAPI Clear(void);
};

//----------------------------------------------------------------------------
//
// ExtRemoteList wraps a basic singly- or double-linked list.
// It can iterate over the list and retrieve nodes both
// forwards and backwards.  It handles both NULL-terminated
// and lists that are circular through a head pointer (NT-style).
//
// When doubly-linked it is assumed that the previous
// pointer immediately follows the next pointer.
//
//----------------------------------------------------------------------------

class ExtRemoteList
{
public:
    ExtRemoteList(_In_ ULONG64 Head,
                  _In_ ULONG LinkOffset,
                  _In_ bool Double = false)
    {
        m_Head = Head;
        m_LinkOffset = LinkOffset;
        m_Double = Double;
        m_MaxIter = 65536;
    }
    ExtRemoteList(_In_ ExtRemoteData& Head,
                  _In_ ULONG LinkOffset,
                  _In_ bool Double = false)
    {
        m_Head = Head.m_Offset;
        m_LinkOffset = LinkOffset;
        m_Double = Double;
        m_MaxIter = 65536;
    }

    void StartHead(void)
    {
        m_Node.Set(m_Head, g_Ext->m_PtrSize);
        m_CurIter = 0;
    }
    void StartTail(void)
    {
        if (!m_Double)
        {
            g_Ext->ThrowRemote(E_INVALIDARG,
                               "ExtRemoteList is singly-linked");
        }
        
        m_Node.Set(m_Head + g_Ext->m_PtrSize, g_Ext->m_PtrSize);
        m_CurIter = 0;
    }

    bool
    IsValid(
        ULONG64 Pointer
    )
    {
            UCHAR Buffer[4];
            ULONG BytesRead;

            HRESULT hResult = g_Ext->m_Data->ReadVirtual(Pointer, Buffer, sizeof(Buffer), &BytesRead);
            if (hResult != S_OK) return FALSE;

            return TRUE;
    }

    bool HasNode(void)
    {
        g_Ext->ThrowInterrupt();
        ULONG64 NodeOffs = m_Node.GetPtr();
        return NodeOffs != 0 && NodeOffs != m_Head && IsValid(NodeOffs);
    }
    ULONG64 GetNodeOffset(void)
    {
        return m_Node.GetPtr() - m_LinkOffset;
    }
    void Next(void)
    {
        if (++m_CurIter > m_MaxIter)
        {
            g_Ext->ThrowRemote(E_INVALIDARG,
                               "List iteration count exceeded, loop assumed");
        }
        
        m_Node.Set(m_Node.GetPtr(), g_Ext->m_PtrSize);
    }
    void Prev(void)
    {
        g_Ext->ThrowInterrupt();

        if (!m_Double)
        {
            g_Ext->ThrowRemote(E_INVALIDARG,
                               "ExtRemoteList is singly-linked");
        }
        
        if (++m_CurIter > m_MaxIter)
        {
            g_Ext->ThrowRemote(E_INVALIDARG,
                               "List iteration count exceeded, loop assumed");
        }
        
        m_Node.Set(m_Node.GetPtr() + g_Ext->m_PtrSize, g_Ext->m_PtrSize);
    }
    
    ULONG64 m_Head;
    ULONG m_LinkOffset;
    bool m_Double;
    ULONG m_MaxIter;
    ExtRemoteData m_Node;
    ULONG m_CurIter;
};

//----------------------------------------------------------------------------
//
// ExtRemoteTypedList enhances the basic ExtRemoteList to
// understand the type of the nodes in the list and to
// automatically determine link offsets from type information.
//
//----------------------------------------------------------------------------

class ExtRemoteTypedList : public ExtRemoteList
{
public:
    ExtRemoteTypedList(_In_ ULONG64 Head,
                       _In_ PCSTR Type,
                       _In_ PCSTR LinkField,
                       _In_ ULONG64 TypeModBase = 0,
                       _In_ ULONG TypeId = 0,
                       _Inout_opt_ PULONG64 CacheCookie = NULL,
                       _In_ bool Double = false) throw(...)
        : ExtRemoteList(Head, 0, Double)
    {
        SetTypeAndLink(Type, LinkField, TypeModBase, TypeId, CacheCookie);
    }
    ExtRemoteTypedList(_In_ ExtRemoteData& Head,
                       _In_ PCSTR Type,
                       _In_ PCSTR LinkField,
                       _In_ ULONG64 TypeModBase = 0,
                       _In_ ULONG TypeId = 0,
                       _Inout_opt_ PULONG64 CacheCookie = NULL,
                       _In_ bool Double = false) throw(...)
        : ExtRemoteList(Head, 0, Double)
    {
        SetTypeAndLink(Type, LinkField, TypeModBase, TypeId, CacheCookie);
    }

    void SetTypeAndLink(_In_ PCSTR Type,
                        _In_ PCSTR LinkField,
                        _In_ ULONG64 TypeModBase = 0,
                        _In_ ULONG TypeId = 0,
                        _Inout_opt_ PULONG64 CacheCookie = NULL) throw(...)
    {
        m_Type = Type;
        m_TypeModBase = TypeModBase;
        m_TypeId = TypeId;
        if (CacheCookie)
        {
            m_LinkOffset = g_Ext->GetCachedFieldOffset(CacheCookie,
                                                       Type,
                                                       LinkField,
                                                       &m_TypeModBase,
                                                       &m_TypeId);
        }
        else
        {
            m_LinkOffset = ExtRemoteTyped::GetTypeFieldOffset(Type, LinkField);
        }
    }

    ExtRemoteTyped GetTypedNodePtr(void) throw(...)
    {
        ExtRemoteTyped Typed;

        if (m_TypeId)
        {
            Typed.Set(true, m_TypeModBase, m_TypeId,
                      m_Node.GetPtr() - m_LinkOffset);
        }
        else
        {
            Typed.SetPrint("(%s*)0x%I64x",
                           m_Type, m_Node.GetPtr() - m_LinkOffset);

            // Save the type info so that future nodes
            // can be resolved without needing
            // expression evaluation.
            ExtRemoteTyped Deref = Typed.Dereference();
            m_TypeModBase = Deref.m_Typed.ModBase;
            m_TypeId = Deref.m_Typed.TypeId;
        }
        return Typed;
    }
    ExtRemoteTyped GetTypedNode(void) throw(...)
    {
        ExtRemoteTyped Typed;
        
        if (m_TypeId)
        {
            Typed.Set(false, m_TypeModBase, m_TypeId,
                      m_Node.GetPtr() - m_LinkOffset);
        }
        else
        {
            Typed.SetPrint("*(%s*)0x%I64x",
                           m_Type, m_Node.GetPtr() - m_LinkOffset);

            // Save the type info so that future nodes
            // can be resolved without needing
            // expression evaluation.
            m_TypeModBase = Typed.m_Typed.ModBase;
            m_TypeId = Typed.m_Typed.TypeId;
        }
        return Typed;
    }

    PCSTR m_Type;
    ULONG64 m_TypeModBase;
    ULONG m_TypeId;
};

//----------------------------------------------------------------------------
//
// Helpers for handling well-known NT data and types.
//
//----------------------------------------------------------------------------

class ExtNtOsInformation
{
public:
    //
    // Kernel mode.
    //
    
    static ULONG64 WINAPI GetKernelLoadedModuleListHead(void);
    static ExtRemoteTypedList WINAPI GetKernelLoadedModuleList(void);
    static ExtRemoteTyped WINAPI GetKernelLoadedModule(_In_ ULONG64 Offset);
    
    static ULONG64 WINAPI GetKernelProcessListHead(void);
    static ExtRemoteTypedList WINAPI GetKernelProcessList(void);
    static ExtRemoteTyped WINAPI GetKernelProcess(_In_ ULONG64 Offset);

    static ULONG64 WINAPI GetKernelProcessThreadListHead(_In_ ULONG64 Process);
    static ExtRemoteTypedList WINAPI GetKernelProcessThreadList(_In_ ULONG64 Process);
    static ExtRemoteTyped WINAPI GetKernelThread(_In_ ULONG64 Offset);
    
    //
    // User mode.
    //

    static ULONG64 WINAPI GetUserLoadedModuleListHead(_In_ bool NativeOnly = false);
    static ExtRemoteTypedList
        WINAPI GetUserLoadedModuleList(_In_ bool NativeOnly = false);
    static ExtRemoteTyped WINAPI GetUserLoadedModule(_In_ ULONG64 Offset,
                                              _In_ bool NativeOnly = false);

    //
    // PEB and TEB.
    //
    // The alternate PEB and TEB are secondary PEB and TEB
    // data, such as the 32-bit PEB and TEB in a WOW64
    // debugging session.  They may or may not be defined
    // depending on the session.
    //

    static ULONG64 WINAPI GetOsPebPtr(void);
    static ExtRemoteTyped WINAPI GetOsPeb(_In_ ULONG64 Offset);
    static ExtRemoteTyped GetOsPeb(void)
    {
        return GetOsPeb(GetOsPebPtr());
    }
    
    static ULONG64 WINAPI GetOsTebPtr(void);
    static ExtRemoteTyped WINAPI GetOsTeb(_In_ ULONG64 Offset);
    static ExtRemoteTyped GetOsTeb(void)
    {
        return GetOsTeb(GetOsTebPtr());
    }
    
    static ULONG64 WINAPI GetAltPebPtr(void);
    static ExtRemoteTyped WINAPI GetAltPeb(_In_ ULONG64 Offset);
    static ExtRemoteTyped GetAltPeb(void)
    {
        return GetAltPeb(GetAltPebPtr());
    }
    
    static ULONG64 WINAPI GetAltTebPtr(void);
    static ExtRemoteTyped WINAPI GetAltTeb(_In_ ULONG64 Offset);
    static ExtRemoteTyped GetAltTeb(void)
    {
        return GetAltTeb(GetAltTebPtr());
    }
    
    static ULONG64 WINAPI GetCurPebPtr(void);
    static ExtRemoteTyped WINAPI GetCurPeb(_In_ ULONG64 Offset);
    static ExtRemoteTyped GetCurPeb(void)
    {
        return GetCurPeb(GetCurPebPtr());
    }
    
    static ULONG64 WINAPI GetCurTebPtr(void);
    static ExtRemoteTyped WINAPI GetCurTeb(_In_ ULONG64 Offset);
    static ExtRemoteTyped GetCurTeb(void)
    {
        return GetCurTeb(GetCurTebPtr());
    }
    
    //
    // Utilities.
    //

    static ULONG64 WINAPI GetNtDebuggerData(_In_ ULONG DataOffset,
                                            _In_ PCSTR Symbol,
                                            _In_ ULONG Flags);

protected:
    static ULONG64 s_KernelLoadedModuleBaseInfoCookie;
    static ULONG64 s_KernelProcessBaseInfoCookie;
    static ULONG64 s_KernelThreadBaseInfoCookie;
    static ULONG64 s_KernelProcessThreadListFieldCookie;
    static ULONG64 s_UserOsLoadedModuleBaseInfoCookie;
    static ULONG64 s_UserAltLoadedModuleBaseInfoCookie;
    static ULONG64 s_OsPebBaseInfoCookie;
    static ULONG64 s_AltPebBaseInfoCookie;
    static ULONG64 s_OsTebBaseInfoCookie;
    static ULONG64 s_AltTebBaseInfoCookie;
};

//----------------------------------------------------------------------------
//
// Number-to-string helpers for things like #define translations.
//
//----------------------------------------------------------------------------

//
// Convenience macros for filling define declarations.
//

#define EXT_DEFINE_DECL(_Def) \
    { #_Def, _Def },
#define EXT_DEFINE_END { NULL, 0 }

// In order to avoid #define replacement on the names
// these macros cannot be nested macros.
#define EXT_DEFINE_DECL2(_Def1, _Def2) \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }
#define EXT_DEFINE_DECL3(_Def1, _Def2, _Def3) \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 }
#define EXT_DEFINE_DECL4(_Def1, _Def2, _Def3, _Def4) \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 }, { #_Def4, _Def4 }
#define EXT_DEFINE_DECL5(_Def1, _Def2, _Def3, _Def4, _Def5) \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 },\
    { #_Def4, _Def4 }, { #_Def5, _Def5 }
#define EXT_DEFINE_DECL6(_Def1, _Def2, _Def3, _Def4, _Def5, _Def6) \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 },\
    { #_Def4, _Def4 }, { #_Def5, _Def5 }, { #_Def6, _Def6 }
#define EXT_DEFINE_DECL7(_Def1, _Def2, _Def3, _Def4, _Def5, _Def6, _Def7) \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 },\
    { #_Def4, _Def4 }, { #_Def5, _Def5 }, { #_Def6, _Def6 }, { #_Def7, _Def7 }

//
// Convenience macros for declaring global maps.
//

#define EXT_DEFINE_MAP_DECL(_Name, _Flags) \
ExtDefineMap g_##_Name##DefineMap(g_##_Name##Defines, _Flags)

#define EXT_DEFINE_MAP1(_Name, _Flags, _Def1) \
ExtDefine g_##_Name##Defines[] = { \
    { #_Def1, _Def1 }, EXT_DEFINE_END \
}; EXT_DEFINE_MAP_DECL(_Name, _Flags)
#define EXT_DEFINE_MAP2(_Name, _Flags, _Def1, _Def2) \
ExtDefine g_##_Name##Defines[] = { \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, EXT_DEFINE_END \
}; EXT_DEFINE_MAP_DECL(_Name, _Flags)
#define EXT_DEFINE_MAP3(_Name, _Flags, _Def1, _Def2, _Def3) \
ExtDefine g_##_Name##Defines[] = { \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 },\
    EXT_DEFINE_END \
}; EXT_DEFINE_MAP_DECL(_Name, _Flags)
#define EXT_DEFINE_MAP4(_Name, _Flags, _Def1, _Def2, _Def3, _Def4) \
ExtDefine g_##_Name##Defines[] = { \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 },\
    { #_Def4, _Def4 }, EXT_DEFINE_END \
}; EXT_DEFINE_MAP_DECL(_Name, _Flags)
#define EXT_DEFINE_MAP5(_Name, _Flags, _Def1, _Def2, _Def3, _Def4, _Def5) \
ExtDefine g_##_Name##Defines[] = { \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 },\
    { #_Def4, _Def4 }, { #_Def5, _Def5 }, EXT_DEFINE_END \
}; EXT_DEFINE_MAP_DECL(_Name, _Flags)
#define EXT_DEFINE_MAP6(_Name, _Flags, _Def1, _Def2, _Def3, _Def4, _Def5, _Def6) \
ExtDefine g_##_Name##Defines[] = { \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 },\
    { #_Def4, _Def4 }, { #_Def5, _Def5 }, { #_Def6, _Def6 },\
    EXT_DEFINE_END \
}; EXT_DEFINE_MAP_DECL(_Name, _Flags)
#define EXT_DEFINE_MAP7(_Name, _Flags, _Def1, _Def2, _Def3, _Def4, _Def5, _Def6, _Def7) \
ExtDefine g_##_Name##Defines[] = { \
    { #_Def1, _Def1 }, { #_Def2, _Def2 }, { #_Def3, _Def3 },\
    { #_Def4, _Def4 }, { #_Def5, _Def5 }, { #_Def6, _Def6 },\
    { #_Def7, _Def7 }, EXT_DEFINE_END \
}; EXT_DEFINE_MAP_DECL(_Name, _Flags)

#define EXT_DEFINE_MAP_BEGIN(_Name) \
ExtDefine g_##_Name##Defines[] = {

#define EXT_DEFINE_MAP_END(_Name, _Flags) \
    EXT_DEFINE_END \
}; EXT_DEFINE_MAP_DECL(_Name, _Flags);

struct ExtDefine
{
    PCSTR Name;
    ULONG64 Value;
};

class ExtDefineMap
{
public:
    ExtDefineMap(_In_ ExtDefine* Defines,
                 _In_ ULONG Flags)
    {
        m_Defines = Defines;
        m_Flags = Flags;
    };

    static const ULONG Bitwise         = 0x00000001;
    static const ULONG OutValue        = 0x00000002;
    static const ULONG OutValue32      = 0x00000004;
    static const ULONG OutValue64      = 0x00000008;
    static const ULONG OutValueAny     = OutValue | OutValue32 | OutValue64;
    static const ULONG OutValueAlready = 0x00000010;
    static const ULONG ValueAny        = OutValueAny | OutValueAlready;
    
    // Defines are searched in the order given for
    // defines where the full value of the define is
    // included in the argument value.  Multi-bit
    // defines should come before single-bit defines
    // so that they take priority for bitwise maps.
    ExtDefine* WINAPI Map(_In_ ULONG64 Value);
    PCSTR WINAPI MapStr(_In_ ULONG64 Value,
                        _In_opt_ PCSTR InvalidStr = NULL);

    // For a bitwise map, outputs all defines
    // that can be found in the value.
    // For non-bitwise, outputs the matching define.
    // Uses wrapped output.
    void WINAPI Out(_In_ ULONG64 Value,
                    _In_ ULONG Flags = 0,
                    _In_opt_ PCSTR InvalidStr = NULL);
    
    ExtDefine* m_Defines;
    ULONG m_Flags;
};

//----------------------------------------------------------------------------
//
// Output capture helper class.
//
//----------------------------------------------------------------------------

template<typename _CharType, typename _BaseClass>
class ExtCaptureOutput : public _BaseClass
{
public:
    ExtCaptureOutput(void)
    {
        m_Started = false;
        m_Text = NULL;
        m_CharTypeSize = (ULONG) sizeof(_CharType);
        Delete();
    }
    ~ExtCaptureOutput(void)
    {
        Delete();
    }
    
    // IUnknown.
    STDMETHOD(QueryInterface)(
        THIS_
        _In_ REFIID InterfaceId,
        _Out_ PVOID* Interface
        )
    {
        *Interface = NULL;

        if (IsEqualIID(InterfaceId, __uuidof(IUnknown)) ||
            IsEqualIID(InterfaceId, __uuidof(_BaseClass)))
        {
            *Interface = (_BaseClass *)this;
            AddRef();
            return S_OK;
        }
        else
        {
            return E_NOINTERFACE;
        }
    }
    STDMETHOD_(ULONG, AddRef)(
        THIS
        )
    {
        // This class is designed to be non-dynamic so
        // there's no true refcount.
        return 1;
    }
    STDMETHOD_(ULONG, Release)(
        THIS
        )
    {
        // This class is designed to be non-dynamic so
        // there's no true refcount.
        return 0;
    }
    
    // IDebugOutputCallbacks*.
    STDMETHOD(Output)(
        THIS_
        _In_ ULONG Mask,
        _In_ const _CharType* Text
        )
    {
        ULONG Chars;
        ULONG CharTypeSize = (ULONG) sizeof(_CharType);

        UNREFERENCED_PARAMETER(Mask);
        
        if (CharTypeSize == sizeof(char))
        {
            Chars = (ULONG) strlen((PSTR)Text) + 1;
        }
        else
        {
            Chars = (ULONG) wcslen((PWSTR)Text) + 1;
        }
        if (Chars < 2)
        {
            return S_OK;
        }

        if (0xffffffff / CharTypeSize - m_UsedChars < Chars)
        {
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        }

        if (m_UsedChars + Chars > m_AllocChars)
        {
            ULONG NewBytes;

            // Overallocate when growing to prevent
            // continuous allocation.
            if (0xffffffff / CharTypeSize - m_UsedChars - Chars > 256)
            {
                NewBytes = (m_UsedChars + Chars + 256) * CharTypeSize;
            }
            else
            {
                NewBytes = (m_UsedChars + Chars) * CharTypeSize;
            }
            PVOID NewMem = realloc(m_Text, NewBytes);
            if (!NewMem)
            {
                return E_OUTOFMEMORY;
            }

            m_Text = (_CharType*)NewMem;
            m_AllocChars = NewBytes / CharTypeSize;
        }

        memcpy(m_Text + m_UsedChars, Text,
               Chars * CharTypeSize);
        // Advance up to but not past the terminator
        // so that it gets overwritten by the next text.
        m_UsedChars += Chars - 1;
        return S_OK;
    }

    void Start(void)
    {
        HRESULT Status;

        if (m_CharTypeSize == sizeof(char))
        {
            if ((Status = g_Ext->m_Client->
                 GetOutputCallbacks((IDebugOutputCallbacks**)
                                    &m_OldOutCb)) != S_OK)
            {
                g_Ext->ThrowStatus(Status,
                                   "Unable to get previous output callback");
            }
            if ((Status = g_Ext->m_Client->
                 SetOutputCallbacks((IDebugOutputCallbacks*)
                                    this)) != S_OK)
            {
                g_Ext->ThrowStatus(Status,
                                   "Unable to set capture output callback");
            }
        }
        else
        {
            if ((Status = g_Ext->m_Client5->
                 GetOutputCallbacksWide((IDebugOutputCallbacksWide**)
                                        &m_OldOutCb)) != S_OK)
            {
                g_Ext->ThrowStatus(Status,
                                   "Unable to get previous output callback");
            }
            if ((Status = g_Ext->m_Client5->
                 SetOutputCallbacksWide((IDebugOutputCallbacksWide*)
                                        this)) != S_OK)
            {
                g_Ext->ThrowStatus(Status,
                                   "Unable to set capture output callback");
            }
        }
            
        m_UsedChars = 0;
        m_Started = true;
    }
    
    void Stop(void)
    {
        HRESULT Status;
        
        m_Started = false;

        if (m_CharTypeSize == sizeof(char))
        {
            if ((Status = g_Ext->m_Client->
                 SetOutputCallbacks((IDebugOutputCallbacks*)
                                    m_OldOutCb)) != S_OK)
            {
                g_Ext->ThrowStatus(Status,
                                   "Unable to restore output callback");
            }
        }
        else
        {
            if ((Status = g_Ext->m_Client5->
                 SetOutputCallbacksWide((IDebugOutputCallbacksWide*)
                                        m_OldOutCb)) != S_OK)
            {
                g_Ext->ThrowStatus(Status,
                                   "Unable to restore output callback");
            }
        }

        m_OldOutCb = NULL;
    }

    void Delete(void)
    {
        if (m_Started)
        {
            Stop();
        }

        free(m_Text);
        m_Text = NULL;
        m_AllocChars = 0;
        m_UsedChars = 0;
    }

    void Execute(_In_ PCSTR Command)
    {
        Start();
        
        // Hide all output from the execution
        // and don't save the command.
        g_Ext->m_Control->Execute(DEBUG_OUTCTL_THIS_CLIENT |
                                  DEBUG_OUTCTL_OVERRIDE_MASK |
                                  DEBUG_OUTCTL_NOT_LOGGED,
                                  Command,
                                  DEBUG_EXECUTE_NOT_LOGGED |
                                  DEBUG_EXECUTE_NO_REPEAT);

        Stop();
    }
    
    const _CharType* GetTextNonNull(void)
    {
        if (m_CharTypeSize == sizeof(char))
        {
            return (_CharType*)(m_Text ? (PCSTR)m_Text : "");
        }
        else
        {
            return (_CharType*)(m_Text ? (PCWSTR)m_Text : L"");
        }
    }
    
    bool m_Started;
    ULONG m_AllocChars;
    ULONG m_UsedChars;
    ULONG m_CharTypeSize;
    _CharType* m_Text;

    _BaseClass* m_OldOutCb;
};
    
typedef ExtCaptureOutput<char, IDebugOutputCallbacks> ExtCaptureOutputA;
typedef ExtCaptureOutput<WCHAR, IDebugOutputCallbacksWide> ExtCaptureOutputW;

#if _MSC_VER >= 800
#pragma warning(default:4121)
#endif
      
#include <poppack.h>

#endif // #ifndef __ENGEXTCPP_HPP__
