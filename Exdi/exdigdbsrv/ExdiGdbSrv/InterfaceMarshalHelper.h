//----------------------------------------------------------------------------
//
// InterfaceMarshalHelper.h
//
// A simple wrapper class that helps marshalling COM interfaces across apartments.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once
#include <exception>
#include <atlcomcli.h>
#include <assert.h>

template <class TInterface> class InterfaceMarshalHelper
{
public:
    InterfaceMarshalHelper(_In_ TInterface *pInterface, DWORD marshalFlags) //flags such as MSHLFLAGS_TABLESTRONG
    {
        m_creatorThreadId = GetCurrentThreadId();

	    HRESULT result = CreateStreamOnHGlobal(NULL, TRUE, &m_pStream);
        if (FAILED(result))
        {
            throw std::exception("Failed to create a global stream");
        }

        result = CoMarshalInterface(m_pStream, __uuidof(TInterface), pInterface, MSHCTX_INPROC, NULL, marshalFlags);
        if (FAILED(result))
        {
            throw std::exception("Failed to marshal the interface");
        }

        InitializeCriticalSection(&m_criticalSection);
    }

    ~InterfaceMarshalHelper()
    {
        assert(m_creatorThreadId == GetCurrentThreadId());
        if (m_pStream != nullptr)
        {
            m_pStream->Seek(LARGE_INTEGER(), SEEK_SET, NULL);
            CoReleaseMarshalData(m_pStream);

            m_pStream->Release();
            m_pStream = nullptr;
        }
        DeleteCriticalSection(&m_criticalSection);
    }

    TInterface *TryUnmarshalInterfaceForCurrentThread()
    {
        EnterCriticalSection(&m_criticalSection);
        m_pStream->Seek(LARGE_INTEGER(), SEEK_SET, NULL);
	    TInterface *pInterface = nullptr;
	    HRESULT result = CoUnmarshalInterface(m_pStream, __uuidof(TInterface), reinterpret_cast<void **>(&pInterface));
        LeaveCriticalSection(&m_criticalSection);

        if (FAILED(result))
        {
            pInterface = nullptr;
        }

        return pInterface;
    }

private:
    IStream *m_pStream;
    CRITICAL_SECTION m_criticalSection;
    DWORD m_creatorThreadId;
};
