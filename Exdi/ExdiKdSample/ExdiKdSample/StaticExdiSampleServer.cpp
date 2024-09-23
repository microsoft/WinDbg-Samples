//----------------------------------------------------------------------------
//
// StaticExdiSampleServer.cpp
//
// A sample EXDI server class demonstrating basic functionality.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "StaticExdiSampleServer.h"
#include "ComHelpers.h"
#include "AsynchronousKDController.h"
#include "KDCommandLogger.h"
#include "ArgumentHelpers.h"
#include "ExceptionHelpers.h"
#include "dbgeng_exdi_io.h"

#define METHOD_NOT_IMPLEMENTED if (IsDebuggerPresent()) \
                                   __debugbreak(); \
                               return E_NOTIMPL

using namespace KDControllerLib;

// CStaticExdiSampleServer

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetTargetInfo( 
    /* [out] */ PGLOBAL_TARGET_INFO_STRUCT pgti)
{
    CheckAndZeroOutArgs(pgti);

    pgti->TargetProcessorFamily = m_detectedProcessorFamily;
    //TODO: handle out-of-memory condition
    pgti->szProbeName = COMHelpers::CopyStringToTaskMem(L"ExdiSample");
    pgti->szTargetName= COMHelpers::CopyStringToTaskMem(L"ExdiSample Target");
    memset(&pgti->dbc, 0, sizeof(pgti->dbc));
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetRunStatus( 
    /* [out] */ PRUN_STATUS_TYPE persCurrent,
    /* [out] */ PHALT_REASON_TYPE pehrCurrent,
    /* [out] */ ADDRESS_TYPE *pCurrentExecAddress,
    /* [out] */ DWORD *pdwExceptionCode,
    /* [out] */ DWORD *pdwProcessorNumberOfLastEvent)
{
    try
    {
        CheckAndZeroOutArgs(persCurrent, pehrCurrent, pCurrentExecAddress, pdwExceptionCode, pdwProcessorNumberOfLastEvent);

        *persCurrent = rsHalted;
        *pehrCurrent = hrUser;

        *pCurrentExecAddress = GetCurrentExecutionAddress(pdwProcessorNumberOfLastEvent);
	    *pdwExceptionCode = 0;

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::Run( void)
{
	METHOD_NOT_IMPLEMENTED;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::Halt( void)
{
	METHOD_NOT_IMPLEMENTED;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::DoSingleStep(DWORD dwProcessorNumber)
{
	UNREFERENCED_PARAMETER(dwProcessorNumber);
	METHOD_NOT_IMPLEMENTED;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::Reboot( void)
{
	METHOD_NOT_IMPLEMENTED;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetNbCodeBpAvail( 
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

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetNbDataBpAvail( 
    /* [out] */ DWORD *pdwNbDataBpAvail)
{
    if (pdwNbDataBpAvail == nullptr)
    {
        return E_POINTER;
    }

    *pdwNbDataBpAvail = 0;
    return S_OK;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::AddCodeBreakpoint( 
    /* [in] */ ADDRESS_TYPE Address,
    /* [in] */ CBP_KIND cbpk,
    /* [in] */ MEM_TYPE mt,
    /* [in] */ DWORD dwExecMode,
    /* [in] */ DWORD dwTotalBypassCount,
    /* [out] */ IeXdiCodeBreakpoint3 **ppieXdiCodeBreakpoint)
{
	UNREFERENCED_PARAMETER(Address);
	UNREFERENCED_PARAMETER(cbpk);
	UNREFERENCED_PARAMETER(mt);
	UNREFERENCED_PARAMETER(dwExecMode);
	UNREFERENCED_PARAMETER(dwTotalBypassCount);
	UNREFERENCED_PARAMETER(ppieXdiCodeBreakpoint);
	METHOD_NOT_IMPLEMENTED;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::DelCodeBreakpoint( 
    /* [in] */ IeXdiCodeBreakpoint3 *pieXdiCodeBreakpoint)
{
	UNREFERENCED_PARAMETER(pieXdiCodeBreakpoint);
	METHOD_NOT_IMPLEMENTED;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::AddDataBreakpoint( 
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
	UNREFERENCED_PARAMETER(Address);
	UNREFERENCED_PARAMETER(AddressMask);
	UNREFERENCED_PARAMETER(dwData);
	UNREFERENCED_PARAMETER(dwDataMask);
	UNREFERENCED_PARAMETER(bAccessWidth);
	UNREFERENCED_PARAMETER(mt);
	UNREFERENCED_PARAMETER(bAddressSpace);
	UNREFERENCED_PARAMETER(da);
	UNREFERENCED_PARAMETER(dwTotalBypassCount);
	UNREFERENCED_PARAMETER(ppieXdiDataBreakpoint);
	METHOD_NOT_IMPLEMENTED;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::DelDataBreakpoint( 
    /* [in] */ IeXdiDataBreakpoint3 *pieXdiDataBreakpoint)
{
	UNREFERENCED_PARAMETER(pieXdiDataBreakpoint);
	METHOD_NOT_IMPLEMENTED;
}
        
       
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::StartNotifyingRunChg( 
    /* [in] */ IeXdiClientNotifyRunChg3 *pieXdiClientNotifyRunChg,
    /* [out] */ DWORD *pdwConnectionCookie)
{
	if (pieXdiClientNotifyRunChg == nullptr)
	{
		return E_POINTER;
	}

    try
    {
        CheckAndZeroOutArgs(pdwConnectionCookie);
        *pdwConnectionCookie = 1;
	    return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::StopNotifyingRunChg( 
    /* [in] */ DWORD dwConnectionCookie)
{
	UNREFERENCED_PARAMETER(dwConnectionCookie);
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
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::ReadVirtualMemory( 
    /* [in] */ ADDRESS_TYPE Address,
    /* [in] */ DWORD dwBytesToRead,
    SAFEARRAY * *pbReadBuffer)
{
    try
    {
        if (pbReadBuffer == nullptr)
        {
            return E_POINTER;
        }

        SimpleCharBuffer buffer = m_pKDController->ReadMemory(Address, dwBytesToRead);
        return SafeArrayFromByteArray(buffer.GetInternalBuffer(), buffer.GetLength(), pbReadBuffer);
    }
    CATCH_AND_RETURN_HRESULT;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::WriteVirtualMemory( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ SAFEARRAY * pBuffer,
        /* [out] */ DWORD *pdwBytesWritten)
{
    if (pBuffer == nullptr || pdwBytesWritten == nullptr)
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

    UNREFERENCED_PARAMETER(bufferSize);
    UNREFERENCED_PARAMETER(pRawBuffer);
    UNREFERENCED_PARAMETER(Address);
    
	*pdwBytesWritten = 0;
	return E_NOTIMPL;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::ReadPhysicalMemoryOrPeriphIO( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ ADDRESS_SPACE_TYPE AddressSpace,
        /* [in] */ DWORD dwBytesToRead,
        /* [out] */ SAFEARRAY * *pReadBuffer)
{
    UNREFERENCED_PARAMETER(Address);
    UNREFERENCED_PARAMETER(AddressSpace);
    UNREFERENCED_PARAMETER(dwBytesToRead);
    UNREFERENCED_PARAMETER(pReadBuffer);
    return E_NOTIMPL;
	//METHOD_NOT_IMPLEMENTED;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::WritePhysicalMemoryOrPeriphIO( 
        /* [in] */ ADDRESS_TYPE Address,
        /* [in] */ ADDRESS_SPACE_TYPE AddressSpace,
        /* [in] */ SAFEARRAY * pBuffer,
        /* [out] */ DWORD *pdwBytesWritten)
{
    UNREFERENCED_PARAMETER(Address);
    UNREFERENCED_PARAMETER(AddressSpace);
    UNREFERENCED_PARAMETER(pdwBytesWritten);
    UNREFERENCED_PARAMETER(pBuffer);
	METHOD_NOT_IMPLEMENTED;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::Ioctl( 
        /* [in] */ SAFEARRAY * pInputBuffer,
        /* [in] */ DWORD dwBuffOutSize,
        /* [out] */ SAFEARRAY * *pOutputBuffer)
{

    UNREFERENCED_PARAMETER(dwBuffOutSize);
    UNREFERENCED_PARAMETER(pOutputBuffer);

    if (pInputBuffer == nullptr)
    {
        return E_POINTER;
    }

    if (pInputBuffer->cDims != 1)
    {
        return E_INVALIDARG;
    }

    VARTYPE dataType;
    if (FAILED(SafeArrayGetVartype(pInputBuffer, &dataType)) || dataType != VT_UI1)
    {
        return E_INVALIDARG;
    }
    
    PVOID pRawBuffer = pInputBuffer->pvData;
    const DBGENG_EXDI_IOCTL_CODE_V3_EX * pExdiV3 = reinterpret_cast<const DBGENG_EXDI_IOCTL_CODE_V3_EX *>(pRawBuffer);
    DBGENG_EXDI_IOCTL_CODE_V3_EX ioctlCode = *pExdiV3;
    switch(ioctlCode)
	{
        //
        //  The below ioctl codes are not implemented in this sample
        //
        //  Return the NT base address (this code is allocated for specific COM servers
        //  that implement finding the NT OS base address rather than the heuristic
        //  used by the debugger engine)
        case DBGENG_EXDI_IOCTL_V3_GET_NT_BASE_ADDRESS_VALUE:
        //
        //  The below codes can be consumed currently by the debugger engine, in order to 
        //  implement various heuristic mechanism to determine the base address
        //  for system Apps (e.g. NT/bootmgr/hv). 
        //
        //  Read of the special registers can be implemented by reading the 
        //  MSR register values (architecture specific)
        case DBGENG_EXDI_IOCTL_V3_GET_SPECIAL_REGISTER_VALUE:
        case DBGENG_EXDI_IOCTL_V3_GET_SUPERVISOR_MODE_MEM_VALUE:
        case DBGENG_EXDI_IOCTL_V3_GET_HYPERVISOR_MODE_MEM_VALUE:
        break;
    }

    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetLastHitBreakpoint( 
    /* [out] */ DBGENG_EXDI3_GET_BREAKPOINT_HIT_OUT *pBreakpointInformation)
{
    UNREFERENCED_PARAMETER(pBreakpointInformation);
    return E_NOTIMPL;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetKPCRForProcessor( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [out] */ ULONG64 *pKPCRPointer)
{
    if (pKPCRPointer == nullptr)
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

    //On a real ARM device the KPCR can be obtained by reading the TPIDRPRW register 
    //and clearing 12 least-significant bits in the value.
	//*reinterpret_cast<ULONG64 *>(pbBufferOut) = ReadCoprocessorRegister(TPIDRPRW) & ~0xFFF;
	*pKPCRPointer = m_pKDController->GetKPCRAddress(dwProcessorNumber);
	return S_OK;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::ReadKdVersionBlock( 
        /* [in] */ DWORD dwBufferSize,
        /* [out] */ SAFEARRAY * *pKdVersionBlockBuffer)
{
    UNREFERENCED_PARAMETER(dwBufferSize);
    UNREFERENCED_PARAMETER(pKdVersionBlockBuffer);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::ReadMSR( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ DWORD dwRegisterIndex,
    /* [out] */ ULONG64 *pValue)
{
    UNREFERENCED_PARAMETER(dwProcessorNumber);
    UNREFERENCED_PARAMETER(dwRegisterIndex);
    UNREFERENCED_PARAMETER(pValue);
    return E_NOTIMPL;
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::WriteMSR( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ DWORD dwRegisterIndex,
    /* [in] */ ULONG64 value)
{
    UNREFERENCED_PARAMETER(dwProcessorNumber);
    UNREFERENCED_PARAMETER(dwRegisterIndex);
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}


// ------------------------------------------------------------------------------


HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [out][in] */ PCONTEXT_ARM4 pContext)
{
	return GetContextEx(dwProcessorNumber, pContext);
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::SetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ CONTEXT_ARM4 Context)
{
	return SetContextEx(dwProcessorNumber, &Context);
}

// ------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [out][in] */ PCONTEXT_X86_64 pContext)
{
	return GetContextEx(dwProcessorNumber, pContext);
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::SetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ CONTEXT_X86_64 Context)
{
	return SetContextEx(dwProcessorNumber, &Context);
}

// ------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [out][in] */ PCONTEXT_ARMV8ARCH64 pContext)
{
	return GetContextEx(dwProcessorNumber, pContext);
}
        
HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::SetContext( 
    /* [in] */ DWORD dwProcessorNumber,
    /* [in] */ CONTEXT_ARMV8ARCH64 context)
{
	return SetContextEx(dwProcessorNumber, &context);
}

// ------------------------------------------------------------------------------

HRESULT CStaticExdiSampleServer::FinalConstruct()
{
    try
    {
        TCHAR KDDirectory[MAX_PATH] = _T("C:\\Program Files (x86)\\Windows Kits\\8.0\\Debuggers\\x64");
        TCHAR KDArguments[1024] = _T("-k usb:targetname=surface");

        DWORD directoryLength = GetEnvironmentVariable(_T("EXDI_SAMPLE_KD_DIRECTORY"), KDDirectory, _countof(KDDirectory));
        DWORD argumentsLength = GetEnvironmentVariable(_T("EXDI_SAMPLE_KD_ARGUMENTS"), KDArguments, _countof(KDArguments));

        if (directoryLength == 0 || argumentsLength == 0)
        {
            if (MessageBox(0, _T("Warning: the EXDI_SAMPLE_KD_DIRECTORY and EXDI_SAMPLE_KD_ARGUMENTS environment variables \
are not defined. The sample will continue with default parameters (trying to connect \
to \\\\.\\pipe\\vmkerneltest1). "), _T("EXDI Sample"), MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST) != IDOK)
            {
                return E_ABORT;
            }
        }

        m_pKDController = AsynchronousKDController::Create(KDDirectory, KDArguments);
        m_pKDController->SetTextHandler(new KDCommandLogger(true));

        HRESULT result = S_OK;

        std::string targetResponse; 
        std::string effectiveMachine = m_pKDController->GetEffectiveMachine(&targetResponse);
        if (effectiveMachine == "ARM")
        {
            size_t archType = targetResponse.find("ARM 64");
            if (archType == std::string::npos)
            {
                m_detectedProcessorFamily = PROCESSOR_FAMILY_ARM;
            }
            else
            {
                m_detectedProcessorFamily = PROCESSOR_FAMILY_ARMV8ARCH64;            
            }
        }
        else if (effectiveMachine == "x64")
        {
            m_detectedProcessorFamily = PROCESSOR_FAMILY_X86;
        }
        else
        {
            MessageBox(0,
                       _T("KD reported an unsupported machine type. This example supports ARM and x64 only"),
                       _T("EXDI Sample"),
                       MB_ICONERROR);
            result = E_FAIL;
        }

        return result;
    }
    CATCH_AND_RETURN_HRESULT;
}

void CStaticExdiSampleServer::FinalRelease()
{
    delete m_pKDController;
    m_pKDController = nullptr;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetNumberOfProcessors( 
    /* [out] */ DWORD *pdwNumberOfProcessors)
{
	if (pdwNumberOfProcessors == nullptr)
	{
		return E_POINTER;
	}
	try
	{
		*pdwNumberOfProcessors = m_pKDController->GetProcessorCount();
		return S_OK;
	}
	CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_ARM4 pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        memset(pContext, 0, sizeof(*pContext));

        std::map<std::string, std::string> registers = m_pKDController->QueryAllRegisters(processorNumber);

        pContext->R0 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r0"]));
        pContext->R1 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r1"]));
        pContext->R2 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r2"]));
        pContext->R3 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r3"]));
        pContext->R4 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r4"]));
        pContext->R5 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r5"]));
        pContext->R6 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r6"]));
        pContext->R7 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r7"]));
        pContext->R8 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r8"]));
        pContext->R9 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r9"]));
        pContext->R10 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r10"]));
        pContext->R11 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r11"]));
        pContext->R12 = static_cast<DWORD>(KDController::ParseRegisterValue(registers["r12"]));
        pContext->Sp = static_cast<DWORD>(KDController::ParseRegisterValue(registers["sp"]));
        pContext->Lr = static_cast<DWORD>(KDController::ParseRegisterValue(registers["lr"]));
        pContext->Pc = static_cast<DWORD>(KDController::ParseRegisterValue(registers["pc"]));
        pContext->Psr = static_cast<DWORD>(KDController::ParseRegisterValue(registers["psr"]));

        pContext->RegGroupSelection.fControlRegs = TRUE;
        pContext->RegGroupSelection.fIntegerRegs = TRUE;

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARM4 *pContext)
{
    UNREFERENCED_PARAMETER(processorNumber);
    UNREFERENCED_PARAMETER(pContext);
	METHOD_NOT_IMPLEMENTED;
}

// ------------------------------------------------------------------------------

#define AMD64_CONTEXT_AMD64             0x00100000L
#define AMD64_CONTEXT_CONTROL           (AMD64_CONTEXT_AMD64 | 0x00000001L)
#define AMD64_CONTEXT_INTEGER           (AMD64_CONTEXT_AMD64 | 0x00000002L)
#define AMD64_CONTEXT_SEGMENTS          (AMD64_CONTEXT_AMD64 | 0x00000004L)
#define AMD64_CONTEXT_FLOATING_POINT    (AMD64_CONTEXT_AMD64 | 0x00000008L)
#define AMD64_CONTEXT_DEBUG_REGISTERS   (AMD64_CONTEXT_AMD64 | 0x00000010L)
#define AMD64_CONTEXT_FULL \
    (AMD64_CONTEXT_CONTROL | AMD64_CONTEXT_INTEGER | AMD64_CONTEXT_FLOATING_POINT)


HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_X86_64 pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        memset(pContext, 0, sizeof(CONTEXT_X86_64));

        //We do not fetch the actual descriptors, thus we mark them as invalid
        pContext->DescriptorCs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorSs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorGs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorFs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorEs.SegFlags = static_cast<DWORD>(-1);
        pContext->DescriptorDs.SegFlags = static_cast<DWORD>(-1);

        std::map<std::string, std::string> registers = m_pKDController->QueryAllRegisters(processorNumber);

        pContext->Rax = KDController::ParseRegisterValue(registers["rax"]);
        pContext->Rbx = KDController::ParseRegisterValue(registers["rbx"]);
        pContext->Rcx = KDController::ParseRegisterValue(registers["rcx"]);
        pContext->Rdx = KDController::ParseRegisterValue(registers["rdx"]);
        pContext->Rsi = KDController::ParseRegisterValue(registers["rsi"]);
        pContext->Rdi = KDController::ParseRegisterValue(registers["rdi"]);
        pContext->Rip = KDController::ParseRegisterValue(registers["rip"]);
        pContext->Rsp = KDController::ParseRegisterValue(registers["rsp"]);
        pContext->Rbp = KDController::ParseRegisterValue(registers["rbp"]);
        pContext->R8  = KDController::ParseRegisterValue(registers["r8"]);
        pContext->R9  = KDController::ParseRegisterValue(registers["r9"]);
        pContext->R10 = KDController::ParseRegisterValue(registers["r10"]);
        pContext->R11 = KDController::ParseRegisterValue(registers["r11"]);
        pContext->R12 = KDController::ParseRegisterValue(registers["r12"]);
        pContext->R13 = KDController::ParseRegisterValue(registers["r13"]);
        pContext->R14 = KDController::ParseRegisterValue(registers["r14"]);
        pContext->R15 = KDController::ParseRegisterValue(registers["r15"]);

        pContext->SegCs = static_cast<DWORD>(KDController::ParseRegisterValue(registers["cs"]));
        pContext->SegSs = static_cast<DWORD>(KDController::ParseRegisterValue(registers["ss"]));
        pContext->SegDs = static_cast<DWORD>(KDController::ParseRegisterValue(registers["ds"]));
        pContext->SegEs = static_cast<DWORD>(KDController::ParseRegisterValue(registers["es"]));
        pContext->SegFs = static_cast<DWORD>(KDController::ParseRegisterValue(registers["fs"]));
        pContext->SegGs = static_cast<DWORD>(KDController::ParseRegisterValue(registers["gs"]));

        pContext->EFlags = KDController::ParseRegisterValue(registers["efl"]);

        pContext->RegGroupSelection.fFloatingPointRegs = FALSE;
        pContext->RegGroupSelection.fDebugRegs = FALSE;
        pContext->RegGroupSelection.fSSERegisters = FALSE;
        pContext->RegGroupSelection.fSystemRegisters = FALSE;

        pContext->RegGroupSelection.fIntegerRegs = TRUE;
        pContext->RegGroupSelection.fSegmentRegs = TRUE;

        pContext->ModeFlags = AMD64_CONTEXT_AMD64 | AMD64_CONTEXT_CONTROL | AMD64_CONTEXT_INTEGER | AMD64_CONTEXT_SEGMENTS;
        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_X86_64 *pContext)
{
	UNREFERENCED_PARAMETER(processorNumber);
	UNREFERENCED_PARAMETER(pContext);
	METHOD_NOT_IMPLEMENTED;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::SetContextEx(_In_ DWORD processorNumber, _In_ const CONTEXT_ARMV8ARCH64 *pContext)
{
	UNREFERENCED_PARAMETER(processorNumber);
	UNREFERENCED_PARAMETER(pContext);
	METHOD_NOT_IMPLEMENTED;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::GetContextEx(_In_ DWORD processorNumber, _Inout_ PCONTEXT_ARMV8ARCH64 pContext)
{
    if (pContext == nullptr)
    {
        return E_POINTER;
    }

    try
    {
        memset(pContext, 0, sizeof(*pContext));

        std::map<std::string, std::string> registers = GetKDController()->QueryAllRegisters(processorNumber);

        for (int i = 0; i < ARMV8ARCH64_MAX_INTERGER_REGISTERS; i++)
        {
            char registerNameStr[4];
            sprintf_s(registerNameStr, _countof(registerNameStr), "x%d", i);
			std::string registerName(registerNameStr);
            pContext->X[i] = KDController::ParseRegisterValue(registers[registerName]);
        }
        pContext->Fp = KDController::ParseRegisterValue(registers["fp"]);
        pContext->Lr = KDController::ParseRegisterValue(registers["lr"]);
        pContext->Sp = KDController::ParseRegisterValue(registers["sp"]);
        pContext->Pc = KDController::ParseRegisterValue(registers["pc"]);
        pContext->Sp = KDController::ParseRegisterValue(registers["sp"]);
        pContext->Psr = KDController::ParseRegisterValue(registers["psr"]);

        pContext->RegGroupSelection.fControlRegs = TRUE;
        pContext->RegGroupSelection.fIntegerRegs = TRUE;

        return S_OK;
    }
    CATCH_AND_RETURN_HRESULT;
}

ADDRESS_TYPE CStaticExdiSampleServer::GetCurrentExecutionAddress(_Out_ DWORD *pProcessorNumberOfLastEvent)
{
    assert(pProcessorNumberOfLastEvent != nullptr);
    *pProcessorNumberOfLastEvent = m_pKDController->GetLastKnownActiveCpu();
    ADDRESS_TYPE result;
    std::map<std::string, std::string> registers = m_pKDController->QueryAllRegisters(*pProcessorNumberOfLastEvent);

    if (m_detectedProcessorFamily == PROCESSOR_FAMILY_ARM || m_detectedProcessorFamily == PROCESSOR_FAMILY_ARMV8ARCH64)
    {
        result = KDController::ParseRegisterValue(registers["pc"]);
    }
    else if (m_detectedProcessorFamily == PROCESSOR_FAMILY_X86)
    {
        result = KDController::ParseRegisterValue(registers["rip"]);
    }
    else
    {
        throw std::exception("Unknown CPU architecture. Please add support for it");
    }

    return result;
}

HRESULT STDMETHODCALLTYPE CStaticExdiSampleServer::SetKeepaliveInterface(/* [in] */ IeXdiKeepaliveInterface3 *pKeepalive)
{
	UNREFERENCED_PARAMETER(pKeepalive);
    return E_NOTIMPL;
}