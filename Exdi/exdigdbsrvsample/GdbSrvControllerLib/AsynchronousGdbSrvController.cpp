//----------------------------------------------------------------------------
//
//  AsynchronousGdbSrvController.cpp
//  This modules handles the asynchronous commands continue:'g',steps ('p'/'t')
//  and code/data breakpoint commands.
//  The continue/step commands will start a separate thread for sending the command and 
//  receiving the command response. The asynchronous commands are tracked
//  differently because they require the dbgeng notification mechanism.
//  Please see the readme.doc for more info.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "CommandLogger.h"
#include "AsynchronousGdbSrvController.h"

#include "cfgExdiGdbSrvHelper.h"
#include "GdbSrvControllerLib.h"
#include "ExceptionHelpers.h"
#include "HandleHelpers.h"
#include <string>

using namespace GdbSrvControllerLib;

//
//  GetDataAccessBreakPointCommand  This function returns the data access breakpoint command that
//                                  will be sent to the GdbServer.
//
//  Parameters:
//  dataAccessType                  Indicates the type of the data break point (break on Write/Read/Access:Write/Read memory)
//  fInsertCmd                      Flag to indicate if the command will add or remove a data breakpoint.
//                                  if the flag is true then it will insert a new breakpoint, if false then it'll delete a break point.
//
//  Return:
//  A pointer to the data breakpoint command. This is independent of the architecture type.
//
//
const char * GetDataAccessBreakPointCommand(_In_ DATA_ACCESS_TYPE dataAccessType, _In_ bool fInsertCmd)
{
    char * pCommandType = nullptr;
    
    if (dataAccessType == daExecution)
    {
        if (fInsertCmd)
        {
            pCommandType = "Z1";
        }
        else
        {
            pCommandType = "z1";
        }
    }
    else if (dataAccessType == daWrite)
    {
        if (fInsertCmd)
        {
            pCommandType = "Z2";
        }
        else
        {
            pCommandType = "z2";
        }
    }
    else if (dataAccessType == daRead)
    {
        if (fInsertCmd)
        {
            pCommandType = "Z3";
        }
        else
        {
            pCommandType = "z3";
        }
    }
    else if (dataAccessType == daBoth)
    {
        if (fInsertCmd)
        {
            pCommandType = "Z4";
        }
        else
        {
            pCommandType = "z4";
        }
    }
    else
    {
        throw _com_error(ERROR_INVALID_PARAMETER);
    }

    return pCommandType;
}

//AsynchronousGdbSrvController * AsynchronousGdbSrvController::Create(_In_ LPCTSTR pConnectParameters) 
AsynchronousGdbSrvController * AsynchronousGdbSrvController::Create(_In_ const std::vector<std::wstring> &coreConnectionParameters) 
{
    if (coreConnectionParameters.empty())
    {
        throw _com_error(E_INVALIDARG);
    }
    AsynchronousGdbSrvController * pResult = new AsynchronousGdbSrvController(coreConnectionParameters);
    if (pResult == nullptr)
    {
        throw _com_error(ERROR_NOT_ENOUGH_MEMORY);
    }
    return pResult;
}

AsynchronousGdbSrvController::AsynchronousGdbSrvController(_In_ const std::vector<std::wstring> &coreConnectionParameters) :
    GdbSrvController(coreConnectionParameters),
    m_asynchronousCommandThread(nullptr),
    m_isAsynchronousCmdStopReplyPacket(false)
{
    m_AsynchronousCmd.pController = nullptr;
    m_AsynchronousCmd.isRspNeeded = false;
}

AsynchronousGdbSrvController::~AsynchronousGdbSrvController()
{
    if (IsAsynchronousCommandInProgress())
    {
        ShutdownGdbSrv();
        WaitForSingleObject(m_asynchronousCommandThread, INFINITE);
    }

    if (m_asynchronousCommandThread != nullptr)
    {
        CloseHandle(m_asynchronousCommandThread);
        m_asynchronousCommandThread = nullptr;
    }
}

//
//  CreateCodeBreakpoint    Insert a code breakpoint at a specific address
//
//  Request:
//  Z type,addr,kind        where:
//                          type is the breakpoint type. 0- memory breakpoint
//                          addr is the breakpoint address
//                          kind is target-specific and typically indicates the 
//                          size of the breakpoint in bytes that should be inserted.
//  Response:
//  'OK'                    if the command succeeded.
//  ''                      not supported.
//  'E NN'                  if failed. 
//
//  Example:
//  bp 0x817d687f
//
//  The below command will be sent to the GdbServer before any step/go command.
//
//  Z0817d687f,1
//  +
//  OK
//  +
//
unsigned AsynchronousGdbSrvController::CreateCodeBreakpoint(_In_ AddressType address)
{
    unsigned slot = static_cast<unsigned>(-1);
    for(unsigned i = 0; i < m_breakpointSlots.size(); ++i)
    {
        if (!m_breakpointSlots[i])
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
    {
        slot = static_cast<unsigned>(m_breakpointSlots.size());
        m_breakpointSlots.push_back(false);
    }

    char breakCmd[128] = {0}; 
    TargetArchitecture targetArchitecture = GdbSrvController::GetTargetArchitecture();
    PCSTR pFormat = (targetArchitecture == ARM64_ARCH || targetArchitecture == AMD64_ARCH) ?
                     "Z0,%I64x,%d" : "Z0,%x,%d";     
    sprintf_s(breakCmd, _countof(breakCmd), pFormat, address, GetBreakPointSize());

    bool isReplyOK = false;
    unsigned totalNumberOfCores = GdbSrvController::GetNumberOfRspConnections();
    for (unsigned numberOfCores = 0; numberOfCores < totalNumberOfCores; ++numberOfCores)
    {
        int retryCounter = 0;
        RSP_Response_Packet replyType;
        do
        {
            std::string reply = ExecuteCommandOnProcessor(breakCmd, true, 0, numberOfCores);

            replyType = GetRspResponse(reply);
            if (replyType == RSP_OK)
            {
                m_breakpointSlots[slot] = true;
                isReplyOK = true;
                break;
            }
        }
        while (IS_BAD_REPLY(replyType) && IS_RETRY_ALLOWED(++retryCounter));
        
    }
    if (!isReplyOK)
    {
        std::exception("Setting a Code breakpoint failed");
    }

    return slot;
}

//
//  DeleteCodeBreakpoint    Deletes a code breakpoint
//
//  Request:
//  z type,addr,kind        where:
//                          type is the breakpoint type. 0- memory breakpoint
//                          addr is the breakpoint address
//                          kind is target-specific and typically indicates the 
//                          size of the breakpoint in bytes that should be inserted.
//  Response:
//  'OK'                    if the command succeeded.
//  ''                      not supported.
//  'E NN'                  if failed. 
//
//  Example:
//  bc 1
//
//  z0817d687f,1
//  +
//  OK
//  +
//
void AsynchronousGdbSrvController::DeleteCodeBreakpoint(_In_ unsigned breakpointNumber, _In_ AddressType address)
{
    if (breakpointNumber >= m_breakpointSlots.size() || !m_breakpointSlots[breakpointNumber])
    {
        throw std::exception("Trying to delete nonexisting breakpoint");
    }

    char breakCmd[128] = {0}; 
    TargetArchitecture targetArchitecture = GdbSrvController::GetTargetArchitecture();
    PCSTR pFormat = (targetArchitecture == ARM64_ARCH || targetArchitecture == AMD64_ARCH) ?
                     "z0,%I64x,%d" : "z0,%x,%d";     
    sprintf_s(breakCmd, _countof(breakCmd), pFormat, address, GetBreakPointSize());

    bool isReplyOK = false;
    unsigned totalNumberOfCores = GdbSrvController::GetNumberOfRspConnections();
    for (unsigned numberOfCores = 0; numberOfCores < totalNumberOfCores; ++numberOfCores)
    {
        int retryCounter = 0;
        RSP_Response_Packet replyType;
        do
        {
            std::string reply = ExecuteCommandOnProcessor(breakCmd, true, 0, numberOfCores);
            replyType = GetRspResponse(reply);
            if (replyType == RSP_OK)
            {
                m_breakpointSlots[breakpointNumber] = false;
                isReplyOK = true;
                break;
            }
        }
        while (IS_BAD_REPLY(replyType) && IS_RETRY_ALLOWED(++retryCounter));
    }
    if (!isReplyOK)
    {
        std::exception("Deleting a Code breakpoint failed");
    }
}

//
//  CreateDataBreakpoint    Insert a data breakpoint at a specific address
//
//  Parameters:
//      address:            Breakpoint address
//      accessWidth:        Specifies the size of the location, in bytes, to monitor for access.
//      dataAccessType:     Specifies the type of access that satisfies the breakpoint 
//                          (can be daWrite/daRead/daBoth)
//
//  Request:
//  For daWrite breakpoint type:
//    Z2,address,accessWidth     
//
//  For daRead breakpoint type:
//    Z3,address,accessWidth     
//
//  For daBoth breakpoint type:
//    Z4,address,accessWidth     
//
//  Response:
//  'OK'                    if the command succeeded.
//  ''                      not supported.
//  'E NN'                  if failed. 
//
//  Example:
//  ba r4 0x81419120
//  
//  $Z3,81419120,32#e4
//  +
//  $OK#9a
//  +  
//
unsigned AsynchronousGdbSrvController::CreateDataBreakpoint(_In_ AddressType address, _In_ BYTE accessWidth, 
                                                            _In_ DATA_ACCESS_TYPE dataAccessType)
{
    unsigned slot = static_cast<unsigned>(-1);
    for(unsigned i = 0; i < m_dataBreakpointSlots.size(); ++i)
    {
        if (!m_dataBreakpointSlots[i])
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
    {
        slot = static_cast<unsigned>(m_dataBreakpointSlots.size());
        m_dataBreakpointSlots.push_back(false);
    }
    const char * pCommandType = GetDataAccessBreakPointCommand(dataAccessType, true);
    assert(pCommandType != nullptr);

    char breakCmd[128] = {0}; 
    TargetArchitecture targetArchitecture = GdbSrvController::GetTargetArchitecture();
    PCSTR pFormat = (targetArchitecture == ARM64_ARCH || targetArchitecture == AMD64_ARCH) ?
                     "%s,%I64x,%d" : "%s,%x,%d";     
    sprintf_s(breakCmd, _countof(breakCmd), pFormat, pCommandType, address, accessWidth);

    bool isReplyOK = false;
    unsigned totalNumberOfCores = GdbSrvController::GetNumberOfRspConnections();
    for (unsigned numberOfCores = 0; numberOfCores < totalNumberOfCores; ++numberOfCores)
    {
        int retryCounter = 0;
        RSP_Response_Packet replyType;
        do
        {
            std::string reply = ExecuteCommandOnProcessor(breakCmd, true, 0, numberOfCores);

            replyType = GetRspResponse(reply);
            if (replyType == RSP_OK)
            {
                m_dataBreakpointSlots[slot] = true;
                isReplyOK = true;
                break;
            }
        }
        while (IS_BAD_REPLY(replyType) && IS_RETRY_ALLOWED(++retryCounter));
    }
    if (!isReplyOK)
    {
        std::exception("Setting a Data breakpoint failed");
    }

    return slot;
}

//
//  DeleteDataBreakpoint    Deletes a data breakpoint
//
//  Parameters:
//      breakpointNumber:   Breakpoint number
//      address:            Breakpoint address
//
//  Request:
//  For daWrite breakpoint type:
//    z2,address,accessWidth     
//
//  For daRead breakpoint type:
//    z3,address,accessWidth     
//
//  For daBoth breakpoint type:
//    z4,address,accessWidth     
//
//  Response:
//  'OK'                    if succeed
//  ''                      not supported
//  'E NN'                  if failed. 
//
void AsynchronousGdbSrvController::DeleteDataBreakpoint(_In_ unsigned breakpointNumber, _In_ AddressType address,
                                                        _In_ BYTE accessWidth, _In_ DATA_ACCESS_TYPE dataAccessType)
{
    if (breakpointNumber >= m_dataBreakpointSlots.size() || !m_dataBreakpointSlots[breakpointNumber])
    {
        throw std::exception("Trying to delete nonexisting data breakpoint");
    }

    const char * pCommandType = GetDataAccessBreakPointCommand(dataAccessType, false);
    assert(pCommandType != nullptr);

    char breakCmd[128] = {0}; 
    TargetArchitecture targetArchitecture = GdbSrvController::GetTargetArchitecture();
    PCSTR pFormat = (targetArchitecture == ARM64_ARCH || targetArchitecture == AMD64_ARCH) ?
                     "%s,%I64x,%d" : "%s,%x,%d";     
    sprintf_s(breakCmd, _countof(breakCmd), pFormat, pCommandType, address, accessWidth);

    bool isReplyOK = false;
    unsigned totalNumberOfCores = GdbSrvController::GetNumberOfRspConnections();
    for (unsigned numberOfCores = 0; numberOfCores < totalNumberOfCores; ++numberOfCores)
    {
        int retryCounter = 0;
        RSP_Response_Packet replyType;
        do
        {
            std::string reply = ExecuteCommandOnProcessor(breakCmd, true, 0, numberOfCores);
            replyType = GetRspResponse(reply);
            if (replyType == RSP_OK)
            {
                m_dataBreakpointSlots[breakpointNumber] = false;
                isReplyOK = true;
                break;
            }
        }
        while (IS_BAD_REPLY(replyType) && IS_RETRY_ALLOWED(++retryCounter));
    }
    if (!isReplyOK)
    {
        std::exception("Deleting a Data breakpoint failed");
    }
}

std::string AsynchronousGdbSrvController::ExecuteCommand(_In_ LPCSTR pCommand)
{
    return AsynchronousGdbSrvController::ExecuteCommandEx(pCommand, true, 0);
}

std::string AsynchronousGdbSrvController::ExecuteCommandEx(_In_ LPCSTR pCommand, _In_ bool isExecCmd, _In_ size_t size)
{
    return AsynchronousGdbSrvController::ExecuteCommandOnProcessor(pCommand, isExecCmd, size, GdbSrvController::GetLastKnownActiveCpu());
}

std::string AsynchronousGdbSrvController::ExecuteCommandOnProcessor(_In_ LPCSTR pCommand, _In_ bool isExecCmd, 
                                                                    _In_ size_t size, _In_ unsigned currentActiveProcessor)
{
    if (IsAsynchronousCommandInProgress())
    {
        throw std::exception("Cannot execute a command while an asynchronous command is in progress (e.g. target is running)\r\n");
    }

    return GdbSrvController::ExecuteCommandOnProcessor(pCommand, isExecCmd, size, currentActiveProcessor);
}

void AsynchronousGdbSrvController::StartAsynchronousCommand(_In_ LPCSTR pCommand, _In_ bool isRspNeeded)
{
    assert(pCommand != nullptr);
    if (IsAsynchronousCommandInProgress())
    {
        throw std::exception("Cannot execute a command while an asynchronous command is in progress (e.g. target is running).");
    }

    if (m_asynchronousCommandThread != nullptr)
    {
        CloseHandle(m_asynchronousCommandThread);
    }

    //At this point no other thread is using these, so no lock is needed
    m_currentAsynchronousCommand = pCommand;
    m_currentAsynchronousCommandResult.clear();

    DWORD threadId = 0;
    m_AsynchronousCmd.pController = this;
    m_AsynchronousCmd.isRspNeeded = isRspNeeded;

    m_asynchronousCommandThread = CreateThread(nullptr, 0, AsynchronousCommandThreadBody, 
                                               reinterpret_cast<PVOID>(&m_AsynchronousCmd), 0, &threadId);
    if (m_asynchronousCommandThread == nullptr)
    {
        throw std::exception("Failed to start asynchronous command thread.");
    }
}

bool AsynchronousGdbSrvController::IsAsynchronousCommandInProgress()
{
    return m_asynchronousCommandThread != nullptr &&
           WaitForSingleObject(m_asynchronousCommandThread, 0) != WAIT_OBJECT_0;
}

bool AsynchronousGdbSrvController::GetAsynchronousCommandResult(_In_ DWORD timeoutInMilliseconds, _Out_opt_ std::string * pResult)
{
    if (pResult != nullptr)
    {
        pResult->clear();
    }

    if (m_asynchronousCommandThread == nullptr)
    {
        throw std::exception("No active asynchronous command is running");
    }

    bool result = false;

    if (WaitForSingleObject(m_asynchronousCommandThread, timeoutInMilliseconds) == WAIT_OBJECT_0)
    {
        result = true;
        if (pResult != nullptr)
        {
            pResult->assign(m_currentAsynchronousCommandResult);
        }

    }
    return result;
}

DWORD AsynchronousGdbSrvController::AsynchronousCommandThreadBody(LPVOID p)
{
    startAsynchronousCommandStruct * pCmdStruct = reinterpret_cast<startAsynchronousCommandStruct *>(p);
    assert(pCmdStruct->pController != nullptr);

    try
    {
        ConfigExdiGdbServerHelper & cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        if (cfgData.GetMultiCoreGdbServer())
        {
            //  We are in the multi_Core GdbServer, but we let go all cores when we do step/continue commands then
            //  we accept the first core response as the response with the program counter value to continue.
            //  We discard all other core responses.
            pCmdStruct->pController->m_currentAsynchronousCommandResult = 
            pCmdStruct->pController->GdbSrvController::ExecuteCommandOnMultiProcessors(pCmdStruct->pController->m_currentAsynchronousCommand.c_str(),
                                                                                       pCmdStruct->isRspNeeded, 0); 
        }
        else
        {
            pCmdStruct->pController->m_currentAsynchronousCommandResult = 
            pCmdStruct->pController->GdbSrvController::ExecuteCommandEx(pCmdStruct->pController->m_currentAsynchronousCommand.c_str(),
                                                                        pCmdStruct->isRspNeeded, 0);
        }
        return 0;
    }
    CATCH_AND_RETURN_DWORD;
}

void AsynchronousGdbSrvController::StartStepCommand(unsigned processorNumber)
{
    if (processorNumber != -1)
    {
        //  Set to run to any thread.
        if (!SetThreadCommand(processorNumber, "c"))
        {
            MessageBox(0, _T("Unable to set processor number or the GdbServer is not ready continue on any thread"), nullptr, MB_ICONERROR);
        }
    }
    StartAsynchronousCommand("s", false);
}

void AsynchronousGdbSrvController::StartRunCommand()
{
    StartAsynchronousCommand("c", false);
}

bool AsynchronousGdbSrvController::HandleInterruptTarget(_Inout_ AddressType * pPcAddress, _Out_ DWORD * pProcessorNumber,
                                                         _Out_ bool * pEventNotification)
{
    assert(pPcAddress != nullptr && pProcessorNumber != nullptr && pEventNotification != nullptr);
    bool isDone = false;

    if (GdbSrvController::InterruptTarget())
    {
        StopReplyPacketStruct stopReply;
        std::string reply = GetCommandResult();
        if (!reply.empty())
        {
            //  Verify the previously asynchronous response
            GdbSrvController::HandleAsynchronousCommandResponse(reply, &stopReply);
        }
        else
        {
            GdbSrvController::ReportReasonTargetHalted(&stopReply);
        }
        //  Is it a T AA packet type?
        if (stopReply.status.isTAAPacket && 
            (stopReply.stopReason == TARGET_BREAK_SIGINT || stopReply.stopReason == TARGET_BREAK_SIGTRAP))
        {
            *pEventNotification = true;            
            *pPcAddress = stopReply.currentAddress;
            // Do we have core/thread specified in the response?
            if (stopReply.status.isThreadFound)
            {
                assert(stopReply.processorNumber != static_cast<ULONG>(-1));
                if (GdbSrvController::GetFirstThreadIndex() > 0)
                {
                    *pProcessorNumber = stopReply.processorNumber - 1;
                }
                else
                {
                    *pProcessorNumber = stopReply.processorNumber;
                }
            }
            else
            {
                *pProcessorNumber = GdbSrvController::GetLastKnownActiveCpu();
            }
            isDone = true;
        } 
        //  Is it a S AA packet type?
        else if (stopReply.status.isSAAPacket)
        {
            *pEventNotification = true;            
            *pProcessorNumber = GdbSrvController::GetLastKnownActiveCpu();
            isDone = true;
        }
    }
    return isDone;
}

int AsynchronousGdbSrvController::GetBreakPointSize()
{
    int breakPointLength = 0;
    TargetArchitecture targetArchitecture = GdbSrvController::GetTargetArchitecture();
    if (targetArchitecture == X86_ARCH || targetArchitecture == AMD64_ARCH)
    {
        //  This represents the kind parameter and contains the length of the breakpoint instruction.
        //  In Intel x86/amd64, the encoding length of the break instruction occupies one byte (int 3 : 0xCC).
        breakPointLength = 1;
    }
    else if (targetArchitecture == ARM32_ARCH)
    {
        //  This represents the kind parameter and contains the length of the breakpoint instruction.
        //  Our ABI ARM thumb implementation uses the breakpoint sequence 0xDEFE.
        breakPointLength = 2;
    }
    else if (targetArchitecture == ARM64_ARCH)
    {
        breakPointLength = 4;
    }
    return breakPointLength;
}