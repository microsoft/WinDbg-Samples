//----------------------------------------------------------------------------
//
// BasicExdiBreakpoint.h
//
// A sample implementation of the IeXdiCodeBreakpoint interface used to represent
// breakpoints maintained by EXDI servers.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once
#include "ExdiKdSample.h"

interface DECLSPEC_UUID("8EC0B42F-9B46-4674-AC60-64105713BB35") IBasicExdiBreakpoint : public IUnknown
{
public:
    virtual unsigned GetBreakpointNumber() = 0;
};

class BasicExdiBreakpoint : public CComObjectRootEx<CComSingleThreadModel>,
                            public IeXdiCodeBreakpoint3,
                            public IBasicExdiBreakpoint
{
public:
    BasicExdiBreakpoint()
        : m_address(0)
        , m_breakpointNumber(0)
    {
    }

BEGIN_COM_MAP(BasicExdiBreakpoint)
	COM_INTERFACE_ENTRY(IeXdiCodeBreakpoint3)
	COM_INTERFACE_ENTRY(IBasicExdiBreakpoint)
END_COM_MAP()

    virtual unsigned GetBreakpointNumber()
    {
        return m_breakpointNumber;
    }

    void Initialize(ULONGLONG address, unsigned breakpointNumber)
    {
        m_address = address;
        m_breakpointNumber = breakpointNumber;
    }

    virtual HRESULT STDMETHODCALLTYPE GetAttributes( 
        /* [out] */ PADDRESS_TYPE pAddress,
        /* [out] */ PCBP_KIND pcbpk,
        /* [out] */ PMEM_TYPE pmt,
        /* [out] */ DWORD *pdwExecMode,
        /* [out] */ DWORD *pdwTotalBypassCount,
        /* [out] */ DWORD *pdwBypassedOccurences,
        /* [out] */ BOOL *pfEnabled)
    {
        if (pAddress == nullptr || pcbpk == nullptr || pmt == nullptr || pdwExecMode == nullptr ||
            pdwTotalBypassCount == nullptr || pdwBypassedOccurences == nullptr || pfEnabled == nullptr)
        {
            return E_POINTER;
        }

        *pAddress = m_address;
        *pcbpk = cbptSW;
        *pmt = mtVirtual;

        *pdwExecMode = 0;
        *pdwTotalBypassCount = 0;
        *pdwBypassedOccurences = 0;
        *pfEnabled = TRUE;
        return S_OK;
    }
        
    virtual HRESULT STDMETHODCALLTYPE SetState( 
        /* [in] */ BOOL fEnabled,
        /* [in] */ BOOL fResetBypassedOccurences)
    {
		UNREFERENCED_PARAMETER(fResetBypassedOccurences);

        if (!fEnabled)
        {
            return E_NOTIMPL;
        }
        
        return S_OK;
    }

private:
    ULONGLONG m_address;
    unsigned m_breakpointNumber;
};