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
    m_AsynchronousCmd.isReqNeeded = false;
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

    ConfigExdiGdbServerHelper& cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
    PCSTR pBpCommand = (cfgData.GetTreatSwBpAsHwBp()) ? "Z1" : "Z0";
    TargetArchitecture targetArchitecture = GdbSrvController::GetTargetArchitecture();
    PCSTR pFormat = (targetArchitecture == ARM64_ARCH || targetArchitecture == AMD64_ARCH) ?
                     "%s,%I64x,%d" : "%s,%x,%d";
    char breakCmd[128] = { 0 };
    sprintf_s(breakCmd, _countof(breakCmd), pFormat, pBpCommand, address, GetBreakPointSize());

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

    ConfigExdiGdbServerHelper& cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
    PCSTR pBpCommand = (cfgData.GetTreatSwBpAsHwBp()) ? "z1" : "z0";
    TargetArchitecture targetArchitecture = GdbSrvController::GetTargetArchitecture();
    PCSTR pFormat = (targetArchitecture == ARM64_ARCH || targetArchitecture == AMD64_ARCH) ?
                     "%s,%I64x,%d" : "%s,%x,%d";
    char breakCmd[128] = { 0 };
    sprintf_s(breakCmd, _countof(breakCmd), pFormat, pBpCommand, address, GetBreakPointSize());

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

std::string AsynchronousGdbSrvController::GetResponseOnProcessor(_In_ size_t size, _In_ unsigned currentActiveProcessor)
{
    if (IsAsynchronousCommandInProgress())
    {
        throw std::exception("Cannot execute a command while an asynchronous command is in progress (e.g. target is running)\r\n");
    }

    return GdbSrvController::GetResponseOnProcessor(size, currentActiveProcessor);
}

void AsynchronousGdbSrvController::StartAsynchronousCommand(_In_ LPCSTR pCommand, _In_ bool isRspNeeded, _In_ bool isReqNeeded)
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
    m_AsynchronousCmd.isReqNeeded = isReqNeeded;

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
            if (pCmdStruct->isReqNeeded)
            {
                pCmdStruct->pController->m_currentAsynchronousCommandResult = 
                pCmdStruct->pController->GdbSrvController::ExecuteCommandEx(pCmdStruct->pController->m_currentAsynchronousCommand.c_str(),
                                                                            pCmdStruct->isRspNeeded, 0);
            }
            else
            {
                pCmdStruct->pController->m_currentAsynchronousCommandResult = 
                pCmdStruct->pController->GdbSrvController::GetResponseOnProcessor(0, 
                    pCmdStruct->pController->GdbSrvController::GetLastKnownActiveCpu());
            }
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

    //  Step by using the new command:
    //      vCont[;s[:thread-id]]
    //  Use resume the inferior thread, specifying different actions for each thread.
    //  For each inferior thread, the leftmost action with a matching thread - id is applied.
    //  Threads that don't match any action remain in their current state.
    //  An action ('s') with no thread - id matches all threads.
    //  Specifying no actions is an error.
    char stepCommand[256] = "vCont;s:";
    _snprintf_s(stepCommand, _TRUNCATE, "%s%s", stepCommand, GetTargetThreadId(processorNumber).c_str());
    StartAsynchronousCommand(stepCommand, false, true);
}

void AsynchronousGdbSrvController::StartRunCommand()
{
    StartAsynchronousCommand("vCont;c", false, true);
}

bool AsynchronousGdbSrvController::HandleInterruptTarget(_Inout_ AddressType * pPcAddress, _Out_ DWORD * pProcessorNumber,
                                                         _Out_ bool * pEventNotification)
{
    assert(pPcAddress != nullptr && pProcessorNumber != nullptr && pEventNotification != nullptr);

    bool isBreakDone = false;
    *pEventNotification = false;

    // check of the asyc recv is still active
    if (!IsAsynchronousCommandInProgress())
    {
        // try to get a new pending response sent by GDBserver
        // that was not processed in the previous request.
        StartAsynchronousCommand("", true, false);
    }

    if (GdbSrvController::InterruptTarget())
    {
        ULONG attempts = 0;
        StopReplyPacketStruct stopReply;
        ULONG totalPackets = 0;

        isBreakDone = true;
        do
        {
            std::string reply = GetCommandResult();
            if (!reply.empty())
            {
                //  Verify the previously asynchronous response
                GdbSrvController::HandleAsynchronousCommandResponse(reply, &stopReply);
                HandleStopReply(reply, stopReply, pPcAddress, pProcessorNumber, pEventNotification);
                // rest attempts on each valid packet
                attempts = 0;
            }
            else
            {
                // wait a little longer for a reply packet
                Sleep(c_asyncResponsePauseMs);
            }
        }
        while (!*pEventNotification &&
            (attempts++ < c_attemptsWaitingOnPendingResponse) &&
            (totalPackets++ < c_maximumReplyPacketsInResponse));

        if (!*pEventNotification)
        {
            //  We did not get the GDB "stop-reply" packet, so enquire
            //  the target status.
            GdbSrvController::ReportReasonTargetHalted(&stopReply);
            std::string noReply;
            HandleStopReply(noReply, stopReply, pPcAddress, pProcessorNumber, pEventNotification);
        }
    }
    return isBreakDone;
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

void AsynchronousGdbSrvController::HandleStopReply(_In_ const std::string reply, _In_ StopReplyPacketStruct & stopReply,
    _Inout_ AddressType* pPcAddress, _Out_ DWORD* pProcessorNumber, _Out_ bool * pEventNotification)
{
    *pEventNotification = false;

    //  Is it a OXX console packet
    if (stopReply.status.isOXXPacket)
    {
        //  Try to display the GDB server ouput message if there is an attached text console.
        GdbSrvController::DisplayConsoleMessage(reply);
        //  Post another receive request on the packet buffer
        ContinueWaitingOnStopReplyPacket();
        Sleep(20);
    }
    else if (stopReply.status.isTAAPacket &&
        (stopReply.stopReason == TARGET_BREAK_SIGINT || 
         stopReply.stopReason == TARGET_BREAK_SIGTRAP ||
         stopReply.stopReason == TARGET_UNKNOWN))
    {
        *pEventNotification = true;
        if (stopReply.status.isPcRegFound)
        {
            *pPcAddress = stopReply.currentAddress;
        }
        // Do we have core/thread specified in the response?
        if (stopReply.status.isThreadFound)
        {
            assert(stopReply.processorNumber != static_cast<ULONG>(-1));
            if (stopReply.processorNumber != static_cast<ULONG>(C_ALLCORES))
            {
                *pProcessorNumber = stopReply.processorNumber;
            }
        }
        else
        {
            *pProcessorNumber = GdbSrvController::GetLastKnownActiveCpu();
        }
    }
    //  Is it a S AA packet type?
    else if (stopReply.status.isSAAPacket)
    {
        *pEventNotification = true;
        *pProcessorNumber = GdbSrvController::GetLastKnownActiveCpu();
    }
    // Is it an "OK" response w/o any other field (e.g. OpenOCD can send this after 's'/'g')?
    else if (stopReply.status.isCoreRunning)
    {
        //  Post another receive request on the packet buffer, since there is still no
        //  trace of the current thread/address packet.
        ContinueWaitingOnStopReplyPacket();
    }
}

void AsynchronousGdbSrvController::ContinueWaitingOnStopReplyPacket()
{
    if (IsAsynchronousCommandInProgress())
    {
        throw std::exception("Cannot execute a command while an asynchronous command is in progress (e.g. target is running).");
    }

    if (m_asynchronousCommandThread == nullptr)
    {
        throw std::exception("No active asynchronous command is running");
    }

    //  the same thread is using the command
    m_currentAsynchronousCommand = "";
    m_currentAsynchronousCommandResult.clear();

    m_AsynchronousCmd.pController = this;
    m_AsynchronousCmd.isRspNeeded = true;
    m_AsynchronousCmd.isReqNeeded = false;
    AsynchronousCommandThreadBody(reinterpret_cast<PVOID>(&m_AsynchronousCmd));
}

void AsynchronousGdbSrvController::StopTargetAtRun()
{
    if (IsAsynchronousCommandInProgress() && 
        m_currentAsynchronousCommand == "c" &&
        m_AsynchronousCmd.isRspNeeded == false)
    {
        //  In case that the target is at run and  the client debugger requested 
        //  a command w/o interruption, then force to interrup the waiting state
        //  of the GDB client link layer. This situation should not happen, since the debugger engine
        //  should not post any command unless the target is at break state, but
        //  there is small chance that this client has not notified the engine about
        //  the current target state (target is at run/at break).
        ADDRESS_TYPE currentAddress;
        DWORD eventProcessor = 0;
        bool eventNotification = false;
        //  Set the thread interrupt event
        HandleInterruptTarget(reinterpret_cast<AddressType*>(&currentAddress),
            &eventProcessor, &eventNotification);
        //  Wait for the thread to finish itself once the interrup event is received.
        WaitForSingleObject(m_asynchronousCommandThread, INFINITE);
    }

}

