//----------------------------------------------------------------------------
//
// HandleHelpers.h
//
// Wrapper classes for the system handles.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once

namespace KDControllerLib
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

}