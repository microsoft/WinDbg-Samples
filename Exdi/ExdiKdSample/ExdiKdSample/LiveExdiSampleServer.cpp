//----------------------------------------------------------------------------
//
// LiveExdiSampleServer.cpp
//
// A sample EXDI server class with support for setting breakpoints, stepping 
// and continuing execution.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "LiveExdiSampleServer.h"
#include "AsynchronousKDController.h"
#include "ExceptionHelpers.h"
#include "ArgumentHelpers.h"
#include "BasicExdiBreakpoint.h"

/*
    WARNING! Read this carefully before changing this code.

    All methods of an EXDI server are supposed to be non-blocking. E.g. when the users resume the execution
    of a system debugged over EXDI, the following sequence of events takes place:

    1. The debugging engine calls the Run() method of the EXDI server
    2. The EXDI server resumes the target and returns immediately
    3. The debug engine waits until the EXDI server reports that the target has stopped. It will not call any
       EXDI server's methods while it is waiting.

    The problem is that the target stop notification should be sent from the same main thread that receives
    calls from the engine. And the engine will use the thread to wait for an event and won't call any our methods
    to poll the state until we send an event from the same thread.

    There are 3 possible solutions to this:
    1. Put EXDI server in a multi-threaded apartment and let COM handle thread issues (not shown here).
    2. Register a timer that will periodically invoke our callback from the main thread. Use this callback
       to poll for JTAG events. This is the most simple but will introduce a latency equal to timer period.
       See the SampleTimerCallback() method for an example how to do this. It is currently provided for
       demonstration only and is not used by this sample.
       WARNING: This method will not work if the EXDI server runs in the CLSCTX_INPROC_SERVER mode as the
                debugging engine does not run message pump in this thread while waiting for certain events.

    3. Create an auxiliary thread that will detect 'target stopped' events and notify the main thread.
       This is achieved by declaring an additional interface in the IDL file and marshalling it to the
       auxiliary thread. COM will ensure that when the auxiliary thread calls OnAsynchronousCommandCompleted(),
       the corresponding method in this class will be called from the main thread and can actually deliver the
       necessary notifications to the debugging engine. This method is currently used by the sample.
*/

static const DWORD ConnectionCookie = 'SMPL';

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::DoSingleStep(DWORD dwProcessorNumber)
{
    try
    {
        DWORD processorCount;
        HRESULT result = GetNumberOfProcessors(&processorCount);
        if (FAILED(result))
        {
            return result;
        }

        if (dwProcessorNumber >= processorCount)
        {
            return E_INVALIDARG;
        }

        GetKDController()->StartStepCommand(dwProcessorNumber);

        if (m_pRunNotificationListener != nullptr)
        {
            m_pRunNotificationListener->NotifyRunStateChange(rsRunning, hrUser, 0, 0, dwProcessorNumber);
        }

        m_lastResumingCommandWasStep = true;
        m_targetIsRunning = true;
        ReleaseSemaphore(m_notificationSemaphore, 1, nullptr);

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::Run(void)
{
    try
    {
        GetKDController()->StartRunCommand();

        if (m_pRunNotificationListener != nullptr)
        {
            m_pRunNotificationListener->NotifyRunStateChange(rsRunning, hrUser, 0, 0, 0);
        }

        m_lastResumingCommandWasStep = false;
        m_targetIsRunning = true;
        ReleaseSemaphore(m_notificationSemaphore, 1, nullptr);

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::Halt(void)
{
    //TODO: add code that will stop the execution of the target over JTAG.
    //Do not forget to call m_pRunNotificationListener->NotifyRunStateChange() when the target
    //is actually stopped. Note that you can only call it from the main thread that receives calls
    //from debugging engine.

    //If your hardware debugger SDK uses asynchronous notification mechanism, use a trick similar to
    //OnAsynchronousCommandCompleted()
    MessageBox(0, _T("This EXDI sample does not support halting the target. Please connect a normal debugger,\
 halt the target and reconnect the sample."), nullptr, MB_ICONERROR);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::GetRunStatus( 
    /* [out] */ PRUN_STATUS_TYPE persCurrent,
    /* [out] */ PHALT_REASON_TYPE pehrCurrent,
    /* [out] */ ADDRESS_TYPE *pCurrentExecAddress,
    /* [out] */ DWORD *pdwExceptionCode,
    /* [out] */ DWORD *pdwProcessorNumberOfLastEvent)
{
    try
    {
        CheckAndZeroOutArgs(persCurrent, pehrCurrent, pCurrentExecAddress, pdwExceptionCode, pdwProcessorNumberOfLastEvent);

        if (m_targetIsRunning)
        {
            *persCurrent = rsRunning;
            *pehrCurrent = hrUnknown;
            *pCurrentExecAddress = 0;
        }
        else
        {
            *persCurrent = rsHalted;
            *pehrCurrent = hrUser;

            if (m_lastResumingCommandWasStep)
            {
                *pehrCurrent = hrStep;
            }

            *pCurrentExecAddress = GetCurrentExecutionAddress(pdwProcessorNumberOfLastEvent);
        }

	    *pdwExceptionCode = 0;

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}


HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::StartNotifyingRunChg(/* [in] */ IeXdiClientNotifyRunChg3 *pieXdiClientNotifyRunChg, 
                                                                      /* [out] */ DWORD *pdwConnectionCookie)
{
    if (pieXdiClientNotifyRunChg == nullptr || pdwConnectionCookie == nullptr)
    {
        return E_POINTER;
    }

    *pdwConnectionCookie = ConnectionCookie;

    //StartNotifyingRunChg is invoked by COM in STA environment, so no need for a critical section here
    if (m_pRunNotificationListener != nullptr)
    {
        //Theoretically EXDI servers can support more than one run change notification.
        //Practically, debugging engine only uses one and the support for multiple ones will most likely be deprecated.
        return E_FAIL;
    }

    m_pRunNotificationListener = pieXdiClientNotifyRunChg;

    return S_OK;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::StopNotifyingRunChg(/* [in] */ DWORD dwConnectionCookie)
{
    if (dwConnectionCookie != ConnectionCookie)
    {
        return E_INVALIDARG;
    }

    m_pRunNotificationListener = nullptr;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARM4 *pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        std::map<std::string, ULONGLONG> registers;
        if (pContext->RegGroupSelection.fIntegerRegs)
        {
            registers["r0"] = pContext->R0;
            registers["r1"] = pContext->R1;
            registers["r2"] = pContext->R2;
            registers["r3"] = pContext->R3;
            registers["r4"] = pContext->R4;
            registers["r5"] = pContext->R5;
            registers["r6"] = pContext->R6;
            registers["r7"] = pContext->R7;
            registers["r8"] = pContext->R8;
            registers["r9"] = pContext->R9;
            registers["r10"] = pContext->R10;
            registers["r11"] = pContext->R11;
            registers["r12"] = pContext->R12;
            registers["sp"] = pContext->Sp;
            registers["lr"] = pContext->Lr;
            registers["pc"] = pContext->Pc;
            registers["psr"] = pContext->Psr;
        }

        GetKDController()->SetRegisters(processorNumber, registers);
        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_X86_64 *pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        std::map<std::string, ULONGLONG> registers;
        if (pContext->RegGroupSelection.fIntegerRegs)
        {
            registers["rax"] = pContext->Rax;
            registers["rbx"] = pContext->Rbx;
            registers["rcx"] = pContext->Rcx;
            registers["rdx"] = pContext->Rdx;
            registers["rsi"] = pContext->Rsi;
            registers["rdi"] = pContext->Rdi;
            registers["rip"] = pContext->Rip;
            registers["rsp"] = pContext->Rsp;
            registers["rbp"] = pContext->Rbp;
            registers["r8"] = pContext->R8;
            registers["r9"] = pContext->R9;
            registers["r10"] = pContext->R10;
            registers["r11"] = pContext->R11;
            registers["r12"] = pContext->R12;
            registers["r13"] = pContext->R13;
            registers["r14"] = pContext->R14;
            registers["r15"] = pContext->R15;
        }

        if (pContext->RegGroupSelection.fSegmentRegs)
        {
            registers["cs"] = pContext->SegCs;
            registers["ss"] = pContext->SegSs;
            registers["ds"] = pContext->SegDs;
            registers["es"] = pContext->SegEs;
            registers["fs"] = pContext->SegFs;
            registers["gs"] = pContext->SegGs;
        }

        GetKDController()->SetRegisters(processorNumber, registers);
        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARMV8ARCH64 *pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        std::map<std::string, ULONGLONG> registers;
        if (pContext->RegGroupSelection.fIntegerRegs)
        {
            for (int i = 0; i < ARMV8ARCH64_MAX_INTERGER_REGISTERS; i++)
            {
                char registerNameStr[4];
                sprintf_s(registerNameStr, _countof(registerNameStr), "x%d", i);
			    std::string registerName(registerNameStr);
                registers[registerName] = pContext->X[i];
            }
            registers["fp"] = pContext->Fp;
            registers["lr"] = pContext->Lr;
        }

        if (pContext->RegGroupSelection.fControlRegs)
        {
            registers["pc"] = pContext->Pc;
            registers["sp"] = pContext->Sp;
            registers["psr"] = pContext->Psr;
        }
        GetKDController()->SetRegisters(processorNumber, registers);
        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}


HRESULT CLiveExdiSampleServer::FinalConstruct()
{
    HRESULT result = CStaticExdiSampleServer::FinalConstruct();
    if (SUCCEEDED(result))
    {
        m_pSelfReferenceForNotificationThread = 
            new InterfaceMarshalHelper<IAsynchronousCommandNotificationReceiver>(this, MSHLFLAGS_TABLEWEAK);
    }

    m_notificationSemaphore = CreateSemaphore(nullptr, 0, LONG_MAX, nullptr);
    assert(m_notificationSemaphore != nullptr);
    DWORD threadId;
    m_notificationThread = CreateThread(nullptr, 0, NotificationThreadBody, this, 0, &threadId);
    assert(m_notificationThread != nullptr);

    m_timerId = SetTimer(nullptr, 0, 100, SampleTimerCallback);
    assert(m_timerId != 0);

    return result;
}

VOID CALLBACK CLiveExdiSampleServer::SampleTimerCallback(_In_  HWND hwnd, _In_  UINT uMsg, _In_  UINT_PTR idEvent, _In_  DWORD dwTime)
{
	UNREFERENCED_PARAMETER(hwnd);
	UNREFERENCED_PARAMETER(uMsg);
	UNREFERENCED_PARAMETER(idEvent);
	UNREFERENCED_PARAMETER(dwTime);
    //TODO: If your JTAG hardware supports polling mode rather than asynchronous notification mode, use this
    //      method to poll whether the target has stopped on an event and send a notification to debugging engine
    //      by calling m_pRunNotificationListener->NotifyRunStateChange().
}

static DWORD WaitForSingleObjectWhileDispatchingMessages(HANDLE object, DWORD timeout)
{
    for (;;)
    {
        DWORD waitStatus = MsgWaitForMultipleObjectsEx(1, &object, timeout, QS_ALLEVENTS, 0);
        if (waitStatus == WAIT_OBJECT_0 + 1)
        {
            MSG msg;

            if (GetMessage(&msg, NULL, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            return waitStatus;
        }
    }
}

void CLiveExdiSampleServer::FinalRelease()
{
    m_terminating = true;

    if (m_timerId != 0)
    {
        KillTimer(nullptr, m_timerId);
        m_timerId = 0;
    }

    ReleaseSemaphore(m_notificationSemaphore, 1, nullptr);
    WaitForSingleObjectWhileDispatchingMessages(m_notificationThread, INFINITE);
    CStaticExdiSampleServer::FinalRelease();
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::OnAsynchronousCommandCompleted(void)
{
    if (m_pRunNotificationListener != nullptr)
    {
        HALT_REASON_TYPE haltReason = hrUnknown;

        if (m_lastResumingCommandWasStep)
        {
            haltReason = hrStep;
        }

        m_targetIsRunning = false;
        DWORD eventProcessor = 0;
        ADDRESS_TYPE currentAddress = GetCurrentExecutionAddress(&eventProcessor);
        m_pRunNotificationListener->NotifyRunStateChange(rsHalted, haltReason, currentAddress, 0, eventProcessor);
    }

    return S_OK;
}

DWORD CALLBACK CLiveExdiSampleServer::NotificationThreadBody(LPVOID p)
{
    CLiveExdiSampleServer *pServer = reinterpret_cast<CLiveExdiSampleServer *>(p);

    HRESULT result = CoInitialize(nullptr);
    assert(SUCCEEDED(result));
	UNREFERENCED_PARAMETER(result);

    for (;;)
    {
        DWORD waitResult = WaitForSingleObject(pServer->m_notificationSemaphore, 100);

        if (pServer->m_terminating)
        {
            break;
        }

        assert(pServer->m_pSelfReferenceForNotificationThread != nullptr);
        
        IAsynchronousCommandNotificationReceiver *pReceiver  =
            pServer->m_pSelfReferenceForNotificationThread->TryUnmarshalInterfaceForCurrentThread();        

        if (!pServer->m_terminating)
        {
            if (pReceiver == nullptr)
            {
                throw new std::exception("Cannot send any requests to the main COM thread");
            }

            pReceiver->PerformKeepaliveChecks();

            if (waitResult == WAIT_OBJECT_0)
            {
                pServer->GetKDController()->GetAsynchronousCommandResult(INFINITE, nullptr);
        
                pReceiver->OnAsynchronousCommandCompleted();
            }
        }

        if (pReceiver != nullptr)
        {
            pReceiver->Release();
        }
    }

    CoUninitialize();
    return 0;
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::AddCodeBreakpoint( 
    /* [in] */ ADDRESS_TYPE Address,
    /* [in] */ CBP_KIND cbpk,
    /* [in] */ MEM_TYPE mt,
    /* [in] */ DWORD dwExecMode,
    /* [in] */ DWORD dwTotalBypassCount,
    /* [out] */ IeXdiCodeBreakpoint3 **ppieXdiCodeBreakpoint)
{
	UNREFERENCED_PARAMETER(cbpk);
	UNREFERENCED_PARAMETER(dwTotalBypassCount);
	UNREFERENCED_PARAMETER(dwExecMode);

    if (ppieXdiCodeBreakpoint == nullptr)
    {
        return E_POINTER;
    }

    *ppieXdiCodeBreakpoint = nullptr;

    if (mt != mtVirtual)
    {
        return E_INVALIDARG;
    }

    //Note that we ignore dwTotalBypassCount here

    try
    {
        unsigned breakpointNumber = GetKDController()->CreateCodeBreakpoint(Address);
        
        BasicExdiBreakpoint *pBreakpoint = new CComObject<BasicExdiBreakpoint>();
        pBreakpoint->Initialize(Address, breakpointNumber);
        pBreakpoint->AddRef();
        *ppieXdiCodeBreakpoint = pBreakpoint;
        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::DelCodeBreakpoint( 
    /* [in] */ IeXdiCodeBreakpoint3 *pieXdiCodeBreakpoint)
{
    if (pieXdiCodeBreakpoint == nullptr)
    {
        return E_POINTER;
    }

    CComPtr<IBasicExdiBreakpoint> pBreakpoint;
    HRESULT result = pieXdiCodeBreakpoint->QueryInterface(&pBreakpoint);
    if (SUCCEEDED(result))
    {
        unsigned breakpointNumber = pBreakpoint->GetBreakpointNumber();

        try
        {
            GetKDController()->DeleteCodeBreakpoint(breakpointNumber);
            result = S_OK;
        }
        CATCH_AND_RETURN_HRESULT;
    }

    return result;
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::SetKeepaliveInterface(/* [in] */ IeXdiKeepaliveInterface3 *pKeepalive)
{
    m_pKeepaliveInterface = pKeepalive;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CLiveExdiSampleServer::PerformKeepaliveChecks(void)
{
    if (m_pKeepaliveInterface == nullptr)
    {
        return S_FALSE;
    }
    
    HRESULT result = m_pKeepaliveInterface->IsDebugSessionAlive();
    if (FAILED(result))
    {
        GetKDController()->ShutdownKD();

        TCHAR fileName[MAX_PATH];
        bool lostConnection = HRESULT_FACILITY(result) == FACILITY_WIN32 && 
            ((HRESULT_CODE(result) == RPC_S_CALL_FAILED) || (HRESULT_CODE(result) == RPC_S_SERVER_UNAVAILABLE));

        if (lostConnection && GetModuleFileName(GetModuleHandle(0), fileName, _countof(fileName)))
        {
            TCHAR *pLastSlash = _tcsrchr(fileName, '\\');
            if (pLastSlash && !_tcsicmp(pLastSlash + 1, _T("dllhost.exe")))
            {
                //We are running out-of-process using dllhost.exe and lost connection to WinDbg or another debugger.
                //COM won't stop our process until a long timeout expires and we want to have our DLL unloaded 
                //ASAP so that you can build another version of it and try it. Thus we exit dllhost explicitly.
                ExitProcess(result);
            }
        }
    }

    return S_OK;
}
