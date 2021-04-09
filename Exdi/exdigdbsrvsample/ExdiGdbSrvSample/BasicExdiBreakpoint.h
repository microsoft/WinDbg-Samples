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
#include "ExdiGdbSrvSample.h"

interface DECLSPEC_UUID("8EC0B42F-9B46-4674-AC60-64105713BB35") IBasicExdiBreakpoint : public IUnknown
{
public:
    virtual unsigned GetBreakpointNumber() = 0;
    virtual ADDRESS_TYPE GetBreakPointAddress() = 0;
};

class BasicExdiBreakpoint : public CComObjectRootEx<CComSingleThreadModel>,
                            public IeXdiCodeBreakpoint3,
                            public IBasicExdiBreakpoint
{
public:
    BasicExdiBreakpoint() : m_address(0),
                            m_breakpointNumber(0)
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

    virtual ADDRESS_TYPE GetBreakPointAddress()
    {
        return m_address;
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

interface DECLSPEC_UUID("268ff389-6a62-48e7-b23b-168228ad89e7") IBasicExdiDataBreakpoint : public IUnknown
{
public:
    virtual unsigned GetBreakpointNumber() = 0;
    virtual ADDRESS_TYPE GetBreakPointAddress() = 0;
    virtual DATA_ACCESS_TYPE GetBreakPointAccessType() = 0;
    virtual BYTE GetBreakPointAccessWidth() = 0;
};

class BasicExdiDataBreakpoint : public CComObjectRootEx<CComSingleThreadModel>,
                                public IeXdiDataBreakpoint3,
                                public IBasicExdiDataBreakpoint
{
public:
    BasicExdiDataBreakpoint() : m_address(0),
                                m_breakpointNumber(0),
                                m_breakpointAccessWidth(0),
                                m_accessType(daWrite)
    {
    }

BEGIN_COM_MAP(BasicExdiDataBreakpoint)
	COM_INTERFACE_ENTRY(IeXdiDataBreakpoint3)
	COM_INTERFACE_ENTRY(IBasicExdiDataBreakpoint)
END_COM_MAP()

    virtual unsigned GetBreakpointNumber()
    {
        return m_breakpointNumber;
    }

    virtual ADDRESS_TYPE GetBreakPointAddress()
    {
        return m_address;
    }

    virtual DATA_ACCESS_TYPE GetBreakPointAccessType()
    {
        return m_accessType;
    }

    virtual BYTE GetBreakPointAccessWidth()
    {
        return m_breakpointAccessWidth;
    }

    void Initialize(ULONGLONG address, unsigned breakpointNumber, DATA_ACCESS_TYPE accessType, BYTE accessWidth)
    {
        m_address = address;
        m_breakpointNumber = breakpointNumber;
        m_accessType = accessType;
        m_breakpointAccessWidth = accessWidth;
    }

    virtual HRESULT STDMETHODCALLTYPE GetAttributes( 
        /* [out] */ PADDRESS_TYPE pAddress,
        /* [out] */ PADDRESS_TYPE pAddressMask,
        /* [out] */ DWORD *pdwData,
        /* [out] */ DWORD *pdwDataMask,
        /* [out] */ BYTE *pbAccessWidth,
        /* [out] */ PMEM_TYPE pmt,
        /* [out] */ BYTE *pbAddressSpace,
        /* [out] */ PDATA_ACCESS_TYPE pda,
        /* [out] */ DWORD *pdwTotalBypassCount,
        /* [out] */ DWORD *pdwBypassedOccurences,
        /* [out] */ BOOL *pfEnabled)
    {
        if (pAddress == nullptr || pAddressMask == nullptr || pdwData == nullptr || pdwDataMask == nullptr ||
            pbAccessWidth == nullptr || pmt == nullptr || pbAddressSpace == nullptr || pda != nullptr || 
            pdwTotalBypassCount != nullptr || pdwBypassedOccurences != nullptr || pfEnabled != nullptr)
        {
            return E_POINTER;
        }

        *pAddress = m_address;
        *pAddressMask = -1;
        *pdwData = 0;
        *pdwDataMask = 0;
        *pbAccessWidth = static_cast<BYTE>(m_breakpointAccessWidth); 
        *pmt = mtVirtual;
        *pbAddressSpace = 0;
        *pda = m_accessType;
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
    BYTE m_breakpointAccessWidth;
    DATA_ACCESS_TYPE m_accessType;
};