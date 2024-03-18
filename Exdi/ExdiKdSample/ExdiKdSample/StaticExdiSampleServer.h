//----------------------------------------------------------------------------
//
// StaticExdiSampleServer.h
//
// A sample EXDI server class demonstrating basic functionality.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once
#include "resource.h"       // main symbols

#include "ExdiKdSample.h"


#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

using namespace ATL;

namespace KDControllerLib
{
    class AsynchronousKDController;
}

// CStaticExdiSampleServer

class ATL_NO_VTABLE CStaticExdiSampleServer :
	public CComObjectRootEx<CComSingleThreadModel>,
    public IeXdiServer3,
    public IeXdiARM4Context3,
    public IeXdiX86_64Context3,
    public IeXdiArmV8Arch64Context3
{
public:
	CStaticExdiSampleServer()
        : m_pKDController(nullptr)
        , m_detectedProcessorFamily(PROCESSOR_FAMILY_UNK)
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_STATICEXDISAMPLESERVER)

BEGIN_COM_MAP(CStaticExdiSampleServer)
	COM_INTERFACE_ENTRY(IeXdiServer3)
	COM_INTERFACE_ENTRY(IeXdiARM4Context3)
	COM_INTERFACE_ENTRY(IeXdiX86_64Context3)
    COM_INTERFACE_ENTRY(IeXdiArmV8Arch64Context3)
END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	virtual HRESULT FinalConstruct();
	virtual void FinalRelease();

#pragma region IeXdiServer implementation
    virtual HRESULT STDMETHODCALLTYPE GetTargetInfo( 
        /* [out] */ PGLOBAL_TARGET_INFO_STRUCT pgti);
        
    virtual HRESULT STDMETHODCALLTYPE GetRunStatus( 
        /* [out] */ PRUN_STATUS_TYPE persCurrent,
        /* [out] */ PHALT_REASON_TYPE pehrCurrent,
        /* [out] */ ADDRESS_TYPE *pCurrentExecAddress,
        /* [out] */ DWORD *pdwExceptionCode,
        /* [out] */ DWORD *pdwProcessorNumberOfLastEvent);
        
    virtual HRESULT STDMETHODCALLTYPE Run( void);
        
    virtual HRESULT STDMETHODCALLTYPE Halt( void);
        
    virtual HRESULT STDMETHODCALLTYPE DoSingleStep(DWORD dwProcessorNumber);
        
    virtual HRESULT STDMETHODCALLTYPE Reboot( void);
        
    virtual HRESULT STDMETHODCALLTYPE GetNbCodeBpAvail( 
        /* [out] */ DWORD *pdwNbHwCodeBpAvail,
        /* [out] */ DWORD *pdwNbSwCodeBpAvail);
        
    virtual HRESULT STDMETHODCALLTYPE GetNbDataBpAvail( 
        /* [out] */ DWORD *pdwNbDataBpAvail);
        
    virtual HRESULT STDMETHODCALLTYPE AddCodeBreakpoint( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ CBP_KIND cbpk,
        /* [in] */ MEM_TYPE mt,
        /* [in] */ DWORD dwExecMode,
        /* [in] */ DWORD dwTotalBypassCount,
        /* [out] */ IeXdiCodeBreakpoint3 **ppieXdiCodeBreakpoint);
        
    virtual HRESULT STDMETHODCALLTYPE DelCodeBreakpoint( 
        /* [in] */ IeXdiCodeBreakpoint3 *pieXdiCodeBreakpoint);
        
    virtual HRESULT STDMETHODCALLTYPE AddDataBreakpoint( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ ADDRESS_TYPE AddressMask,
        /* [in] */ DWORD dwData,
        /* [in] */ DWORD dwDataMask,
        /* [in] */ BYTE bAccessWidth,
        /* [in] */ MEM_TYPE mt,
        /* [in] */ BYTE bAddressSpace,
        /* [in] */ DATA_ACCESS_TYPE da,
        /* [in] */ DWORD dwTotalBypassCount,
        /* [out] */ IeXdiDataBreakpoint3 **ppieXdiDataBreakpoint);
        
    virtual HRESULT STDMETHODCALLTYPE DelDataBreakpoint( 
        /* [in] */ IeXdiDataBreakpoint3 *pieXdiDataBreakpoint);
        
    virtual HRESULT STDMETHODCALLTYPE StartNotifyingRunChg( 
        /* [in] */ IeXdiClientNotifyRunChg3 *pieXdiClientNotifyRunChg,
        /* [out] */ DWORD *pdwConnectionCookie);
        
    virtual HRESULT STDMETHODCALLTYPE StopNotifyingRunChg( 
        /* [in] */ DWORD dwConnectionCookie);
        
    virtual HRESULT STDMETHODCALLTYPE ReadVirtualMemory( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ DWORD dwBytesToRead,
        SAFEARRAY * *pbReadBuffer);
        
    virtual HRESULT STDMETHODCALLTYPE WriteVirtualMemory( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ SAFEARRAY * pBuffer,
        /* [out] */ DWORD *pdwBytesWritten);
        
    virtual HRESULT STDMETHODCALLTYPE ReadPhysicalMemoryOrPeriphIO( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ ADDRESS_SPACE_TYPE AddressSpace,
        /* [in] */ DWORD dwBytesToRead,
        /* [out] */ SAFEARRAY * *pReadBuffer);
        
    virtual HRESULT STDMETHODCALLTYPE WritePhysicalMemoryOrPeriphIO( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ ADDRESS_SPACE_TYPE AddressSpace,
        /* [in] */ SAFEARRAY * pBuffer,
        /* [out] */ DWORD *pdwBytesWritten);
        
    virtual HRESULT STDMETHODCALLTYPE Ioctl( 
        /* [in] */ SAFEARRAY * pInputBuffer,
        /* [in] */ DWORD dwBuffOutSize,
        /* [out] */ SAFEARRAY * *pOutputBuffer);

    virtual HRESULT STDMETHODCALLTYPE GetNumberOfProcessors( 
        /* [out] */ DWORD *pdwNumberOfProcessors);
        
    virtual HRESULT STDMETHODCALLTYPE GetLastHitBreakpoint( 
        /* [out] */ DBGENG_EXDI3_GET_BREAKPOINT_HIT_OUT *pBreakpointInformation);
        
    virtual HRESULT STDMETHODCALLTYPE GetKPCRForProcessor( 
        /* [in] */ DWORD dwProcessorNumber,
        /* [out] */ ULONG64 *pKPCRPointer);
        
    virtual HRESULT STDMETHODCALLTYPE ReadKdVersionBlock( 
        /* [in] */ DWORD dwBufferSize,
        /* [out] */ SAFEARRAY * *pKdVersionBlockBuffer);

    virtual HRESULT STDMETHODCALLTYPE SetKeepaliveInterface( 
        /* [in] */ IeXdiKeepaliveInterface3 *pKeepalive);

    virtual HRESULT STDMETHODCALLTYPE ReadMSR( 
        /* [in] */ DWORD dwProcessorNumber,
        /* [in] */ DWORD dwRegisterIndex,
        /* [out] */ ULONG64 *pValue);
        
    virtual HRESULT STDMETHODCALLTYPE WriteMSR( 
        /* [in] */ DWORD dwProcessorNumber,
        /* [in] */ DWORD dwRegisterIndex,
        /* [in] */ ULONG64 value);
#pragma endregion

#pragma region IeXdiARMContext
    virtual HRESULT STDMETHODCALLTYPE GetContext( 
        /* [in] */ DWORD dwProcessorNumber,
        /* [out][in] */ PCONTEXT_ARM4 pContext);
        
    virtual HRESULT STDMETHODCALLTYPE SetContext( 
        /* [in] */ DWORD dwProcessorNumber,
        /* [in] */ CONTEXT_ARM4 Context);

#pragma endregion

#pragma region IeXdiARMContext
    virtual HRESULT STDMETHODCALLTYPE GetContext( 
        /* [in] */ DWORD dwProcessorNumber,
        /* [out][in] */ PCONTEXT_X86_64 pContext);
        
    virtual HRESULT STDMETHODCALLTYPE SetContext( 
        /* [in] */ DWORD dwProcessorNumber,
        /* [in] */ CONTEXT_X86_64 Context);
#pragma endregion


	//Convenience wrappers for EXDI IOCTLs. They may end up being moved to new EXDI interfaces.
	
    virtual HRESULT STDMETHODCALLTYPE GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_ARM4 pContext);
    virtual HRESULT STDMETHODCALLTYPE SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARM4 *pContext);

    virtual HRESULT STDMETHODCALLTYPE GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_X86_64 pContext);
    virtual HRESULT STDMETHODCALLTYPE SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_X86_64 *pContext);

    virtual HRESULT STDMETHODCALLTYPE GetContext(_In_ DWORD processorNumber, _Inout_ PCONTEXT_ARMV8ARCH64 pContext);
    virtual HRESULT STDMETHODCALLTYPE SetContext(_In_ DWORD processorNumber, _In_ CONTEXT_ARMV8ARCH64 context);

    virtual HRESULT STDMETHODCALLTYPE GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_ARMV8ARCH64 pContext);
    virtual HRESULT STDMETHODCALLTYPE SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARMV8ARCH64 * pContext);

    protected:
        KDControllerLib::AsynchronousKDController *GetKDController()
        {
            return m_pKDController;
        }

        ADDRESS_TYPE GetCurrentExecutionAddress(_Out_ DWORD *pProcessorNumberOfLastEvent);

    private:
        KDControllerLib::AsynchronousKDController *m_pKDController;
        DWORD m_detectedProcessorFamily;

    static VOID CALLBACK KeepaliveTimerCallback(_In_  HWND hwnd, _In_  UINT uMsg, _In_  UINT_PTR idEvent, _In_  DWORD dwTime);
};

class StaticExdiSampleServerImpl : 
    public CStaticExdiSampleServer, 
    public CComCoClass<CStaticExdiSampleServer, &CLSID_StaticExdiSampleServer>
{
    DECLARE_NOT_AGGREGATABLE(StaticExdiSampleServerImpl)
};

OBJECT_ENTRY_AUTO(__uuidof(StaticExdiSampleServer), StaticExdiSampleServerImpl)
