//----------------------------------------------------------------------------
//
// GdbSrvController.cpp
//
// This module implements a class that runs a GdbServer client that services 
// the DbgEng-Exdi debugger requests.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "GdbSrvControllerLib.h"
#include "cfgExdiGdbSrvHelper.h"
#include "ExceptionHelpers.h"
#include <new>
#include <functional>
#include <algorithm>
#include <string>
#include <memory>

using namespace GdbSrvControllerLib;

//=============================================================================
// Private data definitions
//=============================================================================    
#define TARGET_ARM_ARCH_PTR_SIZE(target)      ((target == ARM32_ARCH) ? sizeof(ULONG) : sizeof(ULONG64))
#define TARGET_INTEL_ARCH_PTR_SIZE(target)    ((target == X86_ARCH) ? sizeof(ULONG) : sizeof(ULONG64))
#define GET_PTR_SIZE_BY_ARCH(arch)            (((arch == X86_ARCH) || (arch == AMD64_ARCH)) ? \
                                               TARGET_INTEL_ARCH_PTR_SIZE(arch) : TARGET_ARM_ARCH_PTR_SIZE(arch))

// ARM64 Exception Level 1 cpsr register values (CPSRM_EL1h)
const DWORD C_EL1TCPSRREG = 4;
const DWORD C_EL1HCPSRREG = 5;
// ARM64 Exception Level 2
const DWORD C_EL2TCPSRREG = 8;
const DWORD C_EL2HCPSRREG = 9;

//  Maximum monitor command buffer
const DWORD C_MAX_MONITOR_CMD_BUFFER = 2048;

//  Maximum size of register name string
const DWORD C_MAX_REGISTER_NAME_ARRAY_ELEM = 32;

//  List of Exdi-Component functions that can be invoked from the debugger engine side.
//  This can be expanded to include any function that can be executed from the engine.
//  The engine just passes through this function to the Exdi-Component.
const PCWSTR exdiComponentFunctionList[] =
{
    L"connect",
    L"close"
};

//  The below arrays array will be used for processing CPU context (Set/GetContext) related RSP packets.
//  An array entry contains the following fields:
//  1. The register name descriptor,
//  2. The register ID that matches with the array entry number. 
//     It's used to indentify the register in Set context related packets.
//  3. The register size in bytes.
//  

//  This structure indicates the GdbServer x86 register array.
const RegistersStruct x86RegisterArray[MAX_REG_X86_NUMBER] =
{
  {"Eax",           "0",     4},
  {"Ecx",           "1",     4},
  {"Edx",           "2",     4},
  {"Ebx",           "3",     4},
  {"Esp",           "4",     4},
  {"Ebp",           "5",     4},
  {"Esi",           "6",     4},
  {"Edi",           "7",     4},
  {"Eip",           "8",     4},
  {"EFlags",        "9",     4},
  {"SegCs",         "a",     4},
  {"SegSs",         "b",     4},
  {"SegDs",         "c",     4},
  {"SegEs",         "d",     4},
  {"SegFs",         "e",     4},
  {"SegGs",         "f",     4},
  {"st0",           "10",    10},
  {"st1",           "11",    10},
  {"st2",           "12",    10},
  {"st3",           "13",    10},
  {"st4",           "14",    10},
  {"st5",           "15",    10},
  {"st6",           "16",    10},
  {"st7",           "17",    10},
  {"ControlWord",   "18",    4},
  {"StatusWord",    "19",    4},
  {"TagWord",       "1a",    4},
  {"ErrorOffset",   "1b",    4},
  {"ErrorSelector", "1c",    4},
  {"DataOffset",    "1d",    4},
  {"DataSelector",  "1e",    4},
  {"Cr0NpxState",   "1f",    4},
  {"xmm0",          "20",    16},
  {"xmm1",          "21",    16},
  {"xmm2",          "22",    16},
  {"xmm3",          "23",    16},
  {"xmm4",          "24",    16},
  {"xmm5",          "25",    16},
  {"xmm6",          "26",    16},
  {"xmm7",          "27",    16},
};

// this is a variant implemented by QEMU
// NB: a better approach would be to use the Xfer:features:read packet to query supported registers
const RegistersStruct arm32RegisterArray_Qemu[MAX_REG_ARM32_NUMBER] =
{
  {"r0",           "0",     4},
  {"r1",           "1",     4},
  {"r2",           "2",     4},
  {"r3",           "3",     4},
  {"r4",           "4",     4},
  {"r5",           "5",     4},
  {"r6",           "6",     4},
  {"r7",           "7",     4},
  {"r8",           "8",     4},
  {"r9",           "9",     4},
  {"r10",          "a",     4},
  {"r11",          "b",     4},
  {"r12",          "c",     4},
  {"sp",           "d",     4},
  {"lr",           "e",     4},
  {"pc",           "f",     4},
  // Legacy floating-point registers (always zero)
  {"f0",           "10",    12},
  {"f1",           "11",    12},
  {"f2",           "12",    12},
  {"f3",           "13",    12},
  {"f4",           "14",    12},
  {"f5",           "15",    12},
  {"f6",           "16",    12},
  {"f7",           "17",    12},
  {"fps",          "18",    4},
  // Processor status flags register
  {"Cpsr",         "19",    4},
};

//  This structure indicates the GdbServer Arm 32 bit register array.
const RegistersStruct arm32RegisterArray[MAX_REG_ARM32_NUMBER] =
{
  {"r0",           "0",     4},
  {"r1",           "1",     4},
  {"r2",           "2",     4},
  {"r3",           "3",     4},
  {"r4",           "4",     4},
  {"r5",           "5",     4},
  {"r6",           "6",     4},
  {"r7",           "7",     4},
  {"r8",           "8",     4},
  {"r9",           "9",     4},
  {"r10",          "a",     4},
  {"r11",          "b",     4},
  {"r12",          "c",     4},
  {"sp",           "d",     4},
  {"lr",           "e",     4},
  {"pc",           "f",     4},
  // Processor status flags register
  {"Cpsr",         "10",    4},
  // Neon register DWORD64 D[32]
  {"d0",           "11",    8},
  {"d1",           "12",    8},
  {"d2",           "13",    8},
  {"d3",           "14",    8},
  {"d4",           "15",    8},
  {"d5",           "16",    8},
  {"d6",           "17",    8},
  {"d7",           "18",    8},
  {"d8",           "19",    8},
  {"d9",           "1a",    8},
  {"d10",          "1b",    8},
  {"d11",          "1c",    8},
  {"d12",          "1d",    8},
  {"d13",          "1e",    8},
  {"d14",          "1f",    8},
  {"d15",          "20",    8},
  {"d16",          "21",    8},
  {"d17",          "22",    8},
  {"d18",          "23",    8},
  {"d19",          "24",    8},
  {"d20",          "25",    8},
  {"d21",          "26",    8},
  {"d22",          "27",    8},
  {"d23",          "28",    8},
  {"d24",          "29",    8},
  {"d25",          "2a",    8},
  {"d26",          "2b",    8},
  {"d27",          "2c",    8},
  {"d28",          "2d",    8},
  {"d29",          "2e",    8},
  {"d30",          "2f",    8},
  {"d31",          "30",    8},
  // Floating point status register
  {"Fpscr",        "31",    4},
};

//  This structure indicates the GdbServer amd64 register array.
//  !!!! The amd64 code has not been tested, so ensure testing this array before using it !!!
const RegistersStruct amd64RegisterArray[MAX_REG_AMD64_NUMBER] =
{
  {"rax",           "0",    8},
  {"rbx",           "1",    8},
  {"rcx",           "2",    8},
  {"rdx",           "3",    8},
  {"rsi",           "4",    8},
  {"rdi",           "5",    8},
  {"rbp",           "6",    8},
  {"rsp",           "7",    8},
  {"r8",            "8",    8},
  {"r9",            "9",    8},
  {"r10",           "a",    8},
  {"r11",           "b",    8},
  {"r12",           "c",    8},
  {"r13",           "d",    8},
  {"r14",           "e",    8},
  {"r15",           "f",    8},
  {"rip",           "10",   8},
  {"eflags",        "11",   4},
  {"ds",            "12",   4},
  {"es",            "13",   4},
  {"fs",            "14",   4},
  {"gs",            "15",   4},
  {"st0",           "16",   10},
  {"st1",           "17",   10},
  {"st2",           "18",   10},
  {"st3",           "19",   10},
  {"st4",           "1a",   10},
  {"st5",           "1b",   10},
  {"st6",           "1c",   10},
  {"st7",           "1d",   10},
  {"ControlWord",   "1e",   4},
  {"StatusWord",    "1f",   4},
  {"TagWord",       "20",   4},
  {"ErrorOffset",   "21",   4},
  {"ErrorSelector", "22",   4},
  {"DataOffset",    "23",   4},
  {"DataSelector",  "24",   4},
  {"Cr0NpxState",   "25",   4},
  {"xmm0",          "26",   16},
  {"xmm1",          "27",   16},
  {"xmm2",          "28",   16},
  {"xmm3",          "29",   16},
  {"xmm4",          "2a",   16},
  {"xmm5",          "2b",   16},
  {"xmm6",          "2c",   16},
  {"xmm7",          "2d",   16},
  {"xmm8",          "2e",   16},
  {"xmm9",          "2f",   16},
  {"xmm10",         "30",   16},
  {"xmm11",         "31",   16},
  {"xmm12",         "32",   16},
  {"xmm13",         "33",   16},
  {"xmm14",         "34",   16},
  {"xmm15",         "35",   16},
  {"mxcsr",         "36",   4}
};

//  This structure indicates the GdbServer Arm 64 bit register array.
//  !!! this has not been tested yet, and it's incompleted no fp, lr, V registers !!!
const RegistersStruct arm64RegisterArray[MAX_REG_ARM64_NUMBER] =
{
  {"X0",           "0",     8},
  {"X1",           "1",     8},
  {"X2",           "2",     8},
  {"X3",           "3",     8},
  {"X4",           "4",     8},
  {"X5",           "5",     8},
  {"X6",           "6",     8},
  {"X7",           "7",     8},
  {"X8",           "8",     8},
  {"X9",           "9",     8},
  {"X10",          "a",     8},
  {"X11",          "b",     8},
  {"X12",          "c",     8},
  {"X13",          "d",     8},
  {"X14",          "e",     8},
  {"X15",          "f",     8},
  {"X16",          "10",    8},
  {"X17",          "11",    8},
  {"X18",          "12",    8},
  {"X19",          "13",    8},
  {"X20",          "14",    8},
  {"X21",          "15",    8},
  {"X22",          "16",    8},
  {"X23",          "17",    8},
  {"X24",          "18",    8},
  {"X25",          "19",    8},
  {"X26",          "1a",    8},
  {"X27",          "1b",    8},
  {"X28",          "1c",    8},
  {"fp",           "1d",    8},
  {"lr",           "1e",    8},
  {"sp",           "1f",    8},
  {"pc",           "20",    8},
  {"cpsr",         "21",    8},
  // Neon FP registers, fpsr, fpcr
  {"V0",           "22",    16},
  {"V1",           "23",    16},
  {"V2",           "24",    16},
  {"V3",           "25",    16},
  {"V4",           "26",    16},
  {"V5",           "27",    16},
  {"V6",           "28",    16},
  {"V7",           "29",    16},
  {"V8",           "2a",    16},
  {"V9",           "2b",    16},
  {"V10",          "2c",    16},
  {"V11",          "2d",    16},
  {"V12",          "2e",    16},
  {"V13",          "2f",    16},
  {"V14",          "30",    16},
  {"V15",          "31",    16},
  {"V16",          "32",    16},
  {"V17",          "33",    16},
  {"V18",          "34",    16},
  {"V19",          "35",    16},
  {"V20",          "36",    16},
  {"V21",          "37",    16},
  {"V22",          "38",    16},
  {"V23",          "39",    16},
  {"V24",          "3a",    16},
  {"V25",          "3b",    16},
  {"V26",          "3c",    16},
  {"V27",          "3d",    16},
  {"V28",          "3e",    16},
  {"V29",          "3f",    16},
  {"V30",          "3f",    16},
  {"V31",          "3f",    16},
  {"fpsr",         "40",    4},
  {"fpcr",         "41",    4},
};

// Telemetry command and TargetIDs
LPCWSTR const g_GdbSrvTelemetryCmd = L"ExdiDbgType";
LPCSTR const g_GdbSrvTrace32 = "GdbSrv-Trace32";
LPCSTR const g_GdbSrvGeneric = "GdbSrv-Generic";

//=============================================================================
// Private function definitions
//=============================================================================
#pragma region Architecture helpers

inline void MakeLowerCase(_In_ PCWSTR pIn, _Out_ std::wstring &out)
{
	out += pIn;
	std::transform(out.cbegin(), out.cend(),  // Source
				   out.begin(),				  // destination
				   [] (wchar_t c) {
				       return towlower(c);    // operation
				   });                        
}

inline void DisplayTextData(_In_reads_bytes_(readSize) const char * pBuffer, _In_ size_t readSize,
                            _In_ GdbSrvTextType textType, _In_ IGdbSrvTextHandler * const pTextHandler)
{
    assert(pTextHandler != nullptr);

    pTextHandler->HandleText(textType, pBuffer, readSize);
}

void DisplayCommData(_In_reads_bytes_(readSize) const char * pBuffer,  _In_ size_t readSize,
                     _In_ GdbSrvTextType textType, _In_ IGdbSrvTextHandler * const pTextHandler, _In_ unsigned channel)
{
    UNREFERENCED_PARAMETER(channel);
    DisplayTextData(pBuffer, readSize, textType, pTextHandler);
}

void DisplayCommDataForChannel(_In_reads_bytes_(readSize) const char * pBuffer,  _In_ size_t readSize,
                               _In_ GdbSrvTextType textType, _In_ IGdbSrvTextHandler * const pTextHandler, _In_ unsigned channel)
{
    UNREFERENCED_PARAMETER(readSize);
    assert(pTextHandler != nullptr);

    if (*pBuffer != '\x0')
    {
        char channelText[128]; 
        sprintf_s(channelText, _countof(channelText), "Core:%u ", channel);
        std::string channelString(channelText);
        channelString += pBuffer;
        DisplayTextData(channelString.c_str(), strlen(channelString.c_str()), textType, pTextHandler);
    }
}

//
//  ReverseRegValue         Returns a string containing the passed in register string in reverse order.
//
//  Parameters:
//  inputRegTargetOrder     Reference to the input string that contains hex-ascii characters in target order.
//
//  Return:
//  The reversed string.
//
std::string ReverseRegValue(_In_ const std::string & inputRegTargetOrder)
{
    std::string outRegValue(inputRegTargetOrder); 

    size_t outRegValueLength = outRegValue.length();
    for (size_t idx = 0; idx < outRegValueLength; idx +=2)
    {
        std::swap(outRegValue[idx], outRegValue[idx + 1]);
    }
    reverse(outRegValue.begin(), outRegValue.end());

    return outRegValue;
}

HRESULT SetSpecialMemoryPacketTypeARM64(_In_ ULONGLONG cpsrReg, _Out_ memoryAccessType * pMemType)
{
    assert(pMemType != nullptr);

    HRESULT hr = S_OK;
    switch (cpsrReg & 0xf)
    {
        //  NT space
        case C_EL1HCPSRREG :
        case C_EL1TCPSRREG :
        //  Hypervisor space
        case C_EL2TCPSRREG :
        case C_EL2HCPSRREG :
            {
                pMemType->isSpecialRegs = 1;
            }
        break;

        default:
            {
                hr = E_FAIL;
                MessageBox(0, _T("Error: Invalid processor mode for getting ARM64 special registers."), _T("EXDI-GdbServer"), MB_ICONERROR);
            }
    }
    return hr;
}

HRESULT SetSpecialMemoryPacketType(_In_ TargetArchitecture arch, _In_ ULONGLONG cpsrReg, _Out_ memoryAccessType * pMemType)
{
    HRESULT hr = E_NOTIMPL;
    if (arch == ARM64_ARCH)
    {
        hr = SetSpecialMemoryPacketTypeARM64(cpsrReg, pMemType);
    }
    return hr;
}

const char * GetProcessorStatusRegByArch(_In_ TargetArchitecture arch)
{
    const char * pStatusRegister = nullptr;
    if (arch == ARM64_ARCH)
    {
        pStatusRegister = "cpsr";
    }
    return pStatusRegister;
}
#pragma endregion


class GdbSrvController::GdbSrvControllerImpl
{
    public:
		GdbSrvControllerImpl::GdbSrvControllerImpl(_In_ const std::vector<std::wstring> &coreNumberConnectionParameters) :
                         m_pTextHandler(nullptr),
                         m_cachedProcessorCount(0),
                         m_lastKnownActiveCpu(0),
                         m_TargetHaltReason(TARGET_UNKNOWN),
                         m_displayCommands(true),
                         m_targetProcessorArch(UNKNOWN_ARCH),
                         m_ThreadStartIndex(-1),
                         m_pRspClient(std::unique_ptr <GdbSrvRspClient<TcpConnectorStream>>
                                     (new (std::nothrow) GdbSrvRspClient<TcpConnectorStream>(coreNumberConnectionParameters)))
    {
        m_cachedKPCRStartAddress.clear();
        //  Bind the exdi functions
		SetExdiFunctions(exdiComponentFunctionList[0], std::bind(&GdbSrvController::GdbSrvControllerImpl::AttachGdbSrv, 
		                                                         this, std::placeholders::_1, std::placeholders::_2));
		SetExdiFunctions(exdiComponentFunctionList[0], std::bind(&GdbSrvController::GdbSrvControllerImpl::CloseGdbSrvCore, 
		                                                         this, std::placeholders::_1, std::placeholders::_2));
        ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        m_IsThrowExceptionEnabled = cfgData.IsExceptionThrowEnabled();
        
    }

    GdbSrvControllerImpl::~GdbSrvControllerImpl()
    {
        ShutdownGdbSrv();

        delete m_pTextHandler;
        m_pTextHandler = nullptr;
    }

    //  
    //  ExecuteExdiGdbSrvMonitor          Execute a GdbSrv monitor command.
    //
    //  Parameters:
    //  core                              Processor core.
    //  pCmdToExecute                     Pointer to the monitor command string to execute.
    //
    //  Return:
    //  The response buffer where each byte is transmitted as a two-digit ascii hexadecimal number. 
    //  The response may contain fewer bytes than requested if the server 
    //  was able to read only part of the region of memory. 
    //
    //  Request:
    //  'qRcmd, command in ascii hex digists'  
    //
    //  Response:
    //  'OK'             a command response with no output on the console
    //  'O'<output data> a sequence of output data. The last should be 'OK'.
    //  'E NN'           an Error or bad request.
    //  ''               the command is not recognize by GdbServer.
    //
    SimpleCharBuffer GdbSrvControllerImpl::ExecuteExdiGdbSrvMonitor(_In_ unsigned core, _In_ LPCWSTR pCmdToExecute)
    {
        assert(pCmdToExecute != nullptr);


        HRESULT gdbServerError = S_OK;
        //  Are we connected to the GdbServer on this core?
        if (m_pRspClient->GetRspSessionStatus(gdbServerError, core))
        {
            if (gdbServerError != ERROR_SUCCESS)
            {
                //  We are not connected, so open a RSP channel and connect to it.
                if (!AttachGdbSrv(GetCoreConnectionString(core), core))
                {
                    throw _com_error(E_FAIL);
                }
            }
        }
 
        SimpleCharBuffer monitorResult;
        if (!monitorResult.TryEnsureCapacity(C_MAX_MONITOR_CMD_BUFFER))
        {
            throw _com_error(E_OUTOFMEMORY);
        }

        if (core != C_ALLCORES && core > GetNumberOfRspConnections())
        {
            throw _com_error(E_INVALIDARG);
        }

        if (wcsstr(pCmdToExecute, g_GdbSrvTelemetryCmd) != nullptr)
        {
            // It's an Internal telemetry command
            // Then return the Gdb Server type that is currently connected
            LPCSTR pStrGdbSrvType;
            size_t gdbSrvStrTypeLength;
            if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM))
            {
                pStrGdbSrvType = g_GdbSrvTrace32;
                gdbSrvStrTypeLength = strlen(g_GdbSrvTrace32);
            }
            else
            {
                pStrGdbSrvType = g_GdbSrvGeneric;
                gdbSrvStrTypeLength = strlen(g_GdbSrvGeneric);
            }
            monitorResult.SetLength(monitorResult.GetLength() + gdbSrvStrTypeLength);
            memcpy(&monitorResult[monitorResult.GetLength() - gdbSrvStrTypeLength], pStrGdbSrvType, gdbSrvStrTypeLength);

            return monitorResult;
        }

        size_t cmdToExecMaxLength = (wcslen(pCmdToExecute) + 1) * sizeof(WCHAR);
        unique_ptr<char> pCommand(new (nothrow) char[cmdToExecMaxLength]);
        if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, pCmdToExecute, -1, pCommand.get(), static_cast<int>(cmdToExecMaxLength), nullptr, nullptr) == 0)
        {
            throw _com_error(E_INVALIDARG);
        }

        char * pMonitorCmd = pCommand.get();
        size_t commandLength = strlen(pMonitorCmd);
        std::string dataBuffer;
        for (size_t idx = 0; idx < commandLength; ++idx)
        {
            dataBuffer.insert(dataBuffer.end(), 1, NumberToAciiHex(((pMonitorCmd[idx] >> 4) & 0xf)));
            dataBuffer.insert(dataBuffer.end(), 1, NumberToAciiHex((pMonitorCmd[idx] & 0xf)));
        }

        std::string commandMonitor("qRcmd,");
        commandMonitor += dataBuffer.c_str();

        std::string reply = ExecuteCommandOnProcessor(commandMonitor.c_str(), true, 0, core);
        size_t messageLength = reply.length();

        //  Is an empty response or an error response 'E NN'?
        if (messageLength == 0 || IsReplyError(reply))
        {
            throw _com_error(E_FAIL);
        }

        messageLength = min(messageLength, C_MAX_MONITOR_CMD_BUFFER);
        bool replyDone = false;
        do
        {
            if (IsReplyOK(reply))
            {
                monitorResult.SetLength(monitorResult.GetLength() + messageLength);
                memcpy(&monitorResult[monitorResult.GetLength() - messageLength], reply.c_str(), messageLength);
                replyDone = true;
            }
            else
            {
                if (messageLength > (C_MAX_MONITOR_CMD_BUFFER - monitorResult.GetLength()))
                {
                    if (!monitorResult.TryEnsureCapacity(C_MAX_MONITOR_CMD_BUFFER))
                    {
                        throw _com_error(E_OUTOFMEMORY);
                    }
                }
                size_t pos = (reply[0] == 'O') ? 1 : 0;
	            for (; pos < messageLength; pos += 2)
                {
                    monitorResult.SetLength(monitorResult.GetLength() + 1);
                    unsigned char highByte = ((AciiHexToNumber(reply[pos]) << 4) & 0xf0);
                    monitorResult[monitorResult.GetLength() - 1] = highByte | (AciiHexToNumber(reply[pos + 1]) & 0x0f);
                }
                //  Try to read more packets
                bool IsPollingChannelMode = false;
                if (m_pRspClient->ReceiveRspPacketEx(reply, core, true, IsPollingChannelMode, false))
                {
                    messageLength = reply.length();
                }
                else
                {
                    replyDone = true;
                }
            }
        }
        while (!replyDone);
        
        return monitorResult;
    }

    //  
    //  ExecuteExdiFunction    Execute an Exdi component function.
    //
    //  Parameters:
    //  core                   Processor core.
    //  pFunctionToExecute     Pointer to the function string to execute.
    //
    //  Return:
    //  true                   Succeeded.
    //  false                  Otherwise.
    //
    bool GdbSrvControllerImpl::ExecuteExdiFunction(_In_ unsigned core, _In_ LPCWSTR pFunctionToExecute)
    {
        assert(pFunctionToExecute != nullptr);

        if (!CheckProcessorCoreNumber(core))
        {
            throw _com_error(E_INVALIDARG);
        }
        
        std::wstring functionToExec;
        MakeLowerCase(pFunctionToExecute, functionToExec);
        std::map<std::wstring, ExdiFunctions>::const_iterator itFunction = m_exdiFunctions.find(functionToExec);
        if (itFunction == m_exdiFunctions.end())
        {
            //  Unsupported function
            throw _com_error(E_NOTIMPL);
        }

        bool isAllCores = (core == C_ALLCORES) ? true : false;
        bool isFuncDone = false;
        unsigned numberOfCores = GetNumberOfRspConnections();
        for (unsigned coreNumber = 0; coreNumber < numberOfCores ; ++coreNumber)
        {
            if (isAllCores || coreNumber == core)
            {
                isFuncDone = itFunction->second(GetCoreConnectionString(coreNumber), coreNumber);
                if (!isFuncDone || !isAllCores)
                {
                    break;
                }

            }
        }
        return isFuncDone;
    }
	//  
	//  AttachGdbSrv    Open a new communication channel and connect to the Gdbserver
	//
	//  Parameters:
	//  connectionStr   Connection string.
	//  core            Processor core.
	//
	//  Return:
	//  true            Succeeded.
	//  false           Otherwise.
	//
    bool GdbSrvControllerImpl::AttachGdbSrv(_In_ const std::wstring &connectionStr, _In_ unsigned core)
    {
        assert(m_pRspClient != nullptr);

        bool isAttached = m_pRspClient->AttachRspToCore(connectionStr, core);
        if (isAttached)
        {
            ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
            isAttached = ConfigureGdbSrvCommSession(cfgData.GetDisplayCommPacketsCharacters(), core);
        }
        return isAttached;
    }

    //  
    //  ConnectGdbSrvCore   Connect to specific core.
    //
    //  Parameters:
    //  connectionStr       Connection string.
    //  core                Processor core.
    //
    //  Return:
    //  true                Succeeded.
    //  false               Otherwise.
    //
    bool GdbSrvControllerImpl::ConnectGdbSrvCore(_In_ const std::wstring &connectionStr, _In_ unsigned core)
    {
        assert(m_pRspClient != nullptr);

        return m_pRspClient->ConnectRspToCore(connectionStr, core);
    }

    //  
    //  CloseGdbSrv     Close an opened channel on the GdbServer.
    //
    //  Parameters:
    //  connectionStr   Close string.
    //  core            Processor core.
    //
    //  Return:
    //  true            Succeeded.
    //  false           Otherwise.
    //
    bool GdbSrvControllerImpl::CloseGdbSrvCore(_In_ const std::wstring &closeStr, _In_ unsigned core)
    {
        assert(m_pRspClient != nullptr);
        return m_pRspClient->CloseRspCore(closeStr, core);
    }

    //  
    //  ConnectGdbSrv   Connects to the GdbServer by using the specified link layer
    //                  connecting string.
    //  Return:
    //  true            Succeeded.
    //  false           Otherwise.
    //
    //  Note.
    //  This sample implements only TCP/IP (socket) connection, but the GdbServer
    //  supports serial connection.
    //
    bool GdbSrvControllerImpl::ConnectGdbSrv()
    {
        assert(m_pRspClient != nullptr);
        return m_pRspClient->ConnectRsp();
    }

    //
    //  ShutdownGdbSrv  Shutdown the connection by invoking the shutdown mechanism
    //                  implemented in the GdServer RSP layer.
    //
    void GdbSrvControllerImpl::ShutdownGdbSrv()
    {
        assert(m_pRspClient != nullptr);
        m_pRspClient->ShutDownRsp();
    }

    //
    //  ConfigureGdbSrvCommSession  Configures the communication session by setting the
    //                              connection default parameters. Also, if the display
    //                              communication trace option is set to "On" then
    //                              it'll set the callback display function.
    //  Parameters:
    //  fDisplayCommData            Flag if set then it'll set a callback display function that 
    //                              will show all packet characters.
    //  core                        Processor core.
    //
    //  Return:
    //  true            Succeeded.
    //  false           Otherwise.
    //
    bool GdbSrvControllerImpl::ConfigureGdbSrvCommSession(_In_ bool fDisplayCommData, _In_ unsigned core)
    {
        assert(m_pRspClient != nullptr);

        pSetDisplayCommData pFunction = nullptr;
        ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        if (fDisplayCommData)
        {
            if (cfgData.GetMultiCoreGdbServer())
            {
                pFunction = DisplayCommDataForChannel;
            }
            else
            {
                pFunction = DisplayCommData;
            }
            m_displayCommands = false;
        }
        const RSP_CONFIG_COMM_SESSION commSession = 
        {
            cfgData.GetMaxConnectAttempts(), 
            cfgData.GetSendPacketTimeout(), 
            cfgData.GetReceiveTimeout(), 
            pFunction, 
            m_pTextHandler
        };
        return m_pRspClient->ConfigRspSession(&commSession, core);
    }

    //
    //  RestartGdbSrvTarget This command restarts the target machine.
    //                      This command does not have a Gdbserver reply.
    //
    //  Note.
    //  This command should reboot only the target.
    //  !!! Do not confuse with restarting the GdbServer !!!
    //
    bool GdbSrvControllerImpl::RestartGdbSrvTarget()
    {
        bool isDone = false;

        //  Send the restart packet. It's only supported in extended mode.
        const char cmdRestartTarget[] = "R";
        std::string reply = ExecuteCommandEx(cmdRestartTarget, false, 0);
        if (IsReplyOK(reply))
        {
            isDone = false;
        }
        return isDone;
    }

    //
    //  CheckGdbSrvAlive    Checks if the GdbServer is still connected.
    //
    bool GdbSrvControllerImpl::CheckGdbSrvAlive(_Out_ HRESULT & error)
    {
        assert(m_pRspClient != nullptr);
        return m_pRspClient->GetRspSessionStatus(error, C_ALLCORES);    
    }

    //
    //  ReqGdbServerSupportedFeatures   Request the list of the enabled features from the GdbServer
    //
    //  Request:
    //  'qSupported'
    //  
    //  Response:
    //  The format response can be found here:
    //  https://sourceware.org/gdb/onlinedocs/gdb/General-Query-Packets.html#qSupported
    //
    //  Note.
    //  1.  Because these features are used internally by RSP protocol for formatting the packet then
    //      The processing of the packet response will be handled in the UpdateRspPacketFeatures() function.
    //  2.  Our current implementation looks for two feature replies (packet size and No ACK mode supported) 
    //      by the GdbServer.
    //  
    bool GdbSrvControllerImpl::ReqGdbServerSupportedFeatures()
    {
        assert(m_pRspClient != nullptr);

        //  Send the Q<agent string> packet if it's set in the configuration file
        ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        std::wstring wAgentName;
        cfgData.GetExdiComponentAgentNamePacket(wAgentName);
        if (!wAgentName.empty())
        {
            const std::string sAgentName(wAgentName.begin(), wAgentName.end());
            const std::string reply = ExecuteCommand(sAgentName.c_str());
            if (IsReplyError(reply))
            {
                return false;
            }
        }

        //  Send the "qSupported" packet
        const char qSupported[] = "qSupported";
        std::string cmdResponse = ExecuteCommand(qSupported);

        //  Set the features supported by invoking UpdateRspPacketFeatures(cmdResponse);
        return m_pRspClient->UpdateRspPacketFeatures(cmdResponse);
    }

    //
    //  ReportReasonTargetHalted    This implements the command "?". This command requests the reason of the target halted.
    //
    //
    //  Note.   The response format can be found here:
    //          https://sourceware.org/gdb/onlinedocs/gdb/Stop-Reply-Packets.html#Stop-Reply-Packets
    //
    //  Example:
    //      Submit a request stop reason command '?'
    //  Request:
    //      $?#3f
    //      Response:
    //      +
    //      $T05thread:00000001;05:8c3bb082;04:e43ab082;08:7f586281;#e7
    //      + 
    //
    TARGET_HALTED GdbSrvControllerImpl::ReportReasonTargetHalted(_Out_ StopReplyPacketStruct * pStopReply)
    {
        assert(pStopReply != nullptr);

        //  Send the "?" packet    
        const char cmdHaltReason[] = "?";
        unsigned numberOfCoreConnections = static_cast<unsigned>(m_pRspClient->GetNumberOfStreamConnections());
        m_TargetHaltReason = TARGET_MARKER;
        unsigned lastKnownCpu = GetLastKnownActiveCpu();
        for (unsigned core = 0; core < numberOfCoreConnections; ++core)
        {
            std::string cmdResponse = ExecuteCommandOnProcessor(cmdHaltReason, true, 0, core);
            StopReplyPacketStruct coreStopReply;
            if (HandleAsynchronousCommandResponse(cmdResponse, &coreStopReply) && !coreStopReply.status.isCoreRunning)
            {
                m_TargetHaltReason = coreStopReply.stopReason;
                if (coreStopReply.status.isTAAPacket && coreStopReply.status.isThreadFound)
                {
                    if (coreStopReply.processorNumber != static_cast<ULONG>(C_ALLCORES))
                    {
                        if (GetFirstThreadIndex() > 0)
                        {
                            m_lastKnownActiveCpu = coreStopReply.processorNumber - 1;
                        }
                        else
                        {
                            m_lastKnownActiveCpu = coreStopReply.processorNumber;
                        }
                    }
                    break;
                }
                else if (core == lastKnownCpu)
                {
                    memcpy(pStopReply, &coreStopReply, sizeof(StopReplyPacketStruct)); 
                }
            }
        }
        return m_TargetHaltReason;
    }

    //
    //  RequestTIB  Request the Windows OS specific thread information block.
    //
    //  Request:
    //     qGetTIBAddr:thread-id’   where the 'thread-id' specifies the procesor number/thread ID.
    //
    //  Response:
    //      'OK'    for success.
    //      'E NN'  for errors.
    //      ''      Ignore (empty response when it's not supported).
    //
    //  Note.
    //
    bool GdbSrvControllerImpl::RequestTIB()
    {
        assert(m_pRspClient != nullptr);

        bool isTIB = false;
        const char cmdHaltReason[] = "qGetTIBAddr:0";
        std::string cmdResponse = ExecuteCommand(cmdHaltReason);    

        if (!IsReplyError(cmdResponse))
        {
            //  Decode string
            isTIB = true;
        }
        return isTIB;
    }

    //
    //  IsTargetHalted  Identifies if the reason of the target halted condition is a debug-break.
    //
    //  Note.
    //  Please take a look at the ReportReasonTargetHalted() function for the packet info.
    //  
    bool GdbSrvControllerImpl::IsTargetHalted()
    {
        StopReplyPacketStruct stopReply;
        TARGET_HALTED haltReason = ReportReasonTargetHalted(&stopReply);
        return ((haltReason == TARGET_BREAK_SIGTRAP)||(haltReason == TARGET_BREAK_SIGINT));
    }

    //
    //  InterruptTarget Attempt to interrupt the target by sending the break RSP-GdbServer character sequence.
    //  
    //  Request:
    //      Sends the Interrupt character:
    //      0x03
    //  Response:
    //      The stop reply reason response:
    //          https://sourceware.org/gdb/onlinedocs/gdb/Stop-Reply-Packets.html#Stop-Reply-Packets
    //
    //  Example:
    //  Request:
    //      0x03
    //  Response:
    //      $T02thread:00000001;05:8c3bb082;04:e43ab082;08:7f586281;#e4
    //      +
    //  
    bool GdbSrvControllerImpl::InterruptTarget()
    {
        assert(m_pRspClient != nullptr);

        return m_pRspClient->SendRspInterrupt();
    }

    //  SetThreadCommand    Sets the thread/processor number.
    //  
    //  Request:
    //  'H operation ThreadId'  The operation field depends on the next operation.
    //  'c'                     The next operation is a step or continue operation.
    //  'g'                     For other operations.
    //
    //  Response:
    //  'OK'    for success.
    //  'E NN'  for errors.
    //  ''      Ignore (empty response when it's not supported).
    //
    //  Example:
    //  Request:
    //      $Hg0#df
    //  Response:
    //      +
    //      $OK#9a
    //      +
    //
    bool GdbSrvControllerImpl::SetThreadCommand(_In_ unsigned processorNumber, _In_ const char * pOperation)
    {
        assert(pOperation != nullptr);

        ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        if (cfgData.GetMultiCoreGdbServer())
        {
            //  If we have the multi-GbServer sessions then we do not need to send the command
            //  for setting one specific processor core as we are connected to the specific
            //  core via the core specific GdbServer session.
            m_lastKnownActiveCpu = processorNumber;
            return true;
        }

        //  We need to set the processor number before query the register values
        char setThreadCommand[32] = "H";
        _snprintf_s(setThreadCommand, _TRUNCATE, "%s%s%x", setThreadCommand, pOperation, processorNumber);
        bool isSet = false;
        int retryCounter = 0;
        RSP_Response_Packet replyType;
        unsigned lastGoodActiveCpu = m_lastKnownActiveCpu;
        m_lastKnownActiveCpu = processorNumber;

        do
        {
            std::string cmdResponse = ExecuteCommand(setThreadCommand);
    
            //  We should receive OK or ERR XXXX
            replyType = GetRspResponse(cmdResponse);
            if (replyType == RSP_OK)
            {
                m_lastKnownActiveCpu = processorNumber;
                isSet = true;
                break;
            }
        }
        while (IS_BAD_REPLY(replyType) && IS_RETRY_ALLOWED(++retryCounter));

        if (!isSet)
        {
            m_lastKnownActiveCpu = lastGoodActiveCpu;
        }
        return isSet;
    }

    //
    //  SetTextHandler  Stores the pointer to the trace/logging class (this module will own the pointer now).
    //
    void GdbSrvControllerImpl::SetTextHandler(_In_ IGdbSrvTextHandler * pHandler)
    {
        assert(pHandler != m_pTextHandler && pHandler != nullptr);
        delete m_pTextHandler;
        m_pTextHandler = pHandler;
    }

    //
    //  ExecuteCommandEx    Executes/Posts a GdbServer command.
    //
    //  Parameters:
    //  pCommand            Pointer to the command to be executed.
    //  isRspWaitNeeded     Flag tells if it the command has a response (the post command does not have to wait for response).
    //  stringSize          Size of the result string. Allows to control the maximum size of the string
    //                      in order to minimize the STL automatically resizing mechanism.
    //
    //  Return:
    //  The command response.
    //
    std::string GdbSrvControllerImpl::ExecuteCommandEx(_In_ LPCSTR pCommand, _In_ bool isRspWaitNeeded, _In_ size_t stringSize)
    {
        return GdbSrvControllerImpl::ExecuteCommandOnProcessor(pCommand, isRspWaitNeeded, stringSize, GetLastKnownActiveCpu());
    }

    //
    //  ExecuteCommandOnProcessor   Executes/Posts a GdbServer command on a paricular processor core.
    //
    //  Parameters:
    //  pCommand                    Pointer to the command to be executed.
    //  isRspWaitNeeded             Flag tells if it the command has a response (the post command does not have to wait for response).
    //  stringSize                  Size of the result string. Allows to control the maximum size of the string
    //                              in order to minimize the STL automatically resizing mechanism.
    //  processor                   Processor core to send the command.
    //
    //  Return:
    //  The command response.
    //
    std::string GdbSrvControllerImpl::ExecuteCommandOnProcessor(_In_ LPCSTR pCommand, _In_ bool isRspWaitNeeded, _In_ size_t stringSize,
                                                                _In_ unsigned processor)
    {
        if (pCommand == nullptr)
        {
            throw _com_error(E_POINTER);
        }

	    std::string result;
        if (result.max_size() < stringSize)
        {
            throw _com_error(E_INVALIDARG);
        }

        if (result.capacity() < stringSize)
        {
            result.reserve(stringSize);
        }

        if (m_pTextHandler != nullptr && m_displayCommands)
        {
            m_pTextHandler->HandleText(GdbSrvTextType::Command, pCommand, strlen(pCommand));
        }

        std::string command(pCommand);
        bool isDone = m_pRspClient->SendRspPacket(command, processor);
        if (isDone)
        {
            isDone = m_pRspClient->ReceiveRspPacket(result, processor, isRspWaitNeeded);
            if (!isDone)
            {
                //  A fatal error or a communication error ocurred
                m_pRspClient->HandleRspErrors(GdbSrvTextType::CommandError);
                throw _com_error(HRESULT_FROM_WIN32(m_pRspClient->GetRspLastError()));
            }
        }
        else
        {
            //  A fatal error or a communication error ocurred
            m_pRspClient->HandleRspErrors(GdbSrvTextType::CommandError);
            throw _com_error(HRESULT_FROM_WIN32(m_pRspClient->GetRspLastError()));
        }
    
        if (m_pTextHandler != nullptr && m_displayCommands)
        {
            const char * pResult = result.c_str(); 
            m_pTextHandler->HandleText(GdbSrvTextType::CommandOutput, pResult, strlen(pResult));
        }
        return result;
    }

    //
    //  ExecuteCommandOnProcessor   Executes/Posts a GdbServer command on a paricular processor core.
    //
    //  Parameters:
    //  pCommand                    Pointer to the command to be executed.
    //  isRspWaitNeeded             Flag tells if it the command has a response (the post command does not have to wait for response).
    //  stringSize                  Size of the result string. Allows to control the maximum size of the string
    //                              in order to minimize the STL automatically resizing mechanism.
    //
    //  Return:
    //  The command response.
    //
    //  Note.
    //  This function is mainly used for cases where we set the target to run and we expect a stop reply response.
    //
    std::string GdbSrvControllerImpl::ExecuteCommandOnMultiProcessors(_In_ LPCSTR pCommand, _In_ bool isRspWaitNeeded, _In_ size_t stringSize)
    {
        if (pCommand == nullptr)
        {
            throw _com_error(E_POINTER);
        }

        std::string result;
        if (result.max_size() < stringSize)
        {
            throw _com_error(E_INVALIDARG);
        }

        if (result.capacity() < stringSize)
        {
            result.reserve(stringSize);
        }

        if (m_pTextHandler != nullptr && m_displayCommands)
        {
            m_pTextHandler->HandleText(GdbSrvTextType::Command, pCommand, strlen(pCommand));
        }

        std::string command(pCommand);
        
        bool isDone = false;        
        unsigned numberOfCoreConnections = static_cast<unsigned>(m_pRspClient->GetNumberOfStreamConnections());

        for (unsigned core = 0; core < numberOfCoreConnections; ++core)
        {
            isDone = m_pRspClient->SendRspPacket(command, core);
            if (!isDone)
            {
                break;
            }
        }
        if (isDone)
        {
            bool IsPollingChannelMode = true;

            //  Start checking response from the last known processor core.
            unsigned core = GetLastKnownActiveCpu();
            for (;;)
            {
                isDone = m_pRspClient->ReceiveRspPacketEx(result, core, isRspWaitNeeded, IsPollingChannelMode, true);
                if (isDone || !IsPollingChannelMode)
                {
                    //  Set the core for the first received stop reply packet.
                    SetLastKnownActiveCpu(core);
                    //  Discard any pending response, but the current one as we received.
                    m_pRspClient->DiscardResponse(core);
                    break;
                }
                core = ++core % numberOfCoreConnections;
            }
        }
        else
        {
            //  A fatal error or a communication error ocurred
            m_pRspClient->HandleRspErrors(GdbSrvTextType::CommandError);
            throw _com_error(HRESULT_FROM_WIN32(m_pRspClient->GetRspLastError()));
        }
    
        if (m_pTextHandler != nullptr && m_displayCommands)
        {
            const char * pResult = result.c_str(); 
            m_pTextHandler->HandleText(GdbSrvTextType::CommandOutput, pResult, strlen(pResult));
        }
        return result;
    }

    //
    //  ExecuteCommand  Executes a GdbServer command
    //
    std::string GdbSrvControllerImpl::ExecuteCommand(_In_ LPCSTR pCommand)
    {
        return GdbSrvControllerImpl::ExecuteCommandEx(pCommand, true, 0);
    }

    //
    //  ParseRegisterValue  Converts an ascii hexadecimal 16 bytes register value to a 64 bit value.
    //
    static ULONGLONG GdbSrvControllerImpl::ParseRegisterValue(_In_ const std::string &stringValue)
    {
        ULARGE_INTEGER result;
        if (sscanf_s(stringValue.c_str(), "%I64x", &result.QuadPart) != 1)
        {
            throw _com_error(E_INVALIDARG);
        }
        return result.QuadPart;
    }

    //
    //  ParseRegisterValue32    Converts an ascii hexadecimal 8 bytes register value to a 32 bit value.
    //
    static DWORD GdbSrvControllerImpl::ParseRegisterValue32(_In_ const std::string &stringValue)
    {
        DWORD result;
        if (sscanf_s(stringValue.c_str(), "%I32x", &result) != 1)
        {
            throw _com_error(E_INVALIDARG);
        }
        return result;
    }

    //
    //  ParseRegisterVariableSize  Converts an ascii hexadecimal vector register value stream to a hex vector value.
    //
    static void GdbSrvControllerImpl::ParseRegisterVariableSize(_In_ const std::string &registerValue, 
                                                                _Out_writes_bytes_(registerAreaLength) BYTE pRegisterArea[],
                                                                _In_ int registerAreaLength)
    {
        assert(pRegisterArea != nullptr);
        int lenghtOfRegisterValue = static_cast<int>(registerValue.length());
        assert(lenghtOfRegisterValue <= (registerAreaLength * 2));

	    for (int pos = 0, index = 0; pos < lenghtOfRegisterValue && index < registerAreaLength; pos += 2, ++index)
        {
		    assert(index < registerAreaLength);
		    unsigned char highByte = ((AciiHexToNumber(registerValue[pos]) << 4) & 0xf0);
            pRegisterArea[index] = highByte | (AciiHexToNumber(registerValue[pos + 1]) & 0x0f);
        }
    }

    //
    //  QueryAllRegisters       Reads all general registers.
    //
    //  Request:
    //      'g'
    //
    //  Response
    //      'XXXXXXX...XXXXX'   This is a hex string where each byte will be represented by two hex digits.
    //                          The bytes are transmitted in target byte order. The size and the order are determined
    //                          by the target architecture.
    //      ‘E NN’              Error reading the registers.
    //
    //  Example:
    //  Get all registers (r command)
    //
    //  Request:
    //  $g#67
    //  
    //  Response:
    //  +
    //  $00000000b035d1ff000000007026d685e43ab0828c3bb08220118781000000007f586281460200000800000010000000230
    //  0000023000000300000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    //  000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007f020000000
    //  00000ffff000000000000000000000000000000000000#b6
    //  +
    //
    std::map<std::string, std::string> GdbSrvControllerImpl::QueryAllRegisters(_In_ unsigned processorNumber)
    {
        //  Set the processor core from where we will get the registers.
        if (!SetThreadCommand(processorNumber, "g"))
        {
            throw _com_error(E_FAIL);
        }

	    const char command[] = "g";
        std::string reply = ExecuteCommand(command);
        if (IsReplyError(reply))
        {
            throw _com_error(E_FAIL);
        }

        size_t numberOfElements = 0;
        const RegistersStruct * pRegisterArray = GetRegisterArrayTargetPtr(&numberOfElements);
        std::map<std::string, std::string> result;
        size_t startIdx = 0;
        size_t endIdx = 0;
        size_t replyLength = reply.length();
        for (size_t index = 0; index < numberOfElements && startIdx < replyLength; ++index, ++pRegisterArray)
        {
            //  Each response byte is transmitted as a two-digit hexadecimal ascii number in target order.
            endIdx = (pRegisterArray->registerSize << 1);
            //  Reverse the register value from target order to memory order.
            result[pRegisterArray->name] = ReverseRegValue(reply.substr(startIdx, endIdx));
            startIdx += endIdx;
        }
        return result;
    }

    //
    //  SetRegisters        Sets all general registers.  
    //
    //  Parameters:
    //  processorNumber     Processor core number.
    //  registerValues      Map containing the registers to be set.
    //  isRegisterValuePtr  Flag telling how the register map second element should be treated (pointer/value).
    //
    //  Request:
    //   https://sourceware.org/gdb/onlinedocs/gdb/Packets.html#Packets
    //
    //  Response:
    //  ‘OK’                Success.
    //  ‘E NN’              Error.
    //
    //  Example:
    //  Set the 'es' register ($es=0x24)
    //
    //  Request:
    //  $Pd=24000000#77
    //
    //  Response:    
    //  +
    //  OK
    //
    void GdbSrvControllerImpl::SetRegisters(_In_ unsigned processorNumber, 
                                            _In_ const std::map<std::string, AddressType> &registerValues,
                                            _In_ bool isRegisterValuePtr)
    {
        if (processorNumber != -1)
        {
            //  Set the processor core before setting the register values.
            if (!SetThreadCommand(processorNumber, "g"))
            {
                throw _com_error(E_FAIL);
            }
        }
        for (auto const& kv: registerValues)
        {
            const RegistersStruct * pRegEntry = FindRegisterEntry(kv.first);
            assert(pRegEntry != nullptr);
                
            std::string registerValue;
            const unsigned char * pRawRegBuffer = (isRegisterValuePtr) ? reinterpret_cast<const unsigned char *>(kv.second) : 
                                                                         reinterpret_cast<const unsigned char *>(&kv.second);
            for (size_t idx = 0; idx < pRegEntry->registerSize; ++idx)
            {
                registerValue.insert(registerValue.end(), 1, NumberToAciiHex(((pRawRegBuffer[idx] >> 4) & 0xf)));
                registerValue.insert(registerValue.end(), 1, NumberToAciiHex((pRawRegBuffer[idx] & 0xf)));
            }
            char command[512];
            _snprintf_s(command, _TRUNCATE, "P%s=%s", pRegEntry->nameOrder.c_str(), registerValue.c_str());

            std::string reply = ExecuteCommand(command);
            if (!IsReplyOK(reply))
            {
                throw _com_error(E_FAIL);
            }
        }
    }

    //
    //  QueryRegisters      Request reading a specific set of registers.
    //
    //  Parameters:
    //  processorNumber     Processor core number.
    //  registerNames       An array containing the list of register names to query.
    //  numberOfElements    Number of elements in the query array.
    //  
    //  Return:
    //  A map containing the register name and its hex-decimal ascii value.
    //
    //  Request:
    //  ‘p n’               Reads the register n (where the register number n is in hexadecimal ascii number).
    //               
    //  Response:
    //  ‘XX…’               Success.
    //  ‘E NN’              Error.
    //  ''                  Ignore
    //
    //  Example:
    //  Request the register xmm0 (rXi)
    //
    //  Request:
    //      $p20#d2
    //  Response:    
    //      +
    //      $7d7d7a453aa90f3e836ecd794962dc09#d5
    //      +
    //
    std::map<std::string, std::string> GdbSrvControllerImpl::QueryRegisters(_In_ unsigned processorNumber,
                                                                            _In_reads_(numberOfElements) const char * registerNames[],
                                                                            _In_ const size_t numberOfElements) 
    {
        if (processorNumber != -1)
        {
            //  Set the processor core before setting the register values.
            if (!SetThreadCommand(processorNumber, "g"))
            {
                throw _com_error(E_FAIL);
            }
        }
      
        std::map<std::string, std::string> result;
        for (size_t index = 0; index < numberOfElements; ++index)
        {
            std::string registerName(registerNames[index]); 
            const RegistersStruct * pRegEntry = FindRegisterEntry(registerName);
            assert(pRegEntry != nullptr);
                
            char command[512];
            _snprintf_s(command, _TRUNCATE, "p%s", pRegEntry->nameOrder.c_str());

            std::string reply = ExecuteCommand(command);
            if (IsReplyError(reply) || reply.empty())
            {
                throw _com_error(E_FAIL);
            }
            //  Process the register value returned by the GDBServer
            result[registerName] = ReverseRegValue(reply);
        }
        return result;
    }

    //
    //  ReadMemory      Reads length bytes of memory starting at address addr. 
    //
    //  Parameters:
    //  address         Memory address location to read.
    //  maxSize         Size of the memory chunk to read.
    //  memType         The memory class that will be accessed by the read operation.
    //
    //  Return:
    //  A simple buffer object containing the memory content.
    //
    //  Request:
    //      ‘m address,length’
    //
    //  Response:
    //      ‘XX...’     Memory contents.
    //                  Each byte is transmitted as a two-digit ascii hexadecimal number. 
    //                  The response may contain fewer bytes than requested if the server 
    //                  was able to read only part of the region of memory. 
    //      ‘E NN’      NN is the error number
    //
    //  Example:
    //  Request:
    //      $m81dce840,80#32
    //
    //  Response:
    //      +
    //      $8bc68b5424048b7424088b4c240cf36f8bf0c20c008d49000f32c3908b4c24048b4424088b54240c0f30c20c00ccccccccc
    //      ccccccccccccce8dd03fffffbf4c3cccccccccccccccc558bece81a11ffff84c0744de8cb45ffffe461eb0024fce661eb008
    //      b4d080bc97430b8cf3412002bd2f7f13d0000010072042ac0eb1e50b0#58
    //      +
    //
    SimpleCharBuffer GdbSrvControllerImpl::ReadMemory(_In_ AddressType address, _In_ size_t maxSize, 
                                                      _In_ const memoryAccessType memType)
    {
        SimpleCharBuffer result;
        //  The response is an Ascii hex string, so ensure some extra capacity
        //  in case that GdbServer replies with an unexpected stop reply packet.
        size_t maxReplyLength = (maxSize * 2) + 256;
        if (!result.TryEnsureCapacity(maxReplyLength))
        {
            throw _com_error(E_OUTOFMEMORY);
        }

        ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        size_t maxPacketLength = cfgData.GetMaxServerPacketLength();
        if (maxPacketLength != 0)
        {
            maxPacketLength = (maxPacketLength < maxSize) ? maxPacketLength : maxSize;
        }
        else
        {
            maxPacketLength = maxSize;
        }
        //  We need to support local configuration maximum packet size and packetsize that 
        //  the GdbServer dynamically supports by sending chunk of data until we reach the maximum requested size.
        while (maxSize != 0)
        {   
            size_t recvLength = 0;
            bool fError = false;
            size_t size = maxPacketLength; 
            //  We will send a sequence of ‘m addr,length’ request packets
            //  until we obtain the requested data length from the GdbServer.
            for (;;)
            {
                char memoryCmd[256] = {0}; 
                PCSTR pFormat = GetReadMemoryCmd(memType);
                sprintf_s(memoryCmd, _countof(memoryCmd), pFormat, address, size);
                std::string reply = ExecuteCommandEx(memoryCmd, true, maxReplyLength);

                size_t messageLength = reply.length();
                //  Is an empty response?
                if (messageLength == 0 && result.GetLength() == 0)
                {
                    if (GetThrowExceptionEnabled())
                    {
                        //  Yes, it is unacceptable            
                        throw _com_error(E_FAIL);
                    }
                    fError = true;
                    break;
                }
                //  Is an error response 'E NN'? 
                if (IsReplyError(reply))
                {
                    //  Yes, return the current stored length
                    //  and let the caller's handles the returned data.
                    fError = true;
                    // ... unless we didn't read anything, in which case fail
                    if (recvLength == 0 && GetThrowExceptionEnabled())
                    {
                        throw _com_error(E_FAIL);
                    }
                    break;
                }    

                //  Handle the received memory data
                for (size_t pos = 0; pos < messageLength ; pos += 2)
                {
                    std::string oneByteMemory = reply.substr(pos, 2);
                    int value = 0;
                    if (sscanf_s(oneByteMemory.c_str(), "%x", &value) != 1)
                    {
                        throw _com_error(E_FAIL);
                    }

                    result.SetLength(result.GetLength() + 1);
                    result[result.GetLength() - 1] = static_cast<char>(value);
                    recvLength++;
                }
                //  Are we done with the requested data?
                if (recvLength >= size || size == 0 || messageLength == 0)
                {
                    break;
                }
                //  Update the parameters for the next packet.
                address += recvLength;
                size -= recvLength;
            }
            if (fError)
            {
                break;
            }
            maxSize -= maxPacketLength;
            maxPacketLength = (maxPacketLength >= maxSize) ? maxSize : maxPacketLength; 
        }
        return result;
    }

    //
    //  WriteMemory     Writes length bytes of memory starting at address XX
    //                  The data is transmitted in ascii hexadecimal.
    //                  
    //  Parameters:
    //  address         Address location where we should write the data
    //  size            Size of the memory write.
    //  pRawBuffer      Pointer to the buffer that contains the data to write
    //  pdwBytesWritten Pointer to the variable containing how many bytes have been written.
    //  memType         The memory class that will be accessed by the write operation.
    //  fReportWriteError Flag indicates if the function needs to report packet errors. 
    //
    //  Return:
    //  true            if we succeeded.
    //  false           otherwise.
    //
    //  Request:
    //  ‘M address,length:XX...
    //   address:       The starting address
    //   length:        The number of bytes to write
    //   XX..           The data to write.
    //
    //  Response: 
    //  ‘OK’            Success.
    //  ‘E NN’          Error (includes the case where only part of the data was written). 
    //
    //  Example:
    //  Request:
    //      $M819e7d60,28:0f008025060003004c010c033101000000508081ffffffff18f29f81ffffffff30d0cf81ffffffff#3e
    //  Response:
    //      +
    //      $OK#9a
    //      +
    //
    bool GdbSrvControllerImpl::WriteMemory(_In_ AddressType address, _In_ size_t size, _In_ const void * pRawBuffer, 
                                           _Out_ DWORD * pdwBytesWritten, _In_ const memoryAccessType memType, 
                                           _In_ bool fReportWriteError)
    {
        assert(pRawBuffer != nullptr && pdwBytesWritten != nullptr && m_pRspClient != nullptr);

        bool isDone = false;
        bool isError = false;
        PacketConfig rspFeatures;
        //  Get the stored features
        m_pRspClient->GetRspPacketFeatures(&rspFeatures, PACKET_SIZE);
        //  Get the maximum packet size.
        size_t maxPacketSize = static_cast<size_t>(rspFeatures.featureDefaultValue);
        assert(maxPacketSize != 0);
        size_t packetSize = maxPacketSize = (maxPacketSize < size) ? maxPacketSize : size;
        const unsigned char * pRawDataBuffer = reinterpret_cast<const unsigned char *>(pRawBuffer);

        for (;;)
        {
            std::string dataBuffer;
            for (size_t idx = 0; idx < maxPacketSize; ++idx)
            {
                dataBuffer.insert(dataBuffer.end(), 1, NumberToAciiHex(((pRawDataBuffer[idx] >> 4) & 0xf)));
                dataBuffer.insert(dataBuffer.end(), 1, NumberToAciiHex((pRawDataBuffer[idx] & 0xf)));
            }

            char memoryAddrLength[128];
            bool isQ32GdbServerCmd = false;
            PCSTR pFormat = GetWriteMemoryCmd(memType, isQ32GdbServerCmd);

            sprintf_s(memoryAddrLength, _countof(memoryAddrLength), pFormat, address);
            char dataLength[128];
            sprintf_s(dataLength, _countof(dataLength), "%I64x", static_cast<ULONG64>(maxPacketSize));
			std::string command(memoryAddrLength);
			command += dataLength;
            if (isQ32GdbServerCmd)
            {
                command += ",";
            }
            else
            {
                command += ":";
            }
			command += dataBuffer.c_str();

            std::string reply = ExecuteCommand(command.c_str());

            //  We should receive 'OK' or 'EE NN' response.
            if (IsReplyError(reply))
            {
                isError = fReportWriteError;
                break;
            }
            if (packetSize >= size)
            {
                break;
            }
            pRawDataBuffer += maxPacketSize;
            size_t leftPacketSize = size - packetSize;
            maxPacketSize = (leftPacketSize < maxPacketSize) ? leftPacketSize : maxPacketSize;        
            packetSize += maxPacketSize;
        }

        if (packetSize == size && !isError)
        {
            *pdwBytesWritten = static_cast<DWORD>(size);
            isDone = true;
        }
        return isDone;
    }

    //
    //  GetProcessorCount   Get the number of processor cores in the Target.
    //                      This function relays on RSP query threads info packets
    //                      for retrieving the number of CPU cores as we debug the 
    //                      target in kernel mode then the thread abstraction is used 
    //                      to identify processor cores.
    //  Since the target may be too many threads running to fit into one reply packet,
    //  then the RSP protocol implements a sequence of query packets that may require
    //  more than one query/reply packet sequence in order to obtain the entire list of threads.
    //
    //  Request:
    //  The packet sequence contains two requests the first request packet is 'qfThreadInfo'
    //  for subsequent requests is used ‘qsThreadInfo’. 
    //  'qfThreadInfo'
    //  'qsThreadInfo'
    //  
    //  Response
    //  'm thread-id'   single thread
    //  'm thread-id, thread-id, ...,thread-id'
    //  'l' (lower case 'L') describes the end of the thread list
    //      The operation field depends on the next operation.
    //      'c' the next operation is a step or continue operation.
    //      'g' for other operations.
    //
    //  Example:
    //  One processor core case:
    //  Request:
    //      $qfThreadInfo#bb
    //      +
    //  Response:
    //      m1
    //      +
    //  Request:
    //      $qsThreadInfo#c8
    //      +
    //  Response:
    //      l
    //  
    //  Multi-thread case:
    //  Request:
    //      qfThreadInfo
    //      +
    //  Response:
    //      m1, m2,...m20
    //      +
    //  Request:
    //      qsThreadInfo
    //      +
    //  Response:
    //      m21...m40
    //      +
    //  Request:
    //      $qsThreadInfo#c8
    //      +
    //  Response:
    //      l
    //
    unsigned GdbSrvControllerImpl::GetProcessorCount()
    {
        //  Check if we have multi-core connection for each GdbServer instance
        //  if so, then we accept it as the number of processor cores
        if (m_cachedProcessorCount == 0)
	    {
            unsigned numberOfCoreConnections = static_cast<unsigned>(m_pRspClient->GetNumberOfStreamConnections());
            if (numberOfCoreConnections == 1)
            {
                const char reqfThreadInfo[] = "qfThreadInfo";
                std::string reply = ExecuteCommand(reqfThreadInfo);
                if (reply.empty())
                {
                    throw _com_error(E_FAIL);
                }
                
                if (m_ThreadStartIndex == -1 && reply[0] == 'm' && reply.length() > 1)
                {
                    m_ThreadStartIndex = AciiHexAFToNumber(reply[1]);
                }

                unsigned int countOfThreads = 0;
                do
                {
                    if (reply.find("m") != string::npos && reply.length() > 1)
                    {
                        //  We have a response to parse
                        countOfThreads += static_cast<int>(count(reply.cbegin(), reply.cend(), ','));
                        //  Request the subsequent query packet
                        const char reqSThreadInfo[] = "qsThreadInfo";
                        reply = ExecuteCommand(reqSThreadInfo);
                        countOfThreads++;
                    }
                }
                while(reply.find("l") == string::npos && countOfThreads != 0);
            
                m_cachedProcessorCount = (countOfThreads > 0) ? countOfThreads : 1;
            }
            else
            {
                m_cachedProcessorCount = numberOfCoreConnections;
            }
            m_cachedKPCRStartAddress.clear();
            for (unsigned i = 0; i < m_cachedProcessorCount; ++i)
            {
                m_cachedKPCRStartAddress.push_back(0); 
            }    
        }
	    return m_cachedProcessorCount;
    }

    //
    //  FindPcRegisterArrayEntry    Returns the Pc register array entry for the current architecture
    //
    //  Parameters:
    //  
    //  Return:
    //  The register entry for the PC register.
    //
    const RegistersStruct * FindPcRegisterArrayEntry()
    {
        const RegistersStruct * pRegEntry = nullptr;
        if (m_targetProcessorArch == X86_ARCH)
        {
           pRegEntry  = FindRegisterEntry("Eip");
        }
        else if (m_targetProcessorArch == AMD64_ARCH)
        {
            // !!! The amd64 code is not tested, so try enabling the below code !!!
            //  pRegisterArray = amd64RegisterArray;
            //  if (pNumberOfElements != nullptr)
            //  {
            //      *pNumberOfElements = MAX_REG_AMD64_NUMBER;
            //  }
            assert(false);
        }
        else if (m_targetProcessorArch == ARM32_ARCH || m_targetProcessorArch == ARM64_ARCH)
        {
           pRegEntry  = FindRegisterEntry("pc");
        }
        else
        {
            assert(false);
        }
        assert(pRegEntry != nullptr);
        return pRegEntry;
    }

    //
    //  FindPcAddressFromStopReply          Finds the current instruction address in the GdbServer stop reply 
    //                                      packet according to the current architecture
    //
    //  Parameters:
    //  cmdResponse     String with the stop response to parse
    //  pPcAddress      Pointer to the pc addrress value to return.
    //
    //  Return:
    //  true            Succeeded.
    //  false           Otherwise.    
    //  Indirectly return the pc address field in the stop reply packet structure  
    //
    bool GdbSrvControllerImpl::FindPcAddressFromStopReply(_In_ const std::string & cmdResponse,
                                                          _Out_ AddressType * pPcAddress)
    {
        assert(pPcAddress != nullptr);
        bool isFound = false;

        const RegistersStruct * pRegEntry = FindPcRegisterArrayEntry();
        std::string pcRegAddress = pRegEntry->nameOrder + ":";
        string::size_type pos = cmdResponse.find(pcRegAddress);
        if (pos != string::npos)
        {
            string::size_type regValueStartPos = pos + pcRegAddress.length();
            string::size_type pcEndPos = cmdResponse.find(";", regValueStartPos);
            if (pcEndPos != string::npos)
            {
                std::string pcAddress = cmdResponse.substr(regValueStartPos, pcEndPos - regValueStartPos);
                if (!pcAddress.empty())
                {
                    if (Is64BitArchitecture())
                    {
                        *pPcAddress = ParseRegisterValue(ReverseRegValue(pcAddress));
                    }
                    else
                    {
                        *pPcAddress = ParseRegisterValue32(ReverseRegValue(pcAddress));
                    }
                    isFound = true;    
                }
            }            
        }
        return isFound;
    }

    //
    //  HandleAsynchronousCommandResponse   Parses the GDbServer stop reply reason response used 
    //                                      for asynchronous commnands (like 'c','s', 0x03)..
    //
    //  Parameters:
    //  cmdResponse     String with the stop response to parse
    //  pRspPacket      Pointer to the output stop reply structure.
    //
    //  Return:
    //  true            If the response is not empty
    //  false           otherwise.
    //
    //  Note.
    //      The stop reply reason response format can be found here:
    //      https://sourceware.org/gdb/onlinedocs/gdb/Stop-Reply-Packets.html#Stop-Reply-Packets
    //
    bool GdbSrvControllerImpl::HandleAsynchronousCommandResponse(_In_ const std::string & cmdResponse,
                                                                 _Out_ StopReplyPacketStruct * pRspPacket)
    {
        assert(pRspPacket != nullptr);
        bool isParsed = false;

        if (!cmdResponse.empty())
        {
            memset(pRspPacket, 0x00, sizeof(StopReplyPacketStruct));
             
            string::size_type startPosition = cmdResponse.find("T");
            if (startPosition == string::npos)
            {
                startPosition = cmdResponse.find("S");
                pRspPacket->status.isSAAPacket = true;
            }
            else
            {
                pRspPacket->status.isTAAPacket = true;
            }
            if (startPosition != string::npos)
            {
                if (sscanf_s(&cmdResponse[startPosition + 1], "%2x", &pRspPacket->stopReason) != 1)
                {
                    pRspPacket->stopReason = TARGET_MARKER;
                }
            
                //  Extract the thread/processor number
                string::size_type pos = cmdResponse.find("thread:");
                if (pos != string::npos)
                {
                    pRspPacket->status.isThreadFound = true;
                    string::size_type regValueStartPos = pos + strlen("thread:");
                    string::size_type threadEndPos = cmdResponse.find(";", regValueStartPos);
                    if (threadEndPos != string::npos)
                    {
                        std::string processorNumber = cmdResponse.substr(regValueStartPos, threadEndPos - regValueStartPos);
                        if (sscanf_s(processorNumber.c_str(), "%x", &pRspPacket->processorNumber) != 1)
                        {
                            pRspPacket->processorNumber = static_cast<ULONG>(-1);    
                        }
                    }
                }

                //  Extract the current instruction address
                if (FindPcAddressFromStopReply(cmdResponse, &pRspPacket->currentAddress))
                {
                    pRspPacket->status.isPcRegFound = true;                    
                }
                else
                {
                    //  Try to find if this has been a power down or target running packet
                    if (pRspPacket->status.isSAAPacket)
                    {
                        pRspPacket->status.isPowerDown = (cmdResponse.find("S00") != string::npos) ? true : false;
                    }
                }
            }
            else if (cmdResponse[0] == 'W')
            {
                pRspPacket->stopReason = TARGET_PROCESS_EXIT;
            }
            else if (cmdResponse.find("OK") != string::npos)
            {
                pRspPacket->status.isCoreRunning = true;
            }
            isParsed = true;
        }
        return isParsed;
    }

    //
    //  GetKpcrOffset   Get the KPCR base address for the passed in processor.
    //
    //  Parameters:
    //  processorNumber Core processor number
    //
    //  Return:
    //  The processor kpcr address.
    //
    //  Note.
    //  This address is the start of the processor block for the passed in processor.
    //  This address is stored in the OS KPCR processor array.
    //
    AddressType GdbSrvControllerImpl::GetKpcrOffset(_In_ unsigned processorNumber) 
    {
        assert(processorNumber <= m_cachedKPCRStartAddress.size());
        
        return m_cachedKPCRStartAddress[processorNumber];
    }

    //
    //  SetKpcrOffset   Set the KPCR base address value for the passed in processor.
    //
    //  Parameters:
    //  processorNumber Core processor number
    //  kpcrOffset      KPCR base address value to set
    //
    //  Return:
    //  Nothing.
    //
    //
    void GdbSrvControllerImpl::SetKpcrOffset(_In_ unsigned processorNumber, _In_ AddressType kpcrOffset) 
    {
        assert(processorNumber <= m_cachedKPCRStartAddress.size());

        m_cachedKPCRStartAddress[processorNumber] = kpcrOffset;
    }

    //
    //  IsReplyOK   Check for the OK sub-string in the GDbServer response  
    //  
    //  Parameters:
    //  reply       Response to check
    //
    //  Return:
    //  true        If the response contains the OK sub-string.
    //  false       Otherwise.
    //
    inline bool GdbSrvControllerImpl::IsReplyOK(_In_ const std::string & reply)
    {
        bool isOk = false;
        //  Is it an OK response
        if (reply.find("OK") != string::npos && reply.length() == 2)
        {
            isOk = true;
        }
        return isOk;
    }

    //
    //  GetRspResponse  Parse the GdbServer response and set the output type constant
    //                  according to the GdbServer type of the response content.
    //  
    //  Parameters:
    //  reply           GdbServer response to check
    //
    //  Return:
    //  The type of the response packet.
    //
    inline RSP_Response_Packet GdbSrvControllerImpl::GetRspResponse(_In_ const std::string & reply)
    {
        RSP_Response_Packet response = RSP_ERROR;

        size_t replyLength = reply.length();

        //  Is it an OK response?
        if (reply.find("OK") != string::npos && replyLength == 2)
        {
            response = RSP_OK;
        }
        else if (IsStopReply(reply))
        {
            response = RSP_STOP_REPLY;    
        }
        else if (replyLength == 0)
        {
            //  Ignore this packet as it has length zero
            response = RSP_IGNORE_REPLY;
        }
        return response;
    }

    //
    //  IsReplyError    Check for the ERR sub-string in the GDbServer response  
    //  
    //  Parameters:
    //  reply           Response to check
    //
    //  Return:
    //  true            If the response contains the ERR sub-string
    //  false           Otherwise.
    //
    inline bool GdbSrvControllerImpl::IsReplyError(_In_ const std::string & reply)
    {
        bool isError = false;

        //  Is it an OK response
        if (reply[0] == 'E' && reply.length() > 0)
        {
            isError = true;
        }
        return isError;
    }

    //
    //  IsStopReply     Check for the stop reply reason GdbServer response  
    //  
    //  Parameters:
    //  reply           Response to check
    //
    //  Return:
    //  true            If the response contains the stop reply reason pattern:
    //                  T05thread:00000001;08:......
    //  false           if the response is not a stop reply reason.
    //
    inline bool GdbSrvControllerImpl::IsStopReply(_In_ const std::string & cmdResponse)
    {
        bool isStopPaket = false;

        string::size_type startPosition = cmdResponse.find("T");
        if (startPosition == string::npos)
        {
            startPosition = cmdResponse.find("S");
        }
        if (startPosition != string::npos)
        {
            //  Find the thread/processor pattern
            string::size_type pos = cmdResponse.find("thread:");
             if (pos != string::npos)
            {
                string::size_type regValueStartPos = pos + strlen("thread:");
                string::size_type threadEndPos = cmdResponse.find(";", regValueStartPos);
                if (threadEndPos != string::npos)
                {
                    const RegistersStruct * pRegEntry = FindPcRegisterArrayEntry();
                    assert(pRegEntry != nullptr);
                    std::string pcRegAddress = pRegEntry->nameOrder + ":";
                    pos = cmdResponse.find(pcRegAddress);
                    if (pos != string::npos)
                    {
                        isStopPaket = true;
                    }
                }
            }
        }
        return isStopPaket;
    }

    inline void GdbSrvControllerImpl::SetTargetArchitecture(_In_ TargetArchitecture targetArch) 
    {
        m_targetProcessorArch = targetArch;
    }

    inline TargetArchitecture GdbSrvControllerImpl::GetTargetArchitecture() {return m_targetProcessorArch;}

    inline unsigned GdbSrvControllerImpl::GetLastKnownActiveCpu() {return m_lastKnownActiveCpu;}
    inline void GdbSrvControllerImpl::SetLastKnownActiveCpu(_In_ unsigned cpu) {m_lastKnownActiveCpu = cpu;}

    inline unsigned GdbSrvControllerImpl::GetNumberOfRspConnections() 
    {
        assert(m_pRspClient != nullptr);
        return static_cast<unsigned>(m_pRspClient->GetNumberOfStreamConnections());
    }

    inline void GdbSrvControllerImpl::DisplayLogEntry(_In_reads_bytes_(readSize) const char * pBuffer, _In_ size_t readSize)
    {
        DisplayTextData(pBuffer, readSize, GdbSrvTextType::CommandError, m_pTextHandler);
    }

    void GdbSrvControllerImpl::CreateNeonRegisterNameArray(_In_ const std::string & registerName,
                                                           _Out_writes_bytes_(numberOfRegArrayElem) std::unique_ptr<char> pRegNameArray[],
                                                           _In_ size_t numberOfRegArrayElem)
    {
        assert(pRegNameArray != nullptr);

        size_t numberOfRegisters = 0;
        const RegistersStruct * pRegEntry = FindRegisterEntryAndNumberOfElements(registerName, numberOfRegisters);
        if (pRegEntry == nullptr)
        {
            throw _com_error(E_POINTER);
        }
        assert(numberOfRegArrayElem < numberOfRegisters);

        for (size_t index = 0; index < numberOfRegArrayElem; ++index, ++pRegEntry)
        {
            pRegNameArray[index] = std::unique_ptr <char>(new (std::nothrow) char[C_MAX_REGISTER_NAME_ARRAY_ELEM]);
            if (pRegNameArray[index] == nullptr)
            {
                throw _com_error(E_OUTOFMEMORY);
            }
            FillNeonRegisterNameArrayEntry(pRegEntry, pRegNameArray[index].get(), C_MAX_REGISTER_NAME_ARRAY_ELEM);
        }    
    }

    inline int GdbSrvControllerImpl::GetFirstThreadIndex() {return m_ThreadStartIndex;}

    void GdbSrvControllerImpl::GetMemoryPacketType(_In_ DWORD64 cpsrRegValue, _Out_ memoryAccessType * pMemType)
    {
        assert(pMemType != nullptr);    
        
        *pMemType = {0};
        if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM) && (m_targetProcessorArch == ARM64_ARCH))
        {
            //  Check the current target CPU mode stored in the CPSR register in order to set the correct memory type.
            if (cpsrRegValue != 0)
            {
                switch (cpsrRegValue & 0xf)
                {
                //  NT space
                case C_EL1HCPSRREG :
                case C_EL1TCPSRREG :
                    {
                        pMemType->isSupervisor = true;
                    }
                    break;

                //  Hypervisor space
                case C_EL2TCPSRREG :
                case C_EL2HCPSRREG :
                    {
                        pMemType->isHypervisor = true;
                    }
                    break;

                default:
                    {
                        // Force to use a supervisor mode packet as it should never fail the memory read,
                        // other than hypervisor or secure mode
                        pMemType->isSupervisor = true;
                    }
                }
            }
        }
    }

    inline bool GdbSrvControllerImpl::GetThrowExceptionEnabled()
    {
        return m_IsThrowExceptionEnabled;
    }

    bool GdbSrvControllerImpl::Is64BitArchitecture()
    {
        return m_targetProcessorArch == ARM64_ARCH || m_targetProcessorArch == AMD64_ARCH;
    }

    HRESULT GdbSrvControllerImpl::ReadMsrRegister(_In_ DWORD dwProcessorNumber, _In_ DWORD dwRegisterIndex, _Out_ ULONG64 *pValue)
    {
        HRESULT hr = E_FAIL;
        const char * statusRegister[] = { nullptr };
        statusRegister[0] = GetProcessorStatusRegByArch(m_targetProcessorArch);
        if (statusRegister[0] != nullptr)
        {
            std::map<std::string, std::string> cpsrRegisterValue = QueryRegisters(dwProcessorNumber, statusRegister, ARRAYSIZE(statusRegister));
            ULONGLONG processorStatusRegValue = ParseRegisterValue(cpsrRegisterValue[statusRegister[0]]);

            memoryAccessType memType = {0};
            hr = SetSpecialMemoryPacketType(m_targetProcessorArch, processorStatusRegValue, &memType);
            if (hr == S_OK)
            {
                assert(memType.isSpecialRegs);
                *pValue = 0;
                SimpleCharBuffer buffer = ReadMemory(dwRegisterIndex, sizeof(*pValue), memType);
                size_t copySize = std::min<size_t>(buffer.GetLength(), sizeof(*pValue));
                memcpy(pValue, buffer.GetInternalBuffer(), copySize);
            }
        }
        return hr;
    }

    HRESULT GdbSrvControllerImpl::WriteMsrRegister(_In_ DWORD dwProcessorNumber, _In_ DWORD dwRegisterIndex, _In_ ULONG64 value)
    {
        HRESULT hr = E_FAIL;
        const char * statusRegister[] = { nullptr };
        statusRegister[0] = GetProcessorStatusRegByArch(m_targetProcessorArch);
        if (statusRegister[0] != nullptr)
        {
            std::map<std::string, std::string> cpsrRegisterValue = QueryRegisters(dwProcessorNumber, statusRegister, ARRAYSIZE(statusRegister));
            ULONGLONG processorStatusRegValue = ParseRegisterValue(cpsrRegisterValue[statusRegister[0]]);

            memoryAccessType memType = {0};
            hr = SetSpecialMemoryPacketType(m_targetProcessorArch, processorStatusRegValue, &memType);
            if (hr == S_OK)
            {
                assert(memType.isSpecialRegs);
                DWORD bytesWritten = 0;
                bool isMemoryWritten = WriteMemory(dwRegisterIndex, sizeof(value), &value, &bytesWritten, memType, true);
                hr = (isMemoryWritten && (bytesWritten != 0)) ? S_OK : E_FAIL;
            }
        }
        return hr;
    }

    private:
	IGdbSrvTextHandler * m_pTextHandler;
	unsigned m_cachedProcessorCount;
    unsigned m_lastKnownActiveCpu;
    TARGET_HALTED m_TargetHaltReason;
    bool m_displayCommands;
    TargetArchitecture m_targetProcessorArch;
    std::vector<AddressType> m_cachedKPCRStartAddress;
    int m_ThreadStartIndex;
    std::unique_ptr <GdbSrvRspClient<TcpConnectorStream>> m_pRspClient;
    typedef std::function<bool(const std::wstring &connectionStr, unsigned)> ExdiFunctions;
	std::map<std::wstring, ExdiFunctions> m_exdiFunctions;
    bool m_IsThrowExceptionEnabled;

    inline void GdbSrvControllerImpl::SetExdiFunctions(_In_ PCWSTR pFunctionText, _In_ ExdiFunctions function)
    {
        m_exdiFunctions[std::wstring(pFunctionText)] = function;
    }

    //  Get a pointer to the target architecture array.
    const RegistersStruct * GdbSrvControllerImpl::GetRegisterArrayTargetPtr(_Out_opt_ size_t * pNumberOfElements)
    {
        const RegistersStruct * pRegisterArray = nullptr;

        if (m_targetProcessorArch == X86_ARCH)
        {
            pRegisterArray = x86RegisterArray;
            if (pNumberOfElements != nullptr)
            {
                *pNumberOfElements = MAX_REG_X86_NUMBER;
            }
        }
        else if (m_targetProcessorArch == AMD64_ARCH)
        {
            // !!! The amd64 code is not tested, so try enabling the below code !!!
            //  pRegisterArray = amd64RegisterArray;
            //  if (pNumberOfElements != nullptr)
            //  {
            //      *pNumberOfElements = MAX_REG_AMD64_NUMBER;
            //  }
            assert(false);
        }
        else if (m_targetProcessorArch == ARM32_ARCH)
        {
            // If we want to add another register mapping table (like arm32RegisterArray_Qemu)
            // then we will need to add a configurable setting in the sample config.xml file.
            pRegisterArray = arm32RegisterArray;
            if (pNumberOfElements != nullptr)
            {
                *pNumberOfElements = MAX_REG_ARM32_NUMBER;
            }
        }
        else if (m_targetProcessorArch == ARM64_ARCH)
        {
            pRegisterArray = arm64RegisterArray;
            if (pNumberOfElements != nullptr)
            {
                *pNumberOfElements = MAX_REG_ARM64_NUMBER;
            }
        }
        return pRegisterArray;
    }

    const RegistersStruct * GdbSrvControllerImpl::FindRegisterEntry(_In_ const std::string regName)
    {
        size_t numberOfElements;
        const RegistersStruct * pRegisterArray = GetRegisterArrayTargetPtr(&numberOfElements);
        assert(pRegisterArray != nullptr);

        for (size_t idx = 0; idx < numberOfElements; ++idx, ++pRegisterArray)
        {
            if (pRegisterArray->name == regName)
            {
                return pRegisterArray;
            }
        }
        return nullptr;    
    }

    bool CheckProcessorCoreNumber(_In_ unsigned core)
    {
        bool isChecked = true;

        ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        std::vector<std::wstring> coreConnections;
        cfgData.GetGdbServerConnectionParameters(coreConnections);        
        bool isAllCores = (core == C_ALLCORES) ? true : false;
        if (isAllCores)
        {
            if (coreConnections.size() != GetNumberOfRspConnections())
            {
                throw _com_error(E_ABORT);
            }
        }
        else 
        {
            if (core > coreConnections.size())
            {
                isChecked = false;
            }
        }
        return isChecked;
    }

    inline const std::wstring GetCoreConnectionString(_In_ unsigned core)
    {
        ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        std::vector<std::wstring> coreConnections;
        cfgData.GetGdbServerConnectionParameters(coreConnections);
        return coreConnections[core];        
    }

    const RegistersStruct * GdbSrvControllerImpl::FindRegisterEntryAndNumberOfElements(_In_ const std::string regName,
                                                                                       _Out_ size_t &numberOfElements)
    {
        const RegistersStruct * pRegisterArray = GetRegisterArrayTargetPtr(&numberOfElements);
        if (pRegisterArray != nullptr)
        {
            return FindRegisterEntry(regName);
        }
        return pRegisterArray;
    }

    void GdbSrvControllerImpl::FillNeonRegisterNameArrayEntry(_In_ const RegistersStruct * pRegEntry, 
                                                              _Out_writes_bytes_(lengthArrayElem) char * pRegNameArray,
                                                              _In_ size_t lengthArrayElem)
    {
        assert(pRegEntry != nullptr && pRegNameArray != nullptr);
        assert(strlen(pRegEntry->name.c_str()) < lengthArrayElem);

        if (strcpy_s(pRegNameArray, lengthArrayElem, pRegEntry->name.c_str()) != 0)
        {
            throw _com_error(E_FAIL);
        }
    }

    PCSTR GetReadMemoryCmd(_In_ memoryAccessType memType)
    {
        PCSTR pFormat = nullptr;
        
        if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM) && memType.isPhysical)
        {
             pFormat = (Is64BitArchitecture()) ? "qtrace32.memory:a,%I64x,%x" : "qtrace32.memory:a,%x,%x";
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM) && memType.isSupervisor)
        {
             pFormat = (Is64BitArchitecture()) ? "qtrace32.memory:s,%I64x,%x" : "qtrace32.memory:s,%x,%x";
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM) && memType.isHypervisor)
        {
             pFormat = (Is64BitArchitecture()) ? "qtrace32.memory:h,%I64x,%x" : "qtrace32.memory:h,%x,%x";
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM) && memType.isSpecialRegs)
        {
            if (m_targetProcessorArch == ARM64_ARCH)
            {
                pFormat = "qtrace32.memory:SPR,%x,%x";
            }
            else if (m_targetProcessorArch == ARM32_ARCH)
            {
                pFormat = "qtrace32.memory:C15,%x,%x";
            }
            else
            {
                assert(false);
            }
        }
        else
        {
             pFormat = Is64BitArchitecture() ? "m%I64x,%x" : "m%x,%x";
        }

        return pFormat;
    }

    PCSTR GetWriteMemoryCmd(_In_ memoryAccessType const memType, _Out_ bool & isQ32GdbServerCmd)
    {
        PCSTR pFormat = nullptr;
        
        isQ32GdbServerCmd = m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM);
        if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM) && memType.isPhysical)
        {
             pFormat = (Is64BitArchitecture()) ? "Qtrace32.memory:a,%I64x," : "Qtrace32.memory:a,%x,";
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM) && memType.isSupervisor)
        {
             pFormat = (Is64BitArchitecture()) ? "Qtrace32.memory:s,%I64x," : "Qtrace32.memory:s,%x,";
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM) && memType.isHypervisor)
        {
             pFormat = (Is64BitArchitecture()) ? "Qtrace32.memory:h,%I64x," : "Qtrace32.memory:h,%x,";
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM) && memType.isSpecialRegs)
        {
            if (m_targetProcessorArch == ARM64_ARCH)
            {
                pFormat = "Qtrace32.memory:SPR,%x,";
            }
            else if (m_targetProcessorArch == ARM32_ARCH)
            {
                pFormat = "Qtrace32.memory:C15,%x,";
            }
            else
            {
                assert(false);
            }
        }
        else
        {
             pFormat = Is64BitArchitecture() ? "M%I64x," : "M%x,";
             isQ32GdbServerCmd = false;
        }

        return pFormat;
    }
};

//=============================================================================
// Public function definitions
//=============================================================================
GdbSrvController::GdbSrvController(_In_ const std::vector<std::wstring> &coreConnectionParameters)
{
    assert(!coreConnectionParameters.empty());

    m_pGdbSrvControllerImpl = std::unique_ptr<GdbSrvControllerImpl>(new (std::nothrow) GdbSrvControllerImpl(coreConnectionParameters));
    assert(m_pGdbSrvControllerImpl != nullptr);
}

bool GdbSrvController::ConnectGdbSrv()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->ConnectGdbSrv();
}

void GdbSrvController::ShutdownGdbSrv()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    m_pGdbSrvControllerImpl->ShutdownGdbSrv();
}

bool GdbSrvController::ConfigureGdbSrvCommSession(_In_ bool fDisplayCommData, _In_ int core)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->ConfigureGdbSrvCommSession(fDisplayCommData, static_cast<unsigned>(core));
}

bool GdbSrvController::RestartGdbSrvTarget()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->RestartGdbSrvTarget();
}

bool GdbSrvController::CheckGdbSrvAlive(_Out_ HRESULT & error)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->CheckGdbSrvAlive(error);    
}

bool GdbSrvController::ReqGdbServerSupportedFeatures()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->ReqGdbServerSupportedFeatures();
}

TARGET_HALTED GdbSrvController::ReportReasonTargetHalted(_Out_ StopReplyPacketStruct * pStopReply)
{
    assert(pStopReply != nullptr && m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->ReportReasonTargetHalted(pStopReply);
}

bool GdbSrvController::RequestTIB()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->RequestTIB();
}

bool GdbSrvController::IsTargetHalted()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->IsTargetHalted();    
}

bool GdbSrvController::InterruptTarget()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->InterruptTarget();    
}

GdbSrvController::~GdbSrvController()
{
}

bool GdbSrvController::SetThreadCommand(_In_ unsigned processorNumber, _In_ const char * pOperation)
{
    assert(m_pGdbSrvControllerImpl != nullptr && pOperation != nullptr);
    return m_pGdbSrvControllerImpl->SetThreadCommand(processorNumber, pOperation);    
}

void GdbSrvController::SetTextHandler(_In_ IGdbSrvTextHandler * pHandler)
{
    assert(m_pGdbSrvControllerImpl != nullptr && pHandler != nullptr);
    m_pGdbSrvControllerImpl->SetTextHandler(pHandler);
}

std::string GdbSrvController::ExecuteCommandOnProcessor(_In_ LPCSTR pCommand, _In_ bool isRspWaitNeeded, 
                                                        _In_ size_t stringSize, _In_ unsigned processor)
{
    assert(m_pGdbSrvControllerImpl != nullptr && pCommand != nullptr);
    return m_pGdbSrvControllerImpl->ExecuteCommandOnProcessor(pCommand, isRspWaitNeeded, stringSize, processor);
}

std::string GdbSrvController::ExecuteCommandEx(_In_ LPCSTR pCommand, _In_ bool isRspWaitNeeded, _In_ size_t stringSize)
{
    assert(m_pGdbSrvControllerImpl != nullptr && pCommand != nullptr);
    return m_pGdbSrvControllerImpl->ExecuteCommandEx(pCommand, isRspWaitNeeded, stringSize);
}

std::string GdbSrvController::ExecuteCommand(_In_ LPCSTR pCommand)
{
    assert(m_pGdbSrvControllerImpl != nullptr && pCommand != nullptr);
    return m_pGdbSrvControllerImpl->ExecuteCommand(pCommand);
}

ULONGLONG GdbSrvController::ParseRegisterValue(_In_ const std::string &stringValue)
{
    return GdbSrvControllerImpl::ParseRegisterValue(stringValue);
}

DWORD GdbSrvController::ParseRegisterValue32(_In_ const std::string &stringValue)
{
    return GdbSrvControllerImpl::ParseRegisterValue32(stringValue);
}

void GdbSrvController::ParseRegisterVariableSize(_In_ const std::string &registerValue, 
                                                 _Out_writes_bytes_(registerAreaLength) BYTE pRegisterArea[],
                                                 _In_ int registerAreaLength)
{
    GdbSrvControllerImpl::ParseRegisterVariableSize(registerValue, pRegisterArea, registerAreaLength);
}

std::map<std::string, std::string> GdbSrvController::QueryAllRegisters(_In_ unsigned processorNumber)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->QueryAllRegisters(processorNumber);
}

void GdbSrvController::SetRegisters(_In_ unsigned processorNumber, 
                                    _In_ const std::map<std::string, AddressType> &registerValues,
                                    _In_ bool isRegisterValuePtr)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->SetRegisters(processorNumber, registerValues, isRegisterValuePtr);
}

std::map<std::string, std::string> GdbSrvController::QueryRegisters(_In_ unsigned processorNumber,
                                                                    _In_ const char * registerNames[],
                                                                    _In_ const size_t numberOfElements) 
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->QueryRegisters(processorNumber, registerNames, numberOfElements);
}

SimpleCharBuffer GdbSrvController::ReadMemory(_In_ AddressType address, _In_ size_t size, _In_ const memoryAccessType memType)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->ReadMemory(address, size, memType);
}

bool GdbSrvController::WriteMemory(_In_ AddressType address, _In_ size_t size, _In_ const void * pRawBuffer, 
                                   _Out_ DWORD * pdwBytesWritten, _In_ const memoryAccessType memType)
{
    assert(m_pGdbSrvControllerImpl != nullptr && pRawBuffer != nullptr && pdwBytesWritten != nullptr);
    return m_pGdbSrvControllerImpl->WriteMemory(address, size, pRawBuffer, pdwBytesWritten, memType, false);
}

unsigned GdbSrvController::GetProcessorCount()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetProcessorCount();
}

bool GdbSrvController::HandleAsynchronousCommandResponse(_In_ const std::string & cmdResponse,
                                                         _Out_ StopReplyPacketStruct * pRspPacket)
{
    assert(m_pGdbSrvControllerImpl != nullptr && pRspPacket != nullptr);
    return m_pGdbSrvControllerImpl->HandleAsynchronousCommandResponse(cmdResponse, pRspPacket);
}

bool GdbSrvController::IsReplyOK(_In_ const std::string & reply)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->IsReplyOK(reply);
}

RSP_Response_Packet GdbSrvController::GetRspResponse(_In_ const std::string & reply)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetRspResponse(reply);
}

bool GdbSrvController::IsReplyError(_In_ const std::string & reply)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->IsReplyError(reply);
}

bool GdbSrvController::IsStopReply(_In_ const std::string & reply)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->IsStopReply(reply);
}

void GdbSrvController::SetTargetArchitecture(_In_ TargetArchitecture targetArch) 
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    m_pGdbSrvControllerImpl->SetTargetArchitecture(targetArch);
}

TargetArchitecture GdbSrvController::GetTargetArchitecture()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetTargetArchitecture();
}

unsigned GdbSrvController::GetLastKnownActiveCpu()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetLastKnownActiveCpu();
}

void GdbSrvController::SetLastKnownActiveCpu(_In_ unsigned cpu)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    m_pGdbSrvControllerImpl->SetLastKnownActiveCpu(cpu);
}

AddressType GdbSrvController::GetKpcrOffset(_In_ unsigned processorNumber)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetKpcrOffset(processorNumber);
}

void GdbSrvController::SetKpcrOffset(_In_ unsigned processorNumber, _In_ AddressType kpcrOffset)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    m_pGdbSrvControllerImpl->SetKpcrOffset(processorNumber, kpcrOffset);
}

unsigned GdbSrvController::GetNumberOfRspConnections()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetNumberOfRspConnections();
}

std::string GdbSrvController::ExecuteCommandOnMultiProcessors(_In_ LPCSTR pCommand, _In_ bool isRspWaitNeeded, _In_ size_t stringSize)
{
    assert(m_pGdbSrvControllerImpl != nullptr && pCommand != nullptr);
    return m_pGdbSrvControllerImpl->ExecuteCommandOnMultiProcessors(pCommand, isRspWaitNeeded, stringSize);
}

void GdbSrvController::DisplayLogEntry(_In_reads_bytes_(readSize) const char * pBuffer, _In_ size_t readSize)
{
    assert(m_pGdbSrvControllerImpl != nullptr);    
    m_pGdbSrvControllerImpl->DisplayLogEntry(pBuffer, readSize);
}

bool GdbSrvController::ExecuteExdiFunction(_In_ unsigned dwProcessorNumber, _In_ LPCWSTR pFunctionToExecute)
{
    assert(m_pGdbSrvControllerImpl != nullptr);    
    return m_pGdbSrvControllerImpl->ExecuteExdiFunction(dwProcessorNumber, pFunctionToExecute);
}

SimpleCharBuffer GdbSrvController::ExecuteExdiGdbSrvMonitor(_In_ unsigned dwProcessorNumber, _In_ LPCWSTR pFunctionToExecute)
{
    assert(m_pGdbSrvControllerImpl != nullptr);    
    return m_pGdbSrvControllerImpl->ExecuteExdiGdbSrvMonitor(dwProcessorNumber, pFunctionToExecute);
}

void GdbSrvController::CreateNeonRegisterNameArray(_In_ const std::string & registerName,
                                                   _Out_writes_bytes_(numberOfRegArrayElem) std::unique_ptr<char> pRegNameArray[],
                                                   _In_ size_t numberOfRegArrayElem)
{
    assert(m_pGdbSrvControllerImpl != nullptr);    
    m_pGdbSrvControllerImpl->CreateNeonRegisterNameArray(registerName, pRegNameArray, numberOfRegArrayElem);
}

int GdbSrvController::GetFirstThreadIndex()
{
    assert(m_pGdbSrvControllerImpl != nullptr);    
    return m_pGdbSrvControllerImpl->GetFirstThreadIndex();
}

void GdbSrvController::GetMemoryPacketType(_In_ DWORD64 cpsrRegValue, _Out_ memoryAccessType * pMemType)
{
    assert(m_pGdbSrvControllerImpl != nullptr);    
    return m_pGdbSrvControllerImpl->GetMemoryPacketType(cpsrRegValue, pMemType);
}

bool GdbSrvController::Is64BitArchitecture()
{
    assert(m_pGdbSrvControllerImpl != nullptr);    
    return m_pGdbSrvControllerImpl->Is64BitArchitecture();
}

HRESULT GdbSrvController::ReadMsrRegister(_In_ DWORD dwProcessorNumber, _In_ DWORD dwRegisterIndex, _Out_ ULONG64 * pValue)
{
    assert(m_pGdbSrvControllerImpl != nullptr);    
    return m_pGdbSrvControllerImpl->ReadMsrRegister(dwProcessorNumber, dwRegisterIndex, pValue);
}

HRESULT GdbSrvController::WriteMsrRegister(_In_ DWORD dwProcessorNumber, _In_ DWORD dwRegisterIndex, _In_ ULONG64 value)
{
    assert(m_pGdbSrvControllerImpl != nullptr);    
    return m_pGdbSrvControllerImpl->WriteMsrRegister(dwProcessorNumber, dwRegisterIndex, value);
}
