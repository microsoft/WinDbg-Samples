//**************************************************************************
//
// Utility.cpp
//
// General utilities for the Python provider.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "PyProvider.h"

namespace Debugger::DataModel::ScriptProvider::Python
{

ScriptSwitcher::~ScriptSwitcher()
{
    if (m_pMarshaler != nullptr)
    {
        m_pMarshaler->SetActiveScriptState(m_spPriorScriptState.Get(), Marshal::PythonMarshaler::ScriptEntryType::ScriptExit);
    }

    if (m_gilTaken)
    {
        PyGILState_Release(m_gstate);
    }
}

ScriptSwitcher::ScriptSwitcher(_In_ Marshal::PythonMarshaler *pMarshaler, 
                               _In_ PythonScriptState *pScriptState) :
    m_pMarshaler(pMarshaler)
{
    m_spPriorScriptState = pMarshaler->GetActiveScriptState();
    if (m_spPriorScriptState == nullptr)
    {
        //
        // The first entry into any script context requires that we explicitly hold the GIL.
        //
        m_gilTaken = true;
        m_gstate = PyGILState_Ensure();
    }
    m_pMarshaler->SetActiveScriptState(pScriptState, Marshal::PythonMarshaler::ScriptEntryType::ScriptEntry);
}

BSTR SysAllocStringFromUTF8(_In_z_ const char *pUTF8)
{
    std::unique_ptr<wchar_t[]> spExtended;
    wchar_t buf[256];
    wchar_t *pBuf = buf;
    int bufSz = 256;

    int sz = MultiByteToWideChar(CP_UTF8, 0, pUTF8, -1, nullptr, 0);
    if (sz == 0)
    {
        return nullptr;
    }

    if (sz > ARRAYSIZE(buf))
    {
        spExtended.reset(new(std::nothrow) wchar_t[sz]);
        if (spExtended.get() == nullptr)
        {
            return nullptr;
        }
        pBuf = spExtended.get();
        bufSz = sz;
    }

    int rsz = MultiByteToWideChar(CP_UTF8, 0, pUTF8, -1, pBuf, bufSz);
    if (sz != rsz)
    {
        return nullptr;
    }

    return SysAllocString(pBuf);
}

HRESULT GetUTF16(_In_ PCSTR pUTF8, _Out_ std::wstring* pUTF16)
{
    auto fn = [&]()
    {
        int sz = MultiByteToWideChar(CP_UTF8, 0, pUTF8, -1, nullptr, 0);
        if (sz == 0)
        {
            return E_FAIL;
        }

        pUTF16->resize(sz - 1);
        int rsz = MultiByteToWideChar(CP_UTF8, 0, pUTF8, -1, const_cast<wchar_t *>(pUTF16->data()), sz);
        if (rsz != sz)
        {
            return E_FAIL;
        }

        return S_OK;
    };

    return ConvertException(fn);
}

HRESULT GetUTF16N(_In_ PCSTR pUTF8, _In_ size_t n, _Out_ std::wstring* pUTF16)
{
    auto fn = [&]()
    {
        int sz = MultiByteToWideChar(CP_UTF8, 0, pUTF8, static_cast<int>(n), nullptr, 0);
        if (sz == 0)
        {
            return E_FAIL;
        }

        pUTF16->resize(sz);
        int rsz = MultiByteToWideChar(CP_UTF8, 0, pUTF8, static_cast<int>(n), const_cast<wchar_t *>(pUTF16->data()), sz);
        if (rsz != sz)
        {
            return E_FAIL;
        }

        return S_OK;
    };

    return ConvertException(fn);
}

HRESULT GetUTF8(_In_ PCWSTR pUTF16, _Out_ std::string* pUTF8)
{
    auto fn = [&]()
    {
        int sz = WideCharToMultiByte(CP_UTF8, 0, pUTF16, -1, nullptr, 0, nullptr, nullptr);
        if (sz == 0)
        {
            return E_FAIL;
        }

        pUTF8->resize(sz - 1);
        int rsz = WideCharToMultiByte(CP_UTF8, 0, pUTF16, -1, const_cast<char *>(pUTF8->data()), sz, nullptr, nullptr);
        if (rsz != sz)
        {
            return E_FAIL;
        }

        return S_OK;
    };

    return ConvertException(fn);
}


} // Debugger::DataModel::ScriptProvider::Python
