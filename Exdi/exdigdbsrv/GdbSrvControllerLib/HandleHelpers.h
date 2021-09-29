//----------------------------------------------------------------------------
//
// HandleHelpers.h
//
// Wrapper classes for the system handles.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#pragma once

#define RemoveEntryList(Entry) {\
    PLIST_ENTRY _EX_Blink;\
    PLIST_ENTRY _EX_Flink;\
    _EX_Flink = (Entry)->Flink;\
    _EX_Blink = (Entry)->Blink;\
    _EX_Blink->Flink = _EX_Flink;\
    _EX_Flink->Blink = _EX_Blink;\
    }

#define InitializeListHead(ListHead) (\
    (ListHead)->Flink = (ListHead)->Blink = (ListHead))

#define InsertTailList(ListHead,Entry) {\
    PLIST_ENTRY _EX_Blink;\
    PLIST_ENTRY _EX_ListHead;\
    _EX_ListHead = (ListHead);\
    _EX_Blink = _EX_ListHead->Blink;\
    (Entry)->Flink = _EX_ListHead;\
    (Entry)->Blink = _EX_Blink;\
    _EX_Blink->Flink = (Entry);\
    _EX_ListHead->Blink = (Entry);\
    }


namespace GdbSrvControllerLib
{
    class HandleWrapper
    {
    public:
        HandleWrapper() 
            : m_handle(INVALID_HANDLE_VALUE)
        {
        }

        HandleWrapper(_In_ HANDLE handle) 
            : m_handle(handle)
        {
        }

        ~HandleWrapper() 
        {
            Close();
        }

        HANDLE Detach() 
        {
            HANDLE handle = m_handle;
            m_handle = INVALID_HANDLE_VALUE;
            return handle;
        }

        HANDLE Get() const 
        {
            return m_handle;
        }

        bool IsValid() const 
        {
            return m_handle != INVALID_HANDLE_VALUE;
        }

        HANDLE *operator&() 
        {
            //Used to pass to a _Out_ PHANDLE argument. Similar to CComPtr in ATL.
            //If the handle is already valid, such use case will lead to a handle leak.
            assert(m_handle == INVALID_HANDLE_VALUE);
            return &m_handle;
        }

        void Close()
        {
            if (m_handle == INVALID_HANDLE_VALUE)
            {
                return;
            }

            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }

        void Attach(_In_ HANDLE handle)
        {
            assert(handle != nullptr);
            Close();
            m_handle = handle;
        }

        static void CloseAndInvalidate(_Inout_ HANDLE *pHandle)
        {
            assert(pHandle != nullptr);
            CloseHandle(*pHandle);
            *pHandle = INVALID_HANDLE_VALUE;
        }

    private:
        HANDLE m_handle;

        HandleWrapper(_In_ const HandleWrapper &anotherHandleWrapper);
        void operator=(_In_ const HandleWrapper &anotherHandleWrapper);
    };

    class ValidHandleWrapper : public HandleWrapper
    {
    public:
        ValidHandleWrapper(_In_ HANDLE handle) 
            : HandleWrapper(handle)
        {
            assert(handle != INVALID_HANDLE_VALUE);
        }
    };

    class scoped_lock
    {
        public:
            _Acquires_lock_(cs)
            scoped_lock(_In_ CRITICAL_SECTION& cs) : pCS(&cs)
            {
                EnterCriticalSection(this->pCS);
            }

            _Releases_lock_(*(this->pCS))
            ~scoped_lock()
            {
                LeaveCriticalSection(this->pCS);
            }

        private:
            CRITICAL_SECTION * pCS;
    };

}