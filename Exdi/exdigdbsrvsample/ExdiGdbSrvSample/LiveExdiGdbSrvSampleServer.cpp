//----------------------------------------------------------------------------
//
//  LiveExdiGdbSrvSampleServer.cpp  Implementation of CLiveExdiGdbSrvSampleServerclass
//  This class implements the following interfaces:
//		[default] interface IeXdiServer3;
//      interface IeXdiARM4Context3;
//      interface IeXdiX86_64Context3;
//		interface IeXdiX86ExContext3;
//      interface IAsynchronousCommandNotificationReceiver;
//  
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "LiveExdiGdbSrvSampleServer.h"
#include "ComHelpers.h"
#include "AsynchronousGdbSrvController.h"
#include "CommandLogger.h"
#include "ArgumentHelpers.h"
#include "ExceptionHelpers.h"
#include "GdbSrvRspClient.h"
#include "BasicExdiBreakpoint.h"
#include "dbgeng_exdi_io.h"
#include "cfgExdiGdbSrvHelper.h"
#include <string>
#include <algorithm>
#include <vector>

#define METHOD_NOT_IMPLEMENTED if (IsDebuggerPresent()) \
                                   __debugbreak(); \
                               return E_NOTIMPL

using namespace GdbSrvControllerLib;

//=============================================================================
// Private defines and typedefs
//=============================================================================
//  AMD64 Context Flag definitions
#define AMD64_CONTEXT_AMD64             0x00100000L
#define AMD64_CONTEXT_CONTROL           (AMD64_CONTEXT_AMD64 | 0x00000001L)
#define AMD64_CONTEXT_INTEGER           (AMD64_CONTEXT_AMD64 | 0x00000002L)
#define AMD64_CONTEXT_SEGMENTS          (AMD64_CONTEXT_AMD64 | 0x00000004L)
#define AMD64_CONTEXT_FLOATING_POINT    (AMD64_CONTEXT_AMD64 | 0x00000008L)
#define AMD64_CONTEXT_DEBUG_REGISTERS   (AMD64_CONTEXT_AMD64 | 0x00000010L)
#define AMD64_CONTEXT_FULL \
    (AMD64_CONTEXT_CONTROL | AMD64_CONTEXT_INTEGER | AMD64_CONTEXT_FLOATING_POINT)

//  Used to allow correctly processing of the Segment descriptors by the disassembler
#define X86_DESC_PRESENT                0x80
#define X86_DESC_LONG_MODE              0x200
#define X86_DESC_DEFAULT_BIG            0x400
#define SEGDESC_INVALID                 0xffffffff
#define X86_DESC_FLAGS                  (X86_DESC_DEFAULT_BIG | X86_DESC_PRESENT)


//=============================================================================
// Global data definitions
//=============================================================================    
//  Connection server string
const DWORD s_ConnectionCookie = 'SMPL';

//  SSE register list
const char * s_sseRegList[] = {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"};
//  Number of SSE registers
const int s_numberOfSseRegisters = (ARRAYSIZE(s_sseRegList));
//  SSE x64 register list
const char* s_sseX64RegList[] = { "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
                                  "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" };
//  Number of SSE registers
const int s_numberOfSseX64Registers = (ARRAYSIZE(s_sseX64RegList));

//  80387 coprocessor register info.
const int s_numberOfCoprocessorRegisters = 8;
const int s_numberOfBytesCoprocessorRegister = (SIZE_OF_80387_REGISTERS_IN_BYTES / s_numberOfCoprocessorRegisters);
const char * s_fpRegList[] = {"st0", "st1", "st2", "st3", "st4", "st5", "st6", "st7"};
const int s_numberFPRegList = (ARRAYSIZE(s_fpRegList));

//=============================================================================
// Public function definitions
//=============================================================================

// CLiveExdiGdbSrvSampleServer

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetTargetInfo( 
    /* [out] */ PGLOBAL_TARGET_INFO_STRUCT pgti)
{
    CheckAndZeroOutArgs(pgti);

    pgti->TargetProcessorFamily = m_detectedProcessorFamily;
    pgti->szProbeName = COMHelpers::CopyStringToTaskMem(L"ExdiGdbServerSample");
    if (pgti->szProbeName == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    pgti->szTargetName= COMHelpers::CopyStringToTaskMem(L"GdbServer Target");
    if (pgti->szTargetName == nullptr)
    {
        CoTaskMemFree(reinterpret_cast<LPVOID>(pgti->szProbeName));
        return E_OUTOFMEMORY;
    }
    memset(&pgti->dbc, 0, sizeof(pgti->dbc));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetRunStatus( 
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

        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::Run(void)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        pController->ResetAsynchronousCmdStopReplyPacket();
        pController->StartRunCommand();

        if (m_pRunNotificationListener != nullptr)
        {
            m_pRunNotificationListener->NotifyRunStateChange(rsRunning, hrUser, 0, 0, 0);
        }

        m_lastResumingCommandWasStep = false;
        pController->SetAsynchronousCmdStopReplyPacket();
        m_targetIsRunning = true;
        ReleaseSemaphore(m_notificationSemaphore, 1, nullptr);

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
//
//  Halt                Interrupt the target
//
//  Request:
//      0x03 character -> Interrupt target character
//      ?              -> Query target halt reason if we don't receive a stop reply packet
//
//  Response:
//      
//      T02.....       -> stop reply packet with a signal SIGINT.
//
//  Note:
//  GDB is almost entirely non-preemptive, which is reflected in the sequence of packet exchanges of RSP. 
//  The exception is when GDB wishes to interrupt an executing program (via ctrl-break). 
//  A single byte, 0x03, is sent (no packet structure). If the target is prepared to handle such interrupts 
//  it should recognize such byte. However not all servers are capable of handling such request. 
//  The server is free to ignore such out-of-band characters. 
//
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::Halt(void)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        HRESULT hr = E_FAIL;

        if (m_pRunNotificationListener != nullptr)
        {
            DWORD eventProcessor = 0;
            ADDRESS_TYPE currentAddress = m_lastPcAddress;
            bool eventNotification = false;
            if (pController->HandleInterruptTarget(reinterpret_cast<AddressType *>(&currentAddress), &eventProcessor, &eventNotification))
            {
                if (currentAddress != 0)
                {
                    m_lastPcAddress = currentAddress; 
                }
                if (eventNotification)
                {
                    m_pRunNotificationListener->NotifyRunStateChange(rsHalted, hrUser, currentAddress, 0, eventProcessor);
                }
                hr = S_OK;
            }
            else
            {
                MessageBox(0, _T("The Target break interrupt command failed or the GdbServer does not support the break command."),nullptr, MB_ICONERROR);
                hr = E_NOTIMPL;
            }
        }
        else
        {
            MessageBox(0, _T("Fatal error the Notification listener is not defined."),nullptr, MB_ICONERROR);
            hr = E_NOTIMPL;
        }

        return hr;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::DoSingleStep(DWORD dwProcessorNumber)
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
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        pController->ResetAsynchronousCmdStopReplyPacket();
        pController->StartStepCommand(dwProcessorNumber);

        if (m_pRunNotificationListener != nullptr)
        {
            m_pRunNotificationListener->NotifyRunStateChange(rsRunning, hrUser, 0, 0, dwProcessorNumber);
        }

        m_lastResumingCommandWasStep = true;
        pController->SetAsynchronousCmdStopReplyPacket();
        m_targetIsRunning = true;
        ReleaseSemaphore(m_notificationSemaphore, 1, nullptr);

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::Reboot(void)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        //  This should reboot only the target machine.
        if (pController->RestartGdbSrvTarget())
        {
            return S_OK;
        }
        return E_FAIL;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetNbCodeBpAvail( 
    /* [out] */ DWORD *pdwNbHwCodeBpAvail,
    /* [out] */ DWORD *pdwNbSwCodeBpAvail)
{
    if (pdwNbHwCodeBpAvail == nullptr || pdwNbSwCodeBpAvail == nullptr)
    {
        return E_POINTER;
    }

    *pdwNbHwCodeBpAvail = *pdwNbSwCodeBpAvail = 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetNbDataBpAvail( 
    /* [out] */ DWORD *pdwNbDataBpAvail)
{
    if (pdwNbDataBpAvail == nullptr)
    {
        return E_POINTER;
    }

    //  We support data breakpoints
    *pdwNbDataBpAvail = 1;
    return S_OK;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::AddCodeBreakpoint( 
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

    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        unsigned breakpointNumber = pController->CreateCodeBreakpoint(Address);
        
        BasicExdiBreakpoint *pBreakpoint = new CComObject<BasicExdiBreakpoint>();
        pBreakpoint->Initialize(Address, breakpointNumber);
        pBreakpoint->AddRef();
        *ppieXdiCodeBreakpoint = pBreakpoint;
        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::DelCodeBreakpoint( 
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
        ADDRESS_TYPE address = pBreakpoint->GetBreakPointAddress();
        unsigned breakpointNumber = pBreakpoint->GetBreakpointNumber();
        try
        {
            AsynchronousGdbSrvController * pController = GetGdbSrvController();
            if (pController != nullptr)
            {
                pController->DeleteCodeBreakpoint(breakpointNumber, address);
                result = S_OK;
            }
            else
            {
                result = E_POINTER;
            }
        }
        CATCH_AND_RETURN_HRESULT;
    }
    return result;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::AddDataBreakpoint( 
    /* [in] */ ADDRESS_TYPE Address,
    /* [in] */ ADDRESS_TYPE AddressMask,
    /* [in] */ DWORD dwData,
    /* [in] */ DWORD dwDataMask,
    /* [in] */ BYTE bAccessWidth,
    /* [in] */ MEM_TYPE mt,
    /* [in] */ BYTE bAddressSpace,
    /* [in] */ DATA_ACCESS_TYPE da,
    /* [in] */ DWORD dwTotalBypassCount,
    /* [out] */ IeXdiDataBreakpoint3 **ppieXdiDataBreakpoint)
{
    //  Note that we do not have a way to to set these parameters with
    //  the GdbServer request commands.
    UNREFERENCED_PARAMETER(AddressMask);
    UNREFERENCED_PARAMETER(dwData);
    UNREFERENCED_PARAMETER(dwDataMask);
    UNREFERENCED_PARAMETER(bAddressSpace);
    UNREFERENCED_PARAMETER(dwTotalBypassCount);

    if (ppieXdiDataBreakpoint == nullptr)
    {
        return E_POINTER;
    }

    *ppieXdiDataBreakpoint = nullptr;

    if (mt != mtVirtual)
    {
        return E_INVALIDARG;
    }

    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        unsigned breakpointNumber = pController->CreateDataBreakpoint(Address, bAccessWidth, da);
        
        BasicExdiDataBreakpoint * pBreakpoint = new CComObject<BasicExdiDataBreakpoint>();
        pBreakpoint->Initialize(Address, breakpointNumber, da, bAccessWidth);
        pBreakpoint->AddRef();
        *ppieXdiDataBreakpoint = pBreakpoint;
        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::DelDataBreakpoint( 
    /* [in] */ IeXdiDataBreakpoint3 * pieXdiDataBreakpoint)
{
    if (pieXdiDataBreakpoint == nullptr)
    {
        return E_POINTER;
    }

    CComPtr<IBasicExdiDataBreakpoint> pBreakpoint;
    HRESULT result = pieXdiDataBreakpoint->QueryInterface(&pBreakpoint);
    if (SUCCEEDED(result))
    {
        ADDRESS_TYPE address = pBreakpoint->GetBreakPointAddress();
        unsigned breakpointNumber = pBreakpoint->GetBreakpointNumber();
        BYTE accessWidth = pBreakpoint->GetBreakPointAccessWidth();
        DATA_ACCESS_TYPE accessType = pBreakpoint->GetBreakPointAccessType();
        try
        {
            AsynchronousGdbSrvController * pController = GetGdbSrvController();
            if (pController != nullptr)
            {
                pController->DeleteDataBreakpoint(breakpointNumber, address, accessWidth, accessType);
                result = S_OK;
            }
            else
            {
                result = E_POINTER;
            }
        }
        CATCH_AND_RETURN_HRESULT;
    }
    return result;
}


HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::StartNotifyingRunChg( 
    /* [in] */ IeXdiClientNotifyRunChg3 *pieXdiClientNotifyRunChg,
    /* [out] */ DWORD *pdwConnectionCookie)
{
    if (pieXdiClientNotifyRunChg == nullptr || pdwConnectionCookie == nullptr)
    {
        return E_POINTER;
    }

    *pdwConnectionCookie = s_ConnectionCookie;

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

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::StopNotifyingRunChg( 
    /* [in] */ DWORD dwConnectionCookie)
{
    if (dwConnectionCookie != s_ConnectionCookie)
    {
        return E_INVALIDARG;
    }

    m_pRunNotificationListener = nullptr;

    return S_OK;
}

static HRESULT SafeArrayFromByteArray(_In_reads_bytes_(arraySize) const char *pByteArray, size_t arraySize, _Out_ SAFEARRAY **pSafeArray)
{
    assert(pByteArray != nullptr && pSafeArray != nullptr);
    ULONG copiedSize = static_cast<ULONG>(arraySize);
    *pSafeArray = SafeArrayCreateVector(VT_UI1, 0, copiedSize);
    if (*pSafeArray == nullptr)
    {
        return E_FAIL;
    }

    memcpy((*pSafeArray)->pvData, pByteArray, copiedSize);

    return S_OK;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::ReadVirtualMemory( 
    /* [in] */ ADDRESS_TYPE Address,
    /* [in] */ DWORD dwBytesToRead,
    SAFEARRAY * *pbReadBuffer)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pbReadBuffer == nullptr || pController == nullptr)
        {
            return E_POINTER;
        }

        memoryAccessType memType = {0};
        pController->GetMemoryPacketType(m_lastPSRvalue, &memType);

        SimpleCharBuffer buffer = pController->ReadMemory(Address, dwBytesToRead, memType);
        return SafeArrayFromByteArray(buffer.GetInternalBuffer(), buffer.GetLength(), pbReadBuffer);
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::WriteVirtualMemory( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ SAFEARRAY * pBuffer,
        /* [out] */ DWORD *pdwBytesWritten)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pBuffer == nullptr || pdwBytesWritten == nullptr || pController == nullptr)
        {
            return E_POINTER;
        }

        if (pBuffer->cDims != 1)
        {
            return E_INVALIDARG;
        }

        VARTYPE dataType;
        if (FAILED(SafeArrayGetVartype(pBuffer, &dataType)) || dataType != VT_UI1)
        {
            return E_INVALIDARG;
        }
    
        ULONG bufferSize = pBuffer->rgsabound[0].cElements;
        PVOID pRawBuffer = pBuffer->pvData;

        memoryAccessType memType = {0};
        pController->GetMemoryPacketType(m_lastPSRvalue, &memType);

        bool isWriteDone = pController->WriteMemory(Address, bufferSize, pRawBuffer, pdwBytesWritten, memType);
        if (isWriteDone)
        {
            return S_OK;
        }
        return E_FAIL;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::ReadPhysicalMemoryOrPeriphIO( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ ADDRESS_SPACE_TYPE AddressSpace,
        /* [in] */ DWORD dwBytesToRead,
        /* [out] */ SAFEARRAY * *pReadBuffer)
{
    UNREFERENCED_PARAMETER(AddressSpace);

    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pReadBuffer == nullptr || pController == nullptr)
        {
            return E_POINTER;
        }

        memoryAccessType memoryType = {0};
        memoryType.isPhysical = 1;
        SimpleCharBuffer buffer = pController->ReadMemory(Address, dwBytesToRead, memoryType);
        return SafeArrayFromByteArray(buffer.GetInternalBuffer(), buffer.GetLength(), pReadBuffer);
    }
    CATCH_AND_RETURN_HRESULT;
}
        

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::WritePhysicalMemoryOrPeriphIO( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ ADDRESS_SPACE_TYPE AddressSpace,
        /* [in] */ SAFEARRAY * pBuffer,
        /* [out] */ DWORD *pdwBytesWritten)
{
    UNREFERENCED_PARAMETER(AddressSpace);
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pBuffer == nullptr || pdwBytesWritten == nullptr || pController == nullptr)
        {
            return E_POINTER;
        }

        if (pBuffer->cDims != 1)
        {
            return E_INVALIDARG;
        }

        VARTYPE dataType;
        if (FAILED(SafeArrayGetVartype(pBuffer, &dataType)) || dataType != VT_UI1)
        {
            return E_INVALIDARG;
        }
    
        ULONG bufferSize = pBuffer->rgsabound[0].cElements;
        PVOID pRawBuffer = pBuffer->pvData;

        memoryAccessType memType = {0};
        memType.isPhysical = 1;
        bool isWriteDone = pController->WriteMemory(Address, bufferSize, pRawBuffer, pdwBytesWritten, memType);
        if (isWriteDone)
        {
            return S_OK;
        }
        return E_FAIL;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::Ioctl( 
        /* [in] */ SAFEARRAY * pInputBuffer,
        /* [in] */ DWORD dwBuffOutSize,
        /* [out] */ SAFEARRAY * *pOutputBuffer)
{
    AsynchronousGdbSrvController * pController = GetGdbSrvController();
    if (pOutputBuffer == nullptr || pController == nullptr)
    {
        return E_POINTER;
    }

    HRESULT hr = E_INVALIDARG;
    VARTYPE dataType;
    if (FAILED(SafeArrayGetVartype(pInputBuffer, &dataType)) || dataType != VT_UI1)
    {
        return hr;
    }

    PVOID pRawBuffer = pInputBuffer->pvData;
    if (pRawBuffer == nullptr)
    {
        return hr;
    }

    try
    {
        ULONG bufferSize = pInputBuffer->rgsabound[0].cElements;
        const DBGENG_EXDI_IOCTL_CODE_V3_EX * pExdiV3 = reinterpret_cast<const DBGENG_EXDI_IOCTL_CODE_V3_EX *>(pRawBuffer);
        DBGENG_EXDI_IOCTL_CODE_V3_EX ioctlCode = *pExdiV3;
        switch(ioctlCode)
        {
            //  Get additional gdb server info that will be used by the debugger engine
            case DBGENG_EXDI_IOCTL_V3_GET_ADDITIONAL_SERVER_INFO:
            {
                if (bufferSize == sizeof(DBGENG_EXDI_IOCTL_V3_GET_ADDITIONAL_SERVER_INFO_EX_IN))
                {
                    const DBGENG_EXDI_IOCTL_V3_GET_ADDITIONAL_SERVER_INFO_EX_IN * pAdditionalInfo =
                        reinterpret_cast<const DBGENG_EXDI_IOCTL_V3_GET_ADDITIONAL_SERVER_INFO_EX_IN *>(pRawBuffer);
                    if (pAdditionalInfo -> request.HeuristicChunkSize)
                    {
                        size_t bytesToCopy = min(dwBuffOutSize, sizeof(m_heuristicChunkSize));
                        hr = SafeArrayFromByteArray(reinterpret_cast<const char *>(&m_heuristicChunkSize), bytesToCopy, pOutputBuffer);
                    }
                }
            }
            break;

            //  Store the KPCR value
            case DBGENG_EXDI_IOCTL_V3_STORE_KPCR_VALUE:
            {
                if (bufferSize == sizeof(DBGENG_EXDI_IOCTL_STORE_KPCR_V3_EX_IN))
                {
                    const DBGENG_EXDI_IOCTL_STORE_KPCR_V3_EX_IN * pKPCRV3 = reinterpret_cast<const DBGENG_EXDI_IOCTL_STORE_KPCR_V3_EX_IN *>(pRawBuffer);
                    //  Extract the processor number
                    ULONG processorNumber = pKPCRV3->processorNumber;
                    //  Extract the processor block array offset
                    ULONG64 kpcrOffset = pKPCRV3->kpcrOffset;
                    if (kpcrOffset != 0)
                    {
                        pController->SetKpcrOffset(processorNumber, kpcrOffset);
                        size_t bytesToCopy = min(dwBuffOutSize, sizeof(kpcrOffset));
                        hr = SafeArrayFromByteArray(reinterpret_cast<const char *>(&kpcrOffset), bytesToCopy, pOutputBuffer);
                    }
                }
            }
            break;

            //  This is not implemented by this COM server Exdi sample.
            case DBGENG_EXDI_IOCTL_V3_GET_NT_BASE_ADDRESS_VALUE:
            {
                hr = E_NOTIMPL;
            }
            break;

            //  Read the special registers content Architecture specific.
            case DBGENG_EXDI_IOCTL_V3_GET_SPECIAL_REGISTER_VALUE:
            {
                if (bufferSize == sizeof(DBGENG_EXDI_IOCTL_READ_SPECIAL_MEMORY_EX_IN))
                {
                    DBGENG_EXDI_IOCTL_READ_SPECIAL_MEMORY_EX_IN* const pSpecialRegs = reinterpret_cast<DBGENG_EXDI_IOCTL_READ_SPECIAL_MEMORY_EX_IN* const>(pRawBuffer);
                    memoryAccessType memoryType = { 0 };
                    memoryType.isSpecialRegs = 1;
                    SimpleCharBuffer buffer = pController->ReadSystemRegisters(pSpecialRegs->address, pSpecialRegs->bytesToRead, memoryType);
                    hr = SafeArrayFromByteArray(buffer.GetInternalBuffer(), buffer.GetLength(), pOutputBuffer);
                }
            }
            break;

            //  Read the special memory content Architecture specific.
            case DBGENG_EXDI_IOCTL_V3_GET_SUPERVISOR_MODE_MEM_VALUE:
            case DBGENG_EXDI_IOCTL_V3_GET_HYPERVISOR_MODE_MEM_VALUE:
            {
                if (bufferSize == sizeof(DBGENG_EXDI_IOCTL_READ_SPECIAL_MEMORY_EX_IN))
                {
                    DBGENG_EXDI_IOCTL_READ_SPECIAL_MEMORY_EX_IN * const pSpecialRegs = reinterpret_cast<DBGENG_EXDI_IOCTL_READ_SPECIAL_MEMORY_EX_IN * const>(pRawBuffer);
                    memoryAccessType memoryType = {0};
                    if (ioctlCode == DBGENG_EXDI_IOCTL_V3_GET_HYPERVISOR_MODE_MEM_VALUE)
                    {
                        memoryType.isHypervisor = 1;
                    }
                    else
                    {
                        memoryType.isSupervisor = 1;
                    }
                    SimpleCharBuffer buffer = pController->ReadMemory(pSpecialRegs->address, pSpecialRegs->bytesToRead, memoryType);
                    hr = SafeArrayFromByteArray(buffer.GetInternalBuffer(), buffer.GetLength(), pOutputBuffer);
                }
            }
            break;

            default:
            {
                hr = E_NOTIMPL;
            }
        }
        return hr;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetLastHitBreakpoint( 
    /* [out] */ DBGENG_EXDI3_GET_BREAKPOINT_HIT_OUT *pBreakpointInformation)
{
    UNREFERENCED_PARAMETER(pBreakpointInformation);
    //  The current dbgeng.dll Exdi target does not use this function for Intel targets.
    //  Also, there is no a debugger command that calls this function.
    return E_NOTIMPL;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetKPCRForProcessor( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [out] */ ULONG64 *pKPCRPointer)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pKPCRPointer == nullptr || pController == nullptr)
        {
            return E_POINTER;
        }
        DWORD totalProcessors = 0;
        HRESULT result = GetNumberOfProcessors(&totalProcessors);
        if (FAILED(result))
        {
            return result;
        }
        if (dwProcessorNumber >= totalProcessors)
        {
            return E_INVALIDARG;
        }
        *pKPCRPointer = pController->GetKpcrOffset(dwProcessorNumber);
        if (*pKPCRPointer == 0)
        {
            return E_NOTIMPL;
        }
        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::ReadKdVersionBlock( 
        /* [in] */ DWORD dwBufferSize,
        /* [out] */ SAFEARRAY * *pKdVersionBlockBuffer)
{
    UNREFERENCED_PARAMETER(dwBufferSize);
    UNREFERENCED_PARAMETER(pKdVersionBlockBuffer);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::ReadMSR( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ DWORD dwRegisterIndex,
    /* [out] */ ULONG64 *pValue)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pValue == nullptr || pController == nullptr)
        {
            return E_POINTER;
        }

        return pController->ReadMsrRegister(dwProcessorNumber, dwRegisterIndex, pValue);
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::WriteMSR( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ DWORD dwRegisterIndex,
    /* [in] */ ULONG64 value)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }

        return pController->WriteMsrRegister(dwProcessorNumber, dwRegisterIndex, value);
    }
    CATCH_AND_RETURN_HRESULT;
}


// ------------------------------------------------------------------------------


HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [out][in] */ PCONTEXT_ARM4 pContext)
{
    return GetContextEx(dwProcessorNumber, pContext);
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::SetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ CONTEXT_ARM4 Context)
{
    return SetContextEx(dwProcessorNumber, &Context);
}

// ------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [out][in] */ PCONTEXT_X86_64 pContext)
{
    return GetContextEx(dwProcessorNumber, pContext);
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::SetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ CONTEXT_X86_64 Context)
{
    return SetContextEx(dwProcessorNumber, &Context);
}

// ------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [out][in] */ PCONTEXT_X86_EX pContext)
{
    return GetContextEx(dwProcessorNumber, pContext);
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::SetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ CONTEXT_X86_EX Context)
{
    return SetContextEx(dwProcessorNumber, &Context);
}

// ------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [out][in] */ PCONTEXT_ARMV8ARCH64 pContext)
{
    return GetContextEx(dwProcessorNumber, pContext);
}
        
HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::SetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ CONTEXT_ARMV8ARCH64 context)
{
    return SetContextEx(dwProcessorNumber, &context);
}

// ------------------------------------------------------------------------------

HRESULT CLiveExdiGdbSrvSampleServer::FinalConstruct()
{
    if (SetGdbServerParameters() != S_OK)
    {
        return E_ABORT;
    }

    AsynchronousGdbSrvController * pController = GetGdbSrvController();
    if (pController == nullptr)
    {
        return E_POINTER;
    }
    //  Execute the connection to the GdbServer
    if (SetGdbServerConnection() != S_OK)
    {
        return E_FAIL;
    }
    m_pSelfReferenceForNotificationThread = 
        new InterfaceMarshalHelper<IAsynchronousCommandNotificationReceiver>(this, MSHLFLAGS_TABLEWEAK);

    m_notificationSemaphore = CreateSemaphore(nullptr, 0, LONG_MAX, nullptr);
    if (m_notificationSemaphore == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    DWORD threadId;
    m_notificationThread = CreateThread(nullptr, 0, NotificationThreadBody, this, 0, &threadId);
    if (m_notificationThread == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    m_timerId = SetTimer(nullptr, 0, 100, SampleTimerCallback);
    assert(m_timerId != 0);

    return S_OK;
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

void CLiveExdiGdbSrvSampleServer::FinalRelease()
{
    m_terminating = true;

    if (m_timerId != 0)
    {
        KillTimer(nullptr, m_timerId);
        m_timerId = 0;
    }

    ReleaseSemaphore(m_notificationSemaphore, 1, nullptr);
    WaitForSingleObjectWhileDispatchingMessages(m_notificationThread, INFINITE);

    delete m_pGdbSrvController;
    m_pGdbSrvController = nullptr;	
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetNumberOfProcessors( 
    /* [out] */ DWORD *pdwNumberOfProcessors)
{
    if (pdwNumberOfProcessors == nullptr)
    {
        return E_POINTER;
    }
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }

        *pdwNumberOfProcessors = pController->GetProcessorCount();

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_ARM4 pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }

        memset(pContext, 0, sizeof(CONTEXT_ARM4));

        std::map<std::string, std::string> registers = pController->QueryAllRegisters(processorNumber);
        pContext->R0 = GdbSrvController::ParseRegisterValue32(registers["r0"]);
        pContext->R1 = GdbSrvController::ParseRegisterValue32(registers["r1"]);
        pContext->R2 = GdbSrvController::ParseRegisterValue32(registers["r2"]);
        pContext->R3 = GdbSrvController::ParseRegisterValue32(registers["r3"]);
        pContext->R4 = GdbSrvController::ParseRegisterValue32(registers["r4"]);
        pContext->R5 = GdbSrvController::ParseRegisterValue32(registers["r5"]);
        pContext->R6 = GdbSrvController::ParseRegisterValue32(registers["r6"]);
        pContext->R7 = GdbSrvController::ParseRegisterValue32(registers["r7"]);
        pContext->R8 = GdbSrvController::ParseRegisterValue32(registers["r8"]);
        pContext->R9  = GdbSrvController::ParseRegisterValue32(registers["r9"]);
        pContext->R10  = GdbSrvController::ParseRegisterValue32(registers["r10"]);
        pContext->R11 = GdbSrvController::ParseRegisterValue32(registers["r11"]);
        pContext->R12 = GdbSrvController::ParseRegisterValue32(registers["r12"]);
        pContext->Sp = GdbSrvController::ParseRegisterValue32(registers["sp"]);
        pContext->Lr = GdbSrvController::ParseRegisterValue32(registers["lr"]);
        pContext->Pc = GdbSrvController::ParseRegisterValue32(registers["pc"]);
        pContext->Psr = GdbSrvController::ParseRegisterValue32(registers["Cpsr"]);
        pContext->RegGroupSelection.fControlRegs = TRUE;
        pContext->RegGroupSelection.fIntegerRegs = TRUE;
        // Store the last 'pc' value in order to notify the engine with the last obtained 'pc' value, 
        // This is required for cases when the GdbServer responds with target unvailable packet.
        m_lastPcAddress = pContext->Pc;
        m_lastPSRvalue = pContext->Psr;

        //  Get Neon registers
        try
        {
            //  Get Neon registers, if possible
            GetNeonRegisters(pController, registers, pContext);
        }
        catch (...)
        {
            // ignore failure, and don't report Neon registers
            // (this occurs on QEMU, where is not defined the right register mappings)
        }

        if (pContext->RegGroupSelection.fFloatingPointRegs)
        {
            try
            {
                pContext->Fpscr = GdbSrvController::ParseRegisterValue32(registers["Fpscr"]);
            }
            catch (...)
            {
                // no fpscr was found in the returned context (e.g. on QEMU)
                // rather than failing outright, return the still-useful integer context
                pContext->RegGroupSelection.fFloatingPointRegs = FALSE;
            }
        }
        pContext->RegGroupSelection.fDebugRegs = FALSE;

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARM4 *pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
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
            m_lastPcAddress  = pContext->Pc;
            registers["Cpsr"] = pContext->Psr;
        }
        pController->SetRegisters(processorNumber, registers, false);
        if (pContext->RegGroupSelection.fFloatingPointRegs)
        {
            SetNeonRegisters(processorNumber, pContext, pController);
            registers["Fpscr"] = pContext->Fpscr;
        }

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

// ------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_X86_64 pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }

        memset(pContext, 0, sizeof(CONTEXT_X86_64));

        //We do not fetch the actual descriptors, thus we mark them as invalid
        pContext->DescriptorCs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorSs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorGs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorFs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorEs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorDs.SegFlags = static_cast<DWORD>(-1);

        std::map<std::string, std::string> registers = pController->QueryAllRegisters(processorNumber);
        pContext->Rax = GdbSrvController::ParseRegisterValue(registers["rax"]);
        pContext->Rbx = GdbSrvController::ParseRegisterValue(registers["rbx"]);
        pContext->Rcx = GdbSrvController::ParseRegisterValue(registers["rcx"]);
        pContext->Rdx = GdbSrvController::ParseRegisterValue(registers["rdx"]);
        pContext->Rsi = GdbSrvController::ParseRegisterValue(registers["rsi"]);
        pContext->Rdi = GdbSrvController::ParseRegisterValue(registers["rdi"]);
        pContext->Rip = GdbSrvController::ParseRegisterValue(registers["rip"]);
        // Store the last 'pc' value in order to notify the engine with the last obtained 'pc' value, 
        // This is required for cases when the GdbServer responds with target unvailable packet.
        m_lastPcAddress = pContext->Rip;
        pContext->Rsp = GdbSrvController::ParseRegisterValue(registers["rsp"]);
        pContext->Rbp = GdbSrvController::ParseRegisterValue(registers["rbp"]);
        pContext->R8  = GdbSrvController::ParseRegisterValue(registers["r8"]);
        pContext->R9  = GdbSrvController::ParseRegisterValue(registers["r9"]);
        pContext->R10 = GdbSrvController::ParseRegisterValue(registers["r10"]);
        pContext->R11 = GdbSrvController::ParseRegisterValue(registers["r11"]);
        pContext->R12 = GdbSrvController::ParseRegisterValue(registers["r12"]);
        pContext->R13 = GdbSrvController::ParseRegisterValue(registers["r13"]);
        pContext->R14 = GdbSrvController::ParseRegisterValue(registers["r14"]);
        pContext->R15 = GdbSrvController::ParseRegisterValue(registers["r15"]);
        pContext->EFlags = GdbSrvController::ParseRegisterValue32(registers["eflags"]);
        pContext->RegGroupSelection.fIntegerRegs = TRUE;

        pContext->ModeFlags = AMD64_CONTEXT_AMD64 | AMD64_CONTEXT_CONTROL | 
                              AMD64_CONTEXT_INTEGER | AMD64_CONTEXT_SEGMENTS;

        //  Segment registers
        pContext->SegCs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["cs"]));
        pContext->SegSs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["ss"]));
        pContext->SegDs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["ds"]));
        pContext->SegEs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["es"]));
        pContext->SegFs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["fs"]));
        pContext->SegGs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["gs"]));
        pContext->RegGroupSelection.fSegmentRegs = TRUE;

        //   Control registers (System registers)
        pContext->RegCr0 = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["cr0"]));
        pContext->RegCr2 = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["cr2"]));
        pContext->RegCr3 = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["cr3"]));
        pContext->RegCr4 = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["cr4"]));
        pContext->RegCr8 = static_cast<DWORD>(GdbSrvController::ParseRegisterValue(registers["cr8"]));
        pContext->RegGroupSelection.fSystemRegisters = TRUE;

        //  get all floating point registers (FPU)
        pContext->ControlWord = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["fctrl"]));
        pContext->StatusWord = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["fstat"]));
        pContext->TagWord = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["ftag"]));
        pContext->ErrorOffset = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["fioff"]));
        pContext->ErrorSelector = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["fiseg"]));
        pContext->DataOffset = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["fooff"]));
        pContext->DataSelector = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["foseg"]));

        //  x87 registers (FPU)
        for (int index = 0; index < s_numberFPRegList; ++index)
        {
            std::string regName(s_fpRegList[index]);
            GdbSrvController::ParseRegisterVariableSize(registers[regName],
                reinterpret_cast<BYTE*>(&pContext->RegisterArea[index * s_numberOfBytesCoprocessorRegister]),
                s_numberOfBytesCoprocessorRegister);
        }
        pContext->RegGroupSelection.fFloatingPointRegs = TRUE;

        //  Get X64 SSE registers if the x64 SSE context enabled?
        if (m_fEnableSSEContext)
        {
            registers = pController->QueryRegisters(processorNumber, s_sseX64RegList, s_numberOfSseX64Registers);
            const int numberOfBytesSseX64Registers = sizeof(pContext->RegSSE[0]);
            for (int index = 0; index < s_numberOfSseX64Registers; ++index)
            {
                std::string regName(s_sseX64RegList[index]);
                GdbSrvController::ParseRegisterVariableSize(registers[regName],
                    reinterpret_cast<BYTE*>(&pContext->RegSSE[index]),
                    numberOfBytesSseX64Registers);
            }
            pContext->RegMXCSR = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["mxcsr"]));
            pContext->RegGroupSelection.fSSERegisters = TRUE;
        }

        pContext->RegGroupSelection.fSegmentDescriptors = FALSE;
        pContext->RegGroupSelection.fDebugRegs = FALSE;

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_X86_64 *pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
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
            m_lastPcAddress  = pContext->Rip;
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
            registers["eflags"] = pContext->EFlags;
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

        if (pContext->RegGroupSelection.fSystemRegisters)
        {
            registers["fctrl"] = pContext->ControlWord;
            registers["fstat"] = pContext->StatusWord;
            registers["ftag"] = pContext->TagWord;
            registers["fioff"] = pContext->ErrorOffset;
            registers["fiseg"] = pContext->ErrorSelector;
            registers["fooff"] = pContext->DataOffset;
            registers["foseg"] = pContext->DataSelector;
        }

        //  Control registers
        if (pContext->RegGroupSelection.fSystemRegisters)
        {
            //  Control registers (System registers)
            registers["cr0"] = pContext->RegCr0;
            registers["cr2"] = pContext->RegCr2;
            registers["cr3"] = pContext->RegCr3;
            registers["cr4"] = pContext->RegCr4;
            registers["cr8"] = pContext->RegCr8;
        }
        pController->SetRegisters(processorNumber, registers, false);

        //  Floating point registers
        if (pContext->RegGroupSelection.fFloatingPointRegs)
        {
            for (int index = 0; index < s_numberFPRegList; ++index)
            {
                std::string regName(s_fpRegList[index]);
                registers[regName] = reinterpret_cast<ULONGLONG>(&pContext->RegisterArea[index * s_numberOfBytesCoprocessorRegister]);
            }
            pController->SetRegisters(processorNumber, registers, true);
        }

        //  SSE x64 registers
        if (m_fEnableSSEContext)
        {
            for (int index = 0; index < s_numberOfSseX64Registers; ++index)
            {
                std::string regName(s_sseX64RegList[index]);
                registers[regName] = reinterpret_cast<ULONGLONG>(&pContext->RegSSE[index]);
            }
            pController->SetRegisters(processorNumber, registers, true);
        }

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

// ------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_X86_EX pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }

        memset(pContext, 0, sizeof(CONTEXT_X86_EX));

        pContext->DescriptorCs.Flags = static_cast<DWORD>(X86_DESC_FLAGS);
        pContext->DescriptorSs.Flags = static_cast<DWORD>(X86_DESC_FLAGS);
        pContext->DescriptorGs.Flags = static_cast<DWORD>(X86_DESC_FLAGS);
        pContext->DescriptorFs.Flags = static_cast<DWORD>(X86_DESC_FLAGS);
        pContext->DescriptorEs.Flags = static_cast<DWORD>(X86_DESC_FLAGS);
        pContext->DescriptorDs.Flags = static_cast<DWORD>(X86_DESC_FLAGS);

        std::map<std::string, std::string> registers = pController->QueryAllRegisters(processorNumber);
        //  Get core integer registers
        GetX86CoreRegisters(registers, pContext);
        //  Get the 80387 Copreocessor registers
        GetFPCoprocessorRegisters(registers, processorNumber, pController, reinterpret_cast<PVOID>(pContext));
        //  Is the SSE context enabled?
        if (m_fEnableSSEContext)
        {
            //  Get the SSE registers
            GetSSERegisters(processorNumber, pController, pContext);
        }

        pContext->RegGroupSelection.fDebugRegs = FALSE;
        pContext->RegGroupSelection.fSystemRegisters = FALSE;
        pContext->RegGroupSelection.fSegmentDescriptors = FALSE;

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_X86_EX *pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }

        SetX86CoreRegisters(processorNumber, pContext, pController);

        SetFPCoprocessorRegisters(processorNumber, reinterpret_cast<const VOID *>(pContext), pController);

        //  Is the SSE context enabled?
        if (m_fEnableSSEContext)
        {
            SetSSERegisters(processorNumber, reinterpret_cast<const VOID *>(pContext), pController);
        }

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_ARMV8ARCH64 pContext)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        pController->StopTargetAtRun();
        memset(pContext, 0, sizeof(CONTEXT_ARMV8ARCH64));

        std::map<std::string, std::string> registers = pController->QueryAllRegisters(processorNumber);

        for (int i = 0; i < ARMV8ARCH64_MAX_INTERGER_REGISTERS; ++i)
        {
            char registerNameStr[4] = {0};
            sprintf_s(registerNameStr, _countof(registerNameStr), "X%d", i);
            std::string registerName(registerNameStr);
            pContext->X[i] = GdbSrvController::ParseRegisterValue(registers[registerName]);
        }
        pContext->Fp = GdbSrvController::ParseRegisterValue(registers["fp"]);
        pContext->Lr = GdbSrvController::ParseRegisterValue(registers["lr"]);
        pContext->Sp = GdbSrvController::ParseRegisterValue(registers["sp"]);
        pContext->Pc = GdbSrvController::ParseRegisterValue(registers["pc"]);
        pContext->Psr = GdbSrvController::ParseRegisterValue(registers["cpsr"]);
        m_lastPcAddress = pContext->Pc;
        m_lastPSRvalue = pContext->Psr;

        pContext->RegGroupSelection.fControlRegs = TRUE;
        pContext->RegGroupSelection.fIntegerRegs = TRUE;
        pContext->RegGroupSelection.fFloatingPointRegs = FALSE;
        pContext->RegGroupSelection.fDebugRegs = FALSE;

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARMV8ARCH64 *pContext)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        pController->StopTargetAtRun();

        std::map<std::string, ULONGLONG> registers;
        if (pContext->RegGroupSelection.fIntegerRegs)
        {
            for (int i = 0; i < ARMV8ARCH64_MAX_INTERGER_REGISTERS; ++i)
            {
                char registerNameStr[4] = {0};
                sprintf_s(registerNameStr, _countof(registerNameStr), "X%d", i);
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
            registers["cpsr"] = pContext->Psr;
            m_lastPcAddress  = pContext->Pc;
        }
        pController->SetRegisters(processorNumber, registers, false);

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

ADDRESS_TYPE CLiveExdiGdbSrvSampleServer::GetCurrentExecutionAddress(_Out_ DWORD *pProcessorNumberOfLastEvent)
{
    assert(pProcessorNumberOfLastEvent != nullptr);
    AsynchronousGdbSrvController * pController = GetGdbSrvController();
    assert(pController != nullptr);

    *pProcessorNumberOfLastEvent = pController->GetLastKnownActiveCpu();
    ADDRESS_TYPE result;
    std::map<std::string, std::string> registers = pController->QueryAllRegisters(*pProcessorNumberOfLastEvent);

    if (m_detectedProcessorFamily == PROCESSOR_FAMILY_ARM || m_detectedProcessorFamily == PROCESSOR_FAMILY_ARMV8ARCH64)
    {
        result = GdbSrvController::ParseRegisterValue(registers["pc"]);
    }
    else if (m_detectedProcessorFamily == PROCESSOR_FAMILY_X86)
    {
        if (m_targetProcessorArch == X86_ARCH)
        {
            result = GdbSrvController::ParseRegisterValue(registers["Eip"]);
        }
        else
        {
            result = GdbSrvController::ParseRegisterValue(registers["rip"]);
        }
    }
    else
    {
        throw std::exception("Unknown CPU architecture. Please add support for it");
    }
    m_lastPcAddress = result;
    return result;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::SetKeepaliveInterface(/* [in] */ IeXdiKeepaliveInterface3 *pKeepalive)
{
    m_pKeepaliveInterface = pKeepalive;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::OnAsynchronousCommandCompleted()
{
    if (m_pRunNotificationListener != nullptr)
    {
        HALT_REASON_TYPE haltReason = hrUnknown;

        DWORD eventProcessor = 0;
        ADDRESS_TYPE currentAddress = ParseAsynchronousCommandResult(&eventProcessor, &haltReason);
        if (m_lastResumingCommandWasStep)
        {
            haltReason = hrStep;
        }

        m_targetIsRunning = false;
        if (currentAddress != 0)
        {
            m_pRunNotificationListener->NotifyRunStateChange(rsHalted, haltReason, currentAddress, 0, eventProcessor);
            return S_OK; 
        }
    }
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::PerformKeepaliveChecks(void)
{
    if (m_pKeepaliveInterface == nullptr)
    {
        return S_FALSE;
    }

    AsynchronousGdbSrvController * pController = GetGdbSrvController();
    if (pController == nullptr)
    {
        return E_POINTER;
    }

    // Get the GdbServer connection status
    bool isGdbServerDown = false;
    HRESULT gdbServerError = S_OK;;
    if (pController->CheckGdbSrvAlive(gdbServerError))
    {
        if (gdbServerError == ERROR_OPERATION_ABORTED)
        {
            //  Close the connection with the GdbServer
            pController->ShutdownGdbSrv();
            isGdbServerDown = true;
        }
    }

    HRESULT result = m_pKeepaliveInterface->IsDebugSessionAlive();
    if (FAILED(result) || isGdbServerDown)
    {
        TCHAR fileName[MAX_PATH];
        bool lostConnection = ((gdbServerError == ERROR_OPERATION_ABORTED) || HRESULT_FACILITY(result) == FACILITY_WIN32 && 
                              ((HRESULT_CODE(result) == RPC_S_CALL_FAILED) || (HRESULT_CODE(result) == RPC_S_SERVER_UNAVAILABLE)));

        if (lostConnection && GetModuleFileName(GetModuleHandle(0), fileName, _countof(fileName)))
        {
            TCHAR *pLastSlash = _tcsrchr(fileName, '\\');
            if (pLastSlash && !_tcsicmp(pLastSlash + 1, _T("dllhost.exe")))
            {
                ExitProcess(result);
            }
        }
    }
    return S_OK;
}

HRESULT CLiveExdiGdbSrvSampleServer::SetGdbServerParameters()
{
    try
    {
        TCHAR configXmlFile[MAX_PATH + 1];
        DWORD fileNameLength = GetEnvironmentVariable(_T("EXDI_GDBSRV_XML_CONFIG_FILE"), 
                                                      configXmlFile, _countof(configXmlFile));
        if (fileNameLength == 0)
        {
            MessageBox(0, _T("Error: the EXDI_GDBSRV_XML_CONFIG_FILE environment variable is not defined.\n")
                          _T("The Exdi-GdbServer sample won't continue at this point.\n")
                          _T("Please set the full path to the Exdi xml configuration file."), _T("EXDI-GdbServer"), MB_ICONERROR);
            return E_ABORT;
        }

        ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(configXmlFile);
        m_targetProcessorArch = cfgData.GetTargetArchitecture();
        m_detectedProcessorFamily = cfgData.GetTargetFamily();
        m_fDisplayCommData = cfgData.GetDisplayCommPacketsCharacters();
        m_fEnableSSEContext = cfgData.GetIntelSseContext();
        m_heuristicChunkSize = cfgData.GetHeuristicScanMemorySize();
        unsigned numberOfCores = cfgData.GetNumberOfCores();
        std::vector<std::wstring> coreConnections;
        cfgData.GetGdbServerConnectionParameters(coreConnections);
        if (coreConnections.size() != numberOfCores)
        {
            MessageBox(0, _T("Error: the number of cores does not match with the number of connection strings in the configuration xml file."),
                       _T("EXDI-GdbServer"), MB_ICONERROR);
            return E_ABORT;
        }

        m_pGdbSrvController = AsynchronousGdbSrvController::Create(coreConnections);
        m_pGdbSrvController->SetTargetArchitecture(m_targetProcessorArch);
        m_pGdbSrvController->SetTargetProcessorFamilyByTargetArch(m_targetProcessorArch);
        if (m_fDisplayCommData)
        {
            m_pGdbSrvController->SetTextHandler(new CommandLogger(true));
        }

        WCHAR systemRegMapXmlFile[MAX_PATH + 1];
        fileNameLength = GetEnvironmentVariable(_T("EXDI_SYSTEM_REGISTERS_MAP_XML_FILE"),
            systemRegMapXmlFile, _countof(systemRegMapXmlFile));
        if (fileNameLength != 0)
        {
            m_pGdbSrvController->SetSystemRegisterXmlFile(systemRegMapXmlFile);
        }
        else
        {
            MessageBox(0, _T("Error: the EXDI_SYSTEM_REGISTERS_MAP_XML_FILE environment variable is not defined.\n")
                _T("rdmsr/wrmsr functions won't work at this point.\n")
                _T("Please set the full path to the SYSTEMREGISTERS.XML file."), _T("EXDI-GdbServer"), MB_ICONERROR);
        }

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

DWORD CALLBACK CLiveExdiGdbSrvSampleServer::NotificationThreadBody(LPVOID p)
{
    CLiveExdiGdbSrvSampleServer * pServer = reinterpret_cast<CLiveExdiGdbSrvSampleServer *>(p);
    AsynchronousGdbSrvController * pController = pServer->GetGdbSrvController();
    assert(pController != nullptr);

    HRESULT result = CoInitialize(nullptr);
    assert(SUCCEEDED(result));
    UNREFERENCED_PARAMETER(result);

    ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
    DWORD waitTimeout = (cfgData.GetMultiCoreGdbServer()) ? INFINITE : 6000;  

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
                if (pController->GetAsynchronousCommandResult(waitTimeout, nullptr))
                {
                    pReceiver->OnAsynchronousCommandCompleted();
                }
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

VOID CALLBACK CLiveExdiGdbSrvSampleServer::SampleTimerCallback(_In_  HWND hwnd, _In_  UINT uMsg, _In_  UINT_PTR idEvent, _In_  DWORD dwTime)
{
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(idEvent);
    UNREFERENCED_PARAMETER(dwTime);
    //  If your JTAG hardware supports polling mode rather than asynchronous notification mode, use this
    //  method to poll whether the target has stopped on an event and send a notification to debugging engine
    //  by calling m_pRunNotificationListener->NotifyRunStateChange().
}

HRESULT CLiveExdiGdbSrvSampleServer::SetGdbServerConnection(void)
{
    try
    {
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }

        //  Configure the GdbServer communication session.
        if (!pController->ConfigureGdbSrvCommSession(m_fDisplayCommData, C_ALLCORES))
        {
            //  Failed configuring the session.
            MessageBox(0, _T("Error: Unable to configure the GdbServer session."), _T("EXDI-GdbServer"), MB_ICONERROR);
            return E_ABORT;
        }

        //  Execute the connection to the GdbServer
        if (!pController->ConnectGdbSrv())
        {
            //  Failed connecting to the GdbServer.
            MessageBox(0, _T("Error: Unable to establish a connection with the GdbServer.")
                          _T("Please verify the connection string <hostname/ip>:portnumber."),
                          _T("EXDI-GdbServer"), MB_ICONERROR);
            return E_ABORT;
        }

        HRESULT result = E_FAIL;

        //  Establish the handshaking with the GdbServer.
        //  Request the set of features supported by the GdbServer
        if (pController->ReqGdbServerSupportedFeatures())
        {
            //  Ensure that the target architecture matches with the current GDB server
            m_targetProcessorArch = m_pGdbSrvController->GetTargetArchitecture();
            m_detectedProcessorFamily = m_pGdbSrvController->GetProcessorFamilyArchitecture();
            //  Is the target broken because the GdbServer sends a break request?
            if (pController->IsTargetHalted())
            {
                result = S_OK;
            }
        }
        return result;
    }
    CATCH_AND_RETURN_HRESULT;
}

ADDRESS_TYPE CLiveExdiGdbSrvSampleServer::ParseAsynchronousCommandResult(_Out_ DWORD * pProcessorNumberOfLastEvent, _Out_ HALT_REASON_TYPE * pHaltReason)
{
    assert(pProcessorNumberOfLastEvent != nullptr);
    AsynchronousGdbSrvController * pController = GetGdbSrvController();
    assert(pController != nullptr);

    ADDRESS_TYPE currentPcAddress = 0;
    if (pController->GetAsynchronousCmdStopReplyPacket())
    {
        int attempts = 0;
        bool isWaitingOnStopReply = false;
        ULONG totalPackets = 0;
        do
        {
            StopReplyPacketStruct stopReply;
            const std::string reply = pController->GetCommandResult();
            bool isParsed = pController->HandleAsynchronousCommandResponse(reply, &stopReply);
            if (isParsed)
            {
                attempts = 0;
                //  Is it a OXX console packet?
                if (stopReply.status.isOXXPacket)
                {
                    //  Try to display the GDB server ouput message if there is an attached text console.
                    pController->DisplayConsoleMessage(reply);
                    //  Post another receive request on the packet buffer
                    pController->ContinueWaitingOnStopReplyPacket();
                    isWaitingOnStopReply = true;
                }
                //  Is it a T packet?
                else if (stopReply.status.isTAAPacket)
                {
                    if (stopReply.status.isPcRegFound)
                    {
                        assert(stopReply.currentAddress != 0);
                        currentPcAddress = m_lastPcAddress = stopReply.currentAddress;
                    }
                    else
                    {
                        //  The packet didn't contain the PC, but we'd better find out what it is, so we can inform the debugger
                        DWORD pcAddressRequest;
                        currentPcAddress = m_lastPcAddress = GetCurrentExecutionAddress(&pcAddressRequest);
                    }

                    if (stopReply.status.isThreadFound)
                    {
                        assert(stopReply.processorNumber != static_cast<ULONG>(-1));
                        if (stopReply.processorNumber <= pController->GetProcessorCount())
                        {
                            *pProcessorNumberOfLastEvent = stopReply.processorNumber;
                        }
                    }
                    else
                    {
                        *pProcessorNumberOfLastEvent = pController->GetLastKnownActiveCpu();
                    }
                    isWaitingOnStopReply = false;
                }
                //  Is it a S AA packet?
                else if (stopReply.status.isSAAPacket)
                {
                    //  There is a no any processor number or pc adddress in the response
                    if (stopReply.status.isPowerDown)
                    {
                        MessageBox(0, _T("The Target is running or it is in a power down state."),nullptr, MB_ICONERROR);                    
                    }
                    stopReply.currentAddress = m_lastPcAddress;
                    *pProcessorNumberOfLastEvent = pController->GetLastKnownActiveCpu();
                    isWaitingOnStopReply = false;
                }

                if (!isWaitingOnStopReply)
                {
                    //  Convert the stop reason code
                    switch (stopReply.stopReason) 
                    {
                    case TARGET_BREAK_SIGINT:
                       *pHaltReason = hrUser;
                       break;
                    case TARGET_BREAK_SIGTRAP:
                       *pHaltReason = hrBp;
                       break;
                    default:
                       *pHaltReason = hrUnknown;
                    }
                    pController->ResetAsynchronousCmdStopReplyPacket();
                }
            }
            else
            {
                Sleep(c_asyncResponsePauseMs);
            }
        }
        while (isWaitingOnStopReply &&
            (attempts++ < c_attemptsWaitingOnPendingResponse) &&
            (totalPackets < c_maximumReplyPacketsInResponse));
    }
    else
    {
        //  This can happen only if there was a previously handled Halt event.
        currentPcAddress = m_lastPcAddress;
    }
    return currentPcAddress;
}

void CLiveExdiGdbSrvSampleServer::GetX86CoreRegisters(_In_ std::map<std::string, std::string> &registers, 
                                                      _Out_ CONTEXT_X86_EX * pContext)
{
    assert(pContext != nullptr);

    pContext->Eax = GdbSrvController::ParseRegisterValue32(registers["Eax"]);
    pContext->Ebx = GdbSrvController::ParseRegisterValue32(registers["Ebx"]);
    pContext->Ecx = GdbSrvController::ParseRegisterValue32(registers["Ecx"]);
    pContext->Edx = GdbSrvController::ParseRegisterValue32(registers["Edx"]);
    pContext->Esi = GdbSrvController::ParseRegisterValue32(registers["Esi"]);
    pContext->Edi = GdbSrvController::ParseRegisterValue32(registers["Edi"]);
    pContext->Eip = GdbSrvController::ParseRegisterValue32(registers["Eip"]);
    m_lastPcAddress = pContext->Eip;
    pContext->Esp = GdbSrvController::ParseRegisterValue32(registers["Esp"]);
    pContext->Ebp = GdbSrvController::ParseRegisterValue32(registers["Ebp"]);
    pContext->EFlags = GdbSrvController::ParseRegisterValue32(registers["EFlags"]);
    pContext->RegGroupSelection.fIntegerRegs = TRUE;

    pContext->SegCs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["SegCs"]));
    pContext->SegSs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["SegSs"]));
    pContext->RegGroupSelection.fControlRegs = TRUE;

    pContext->SegDs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["SegDs"]));
    pContext->SegEs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["SegEs"]));
    pContext->SegFs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["SegFs"]));
    pContext->SegGs = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["SegGs"]));
    pContext->RegGroupSelection.fSegmentRegs = TRUE;
}

void CLiveExdiGdbSrvSampleServer::GetFPCoprocessorRegisters(_In_ std::map<std::string, std::string> &registers,
                                                            _In_ DWORD processorNumber, 
                                                            _In_ AsynchronousGdbSrvController * const pController,
                                                            _Out_ PVOID pContext)
{
    assert(pContext != nullptr);

    PCONTEXT_X86_EX pContextFP = reinterpret_cast<PCONTEXT_X86_EX>(pContext);

    pContextFP->ControlWord = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["ControlWord"]));
    pContextFP->StatusWord = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["StatusWord"]));
    pContextFP->TagWord = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["TagWord"]));
    pContextFP->ErrorOffset = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["ErrorOffset"]));
    pContextFP->ErrorSelector = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["ErrorSelector"]));
    pContextFP->DataOffset = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["DataOffset"]));
    pContextFP->DataSelector = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(registers["DataSelector"]));

    for (int index = 0; index < s_numberFPRegList; ++index)
    {
        std::string regName(s_fpRegList[index]);
        GdbSrvController::ParseRegisterVariableSize(registers[regName], 
                                                    reinterpret_cast<BYTE *>(&pContextFP->RegisterArea[index * s_numberOfBytesCoprocessorRegister]), 
                                                    s_numberOfBytesCoprocessorRegister);
    }

    const char * fpNpxStateRegister[] = {"Cr0NpxState"};
    std::map<std::string, std::string> fpNpxStateRegValue = pController->QueryRegisters(processorNumber, fpNpxStateRegister, ARRAYSIZE(fpNpxStateRegister));
    pContextFP->Cr0NpxState = static_cast<DWORD>(GdbSrvController::ParseRegisterValue32(fpNpxStateRegValue[fpNpxStateRegister[0]]));

    pContextFP->RegGroupSelection.fFloatingPointRegs = TRUE;
}

void CLiveExdiGdbSrvSampleServer::GetSSERegisters(_In_ DWORD processorNumber,
                                                  _In_ AsynchronousGdbSrvController * const pController, 
                                                  _Out_ PVOID pContext)
{
    assert(pController != nullptr && pContext != nullptr);
    
    std::map<std::string, std::string> registers = pController->QueryRegisters(processorNumber, s_sseRegList, s_numberOfSseRegisters);

    PCONTEXT_X86_EX pContextSSE = reinterpret_cast<PCONTEXT_X86_EX>(pContext);
    const int numberOfBytesSseRegisters = sizeof(pContextSSE->Sse[0]);

    for (int index = 0; index < s_numberOfSseRegisters; ++index)
    {
        std::string regName(s_sseRegList[index]);
        GdbSrvController::ParseRegisterVariableSize(registers[regName], 
                                                    reinterpret_cast<BYTE *>(&pContextSSE->Sse[index]), 
                                                    numberOfBytesSseRegisters);
    }
    pContextSSE->RegGroupSelection.fSSERegisters = TRUE;
}

void CLiveExdiGdbSrvSampleServer::SetX86CoreRegisters(_In_ DWORD processorNumber, _In_ const CONTEXT_X86_EX * pContext,
                                                      _In_ AsynchronousGdbSrvController * const pController)
{
    assert(pContext != nullptr && pController != nullptr);

    std::map<std::string, ULONGLONG> registers;

    if (pContext->RegGroupSelection.fIntegerRegs)
    {
        registers["Eax"] = pContext->Eax;
        registers["Ebx"] = pContext->Ebx;
        registers["Ecx"] = pContext->Ecx;
        registers["Edx"] = pContext->Edx;
        registers["Esi"] = pContext->Esi;
        registers["Edi"] = pContext->Edi;
        registers["Eip"] = pContext->Eip;
        m_lastPcAddress  = pContext->Eip;
        registers["Esp"] = pContext->Esp;
        registers["Ebp"] = pContext->Ebp;
    }

    if (pContext->RegGroupSelection.fSegmentRegs)
    {
        registers["SegCs"] = pContext->SegCs;
        registers["SegSs"] = pContext->SegSs;
        registers["SegDs"] = pContext->SegDs;
        registers["SegEs"] = pContext->SegEs;
        registers["SegFs"] = pContext->SegFs;
        registers["SegGs"] = pContext->SegGs;
    }

    if (pContext->RegGroupSelection.fFloatingPointRegs)
    {
        registers["ControlWord"] = pContext->ControlWord;
        registers["StatusWord"] = pContext->StatusWord;
        registers["TagWord"] = pContext->TagWord;
        registers["ErrorOffset"] = pContext->ErrorOffset;
        registers["ErrorSelector"] = pContext->ErrorSelector;
        registers["DataOffset"] = pContext->DataOffset;
        registers["DataSelector"] = pContext->DataSelector;
        registers["Cr0NpxState"] = pContext->Cr0NpxState;
    }

    pController->SetRegisters(processorNumber, registers, false);
}

void CLiveExdiGdbSrvSampleServer::SetFPCoprocessorRegisters(_In_ DWORD processorNumber, _In_ const VOID * pContext,
                                                            _In_ AsynchronousGdbSrvController * const pController)
{
    assert(pContext != nullptr && pController != nullptr);

    const CONTEXT_X86_EX * pContextFP = reinterpret_cast<const CONTEXT_X86_EX *>(pContext);
    if (pContextFP->RegGroupSelection.fFloatingPointRegs)
    {
        std::map<std::string, ULONGLONG> registers;

        for (int index = 0; index < s_numberFPRegList; ++index)
        {
            std::string regName(s_fpRegList[index]);
            registers[regName] = reinterpret_cast<ULONGLONG>(&pContextFP->RegisterArea[index * s_numberOfBytesCoprocessorRegister]);
        }
        pController->SetRegisters(processorNumber, registers, true);
    }
}

void CLiveExdiGdbSrvSampleServer::SetSSERegisters(_In_ DWORD processorNumber, _In_ const VOID * pContext,
                                                  _In_ AsynchronousGdbSrvController * const pController)
{
    assert(pContext != nullptr && pController != nullptr);

    const CONTEXT_X86_EX * pContextSse = reinterpret_cast<const CONTEXT_X86_EX *>(pContext);
    if (pContextSse->RegGroupSelection.fSSERegisters)
    {
        std::map<std::string, ULONGLONG> registers;

        for (int index = 0; index < s_numberOfSseRegisters; ++index)
        {
            std::string regName(s_sseRegList[index]);
            registers[regName] = reinterpret_cast<ULONGLONG>(&pContextSse->Sse[index]);
        }
        pController->SetRegisters(processorNumber, registers, true);
    }
}

void CLiveExdiGdbSrvSampleServer::GetNeonRegisters(_In_ AsynchronousGdbSrvController * const pController,
                                                   _In_ std::map<std::string, std::string> &registers,
                                                   _Out_ PVOID pContext)
{
    assert(pController != nullptr && pContext != nullptr);
    
    std::unique_ptr<char> neonNameRegArray[EXDI_ARM_MAX_NEON_FP_REGISTERS];    
    std::string firstNeonRegister("d0");
    pController->CreateNeonRegisterNameArray(firstNeonRegister, neonNameRegArray, EXDI_ARM_MAX_NEON_FP_REGISTERS);

    PCONTEXT_ARM4 pContextArm = reinterpret_cast<PCONTEXT_ARM4>(pContext);
    const int numberOfBytesNeonRegisters = sizeof(pContextArm->D[0]);
    for (size_t index = 0; index < EXDI_ARM_MAX_NEON_FP_REGISTERS; ++index)
    {
        std::string regName(neonNameRegArray[index].get());
        GdbSrvController::ParseRegisterVariableSize(registers[regName], 
                                                    reinterpret_cast<BYTE *>(&pContextArm->D[index]), 
                                                    numberOfBytesNeonRegisters);
    }
    pContextArm->RegGroupSelection.fFloatingPointRegs = TRUE;
}

void CLiveExdiGdbSrvSampleServer::SetNeonRegisters(_In_ DWORD processorNumber, _In_ const VOID * pContext,
                                                   _In_ AsynchronousGdbSrvController * const pController)
{
    assert(pContext != nullptr && pController != nullptr);

    std::unique_ptr<char> neonNameRegArray[EXDI_ARM_MAX_NEON_FP_REGISTERS];    
    std::string firstNeonRegister("d0");
    pController->CreateNeonRegisterNameArray(firstNeonRegister, neonNameRegArray, EXDI_ARM_MAX_NEON_FP_REGISTERS);

    const CONTEXT_ARM4 * pContextArm = reinterpret_cast<const CONTEXT_ARM4 *>(pContext);
    if (pContextArm->RegGroupSelection.fFloatingPointRegs)
    {
        std::map<std::string, ULONGLONG> registers;
        for (int index = 0; index < EXDI_ARM_MAX_NEON_FP_REGISTERS; ++index)
        {
            std::string regName(neonNameRegArray[index].get());
            registers[regName] = reinterpret_cast<ULONGLONG>(&pContextArm->D[index]);
        }
        pController->SetRegisters(processorNumber, registers, true);
    }
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::ExecuteExdiComponentFunction( 
    /* [in] */ ExdiComponentFunctionType type,
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ LPCWSTR pFunctionToExecute)
{
    try
    {
        if (pFunctionToExecute == nullptr)
        {
            return E_POINTER;
        }
        if (type != exdiComponentSession)
        {
            return E_INVALIDARG;
        }
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        if (!pController->ExecuteExdiFunction(dwProcessorNumber, pFunctionToExecute))
        {
            return E_FAIL;
        }
        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CLiveExdiGdbSrvSampleServer::ExecuteTargetEntityFunction( 
    /* [in] */ ExdiComponentFunctionType type,
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ LPCWSTR pFunctionToExecute,
    /* [out] */ SAFEARRAY ** pFunctionResponseBuffer)
{
    try
    {
        if (pFunctionToExecute == nullptr)
        {
            return E_POINTER;
        }
        if (type != exdiTargetEntity || dwProcessorNumber == C_ALLCORES)
        {
            return E_INVALIDARG;
        }
        AsynchronousGdbSrvController * pController = GetGdbSrvController();
        if (pController == nullptr)
        {
            return E_POINTER;
        }
        SimpleCharBuffer buffer = pController->ExecuteExdiGdbSrvMonitor(dwProcessorNumber, pFunctionToExecute);
        return SafeArrayFromByteArray(buffer.GetInternalBuffer(), buffer.GetLength(), pFunctionResponseBuffer);
    }
    CATCH_AND_RETURN_HRESULT;
}
