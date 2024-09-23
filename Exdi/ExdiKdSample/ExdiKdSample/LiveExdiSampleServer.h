//----------------------------------------------------------------------------
//
// LiveExdiSampleServer.h
//
// A sample EXDI server class with support for setting breakpoints, stepping 
// and continuing execution.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once
#include "resource.h"       // main symbols

#include "ExdiKdSample.h"
#include "StaticExdiSampleServer.h"
#include "InterfaceMarshalHelper.h"

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

using namespace ATL;


// CLiveExdiSampleServer

class ATL_NO_VTABLE CLiveExdiSampleServer :
	public CComCoClass<CLiveExdiSampleServer, &CLSID_LiveExdiSampleServer>,
	public CStaticExdiSampleServer,
    public IAsynchronousCommandNotificationReceiver
{
public:
	CLiveExdiSampleServer()
        : m_pSelfReferenceForNotificationThread(nullptr)
        , m_notificationThread(nullptr)
        , m_notificationSemaphore(nullptr)
        , m_terminating(false)
        , m_lastResumingCommandWasStep(false)
        , m_targetIsRunning(false)
        , m_timerId(0)
	{
	}

    ~CLiveExdiSampleServer()
    {
        assert(m_terminating);
    }

DECLARE_REGISTRY_RESOURCEID(IDR_LIVEEXDISAMPLESERVER)
DECLARE_NOT_AGGREGATABLE(CLiveExdiSampleServer)

BEGIN_COM_MAP(CLiveExdiSampleServer)
	COM_INTERFACE_ENTRY(IeXdiServer3)
	COM_INTERFACE_ENTRY(IeXdiARM4Context3)
	COM_INTERFACE_ENTRY(IeXdiX86_64Context3)
    COM_INTERFACE_ENTRY(IeXdiArmV8Arch64Context3)
    COM_INTERFACE_ENTRY(IAsynchronousCommandNotificationReceiver)
END_COM_MAP()

	virtual HRESULT FinalConstruct() override;
	virtual void FinalRelease() override;

    virtual HRESULT STDMETHODCALLTYPE DoSingleStep(DWORD dwProcessorNumber) override;
    virtual HRESULT STDMETHODCALLTYPE Run(void) override;
    virtual HRESULT STDMETHODCALLTYPE Halt(void) override;

    virtual HRESULT STDMETHODCALLTYPE StartNotifyingRunChg( 
        /* [in] */ IeXdiClientNotifyRunChg3 *pieXdiClientNotifyRunChg,
        /* [out] */ DWORD *pdwConnectionCookie)  override;
        
    virtual HRESULT STDMETHODCALLTYPE StopNotifyingRunChg( 
        /* [in] */ DWORD dwConnectionCookie) override;

    virtual HRESULT STDMETHODCALLTYPE SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARM4 *pContext) override;
    virtual HRESULT STDMETHODCALLTYPE SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_X86_64 *pContext) override;

    virtual HRESULT STDMETHODCALLTYPE GetRunStatus( 
        /* [out] */ PRUN_STATUS_TYPE persCurrent,
        /* [out] */ PHALT_REASON_TYPE pehrCurrent,
        /* [out] */ ADDRESS_TYPE *pCurrentExecAddress,
        /* [out] */ DWORD *pdwExceptionCode,
        /* [out] */ DWORD *pdwProcessorNumberOfLastEvent) override;

    virtual HRESULT STDMETHODCALLTYPE AddCodeBreakpoint( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ CBP_KIND cbpk,
        /* [in] */ MEM_TYPE mt,
        /* [in] */ DWORD dwExecMode,
        /* [in] */ DWORD dwTotalBypassCount,
        /* [out] */ IeXdiCodeBreakpoint3 **ppieXdiCodeBreakpoint) override;
        
    virtual HRESULT STDMETHODCALLTYPE DelCodeBreakpoint( 
        /* [in] */ IeXdiCodeBreakpoint3 *pieXdiCodeBreakpoint) override;

    virtual HRESULT STDMETHODCALLTYPE SetKeepaliveInterface( 
        /* [in] */ IeXdiKeepaliveInterface3 *pKeepalive);

    virtual HRESULT STDMETHODCALLTYPE OnAsynchronousCommandCompleted(void);
    virtual HRESULT STDMETHODCALLTYPE PerformKeepaliveChecks(void);

    virtual HRESULT STDMETHODCALLTYPE SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARMV8ARCH64 * pContext) override;

private:
    CComPtr<IeXdiClientNotifyRunChg3> m_pRunNotificationListener;

    InterfaceMarshalHelper<IAsynchronousCommandNotificationReceiver> *m_pSelfReferenceForNotificationThread;
    HANDLE m_notificationThread;
    HANDLE m_notificationSemaphore;
    bool m_terminating;
    bool m_lastResumingCommandWasStep;
    bool m_targetIsRunning;
    UINT_PTR m_timerId;
    CComPtr<IeXdiKeepaliveInterface3> m_pKeepaliveInterface;

    static DWORD CALLBACK NotificationThreadBody(LPVOID p);

    static VOID CALLBACK SampleTimerCallback(_In_  HWND hwnd, _In_  UINT uMsg, _In_  UINT_PTR idEvent, _In_  DWORD dwTime);
};

OBJECT_ENTRY_AUTO(__uuidof(LiveExdiSampleServer), CLiveExdiSampleServer)
