//----------------------------------------------------------------------------
//
// GdbSrvControllerLib.cpp
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
#include <locale>
#include <codecvt>
#include "TargetArchitectureHelpers.h"
#include "TargetGdbServerHelpers.h"

using namespace GdbSrvControllerLib;

//=============================================================================
// Private data definitions
//=============================================================================    
//  Maximum monitor command buffer
const DWORD C_MAX_MONITOR_CMD_BUFFER = 8192;

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

// 
//  Request to read feature target file from the GDB server 
//  It's use to request reading xml registers target file.
// 
LPCSTR const g_RequestGdbReadFeatureFile = "qXfer:features:read:";

//
//  Request PA memory access mode
//
LPCSTR const g_RequestGdbSetReadPAmode = "qqemu.PhyMemMode";
LPCSTR const g_RequestGdbSetWritePAmode = "Qqemu.PhyMemMode";

//
//  Set of internal Exdi commands that are not sent to the GDB server,
//  so these commands are processed offline, and they are accessible via 
// .exdicmd debugger command.
//

//  Telemetry command and TargetIDs
LPCWSTR const g_GdbSrvTelemetryCmd = L"ExdiDbgType";
LPCSTR const g_GdbSrvTrace32 = "GdbSrv-Trace32";
LPCSTR const g_GdbSrvGeneric = "GdbSrv-Generic";

//  Print current set of System registers
LPCWSTR const g_GdbSrvPrintSystemRegs = L"info registers system";
LPCWSTR const g_GdbSrvPrintSystemRegsVerbose = L"info registers system -v";
LPCWSTR const g_GdbSrvPrintCoreRegs = L"info registers core";

//  Set Memory Mode on specific servers
LPCWSTR const g_GdbSrvSetPAMemoryMode = L"SetPAMemoryMode";

//  Server Name that supports only memory request mode via PAa
LPCWSTR const g_GdbSrvPaMemoryMode = L"BMC-SMM";

//  Header for the verbose command
PCSTR const g_headerRegisterVerbose[] =
{
    "Name",
    "Value",
    "Access code"
};

//=============================================================================
// Private function definitions
//=============================================================================

class GdbSrvController::GdbSrvControllerImpl
{
public:
    GdbSrvControllerImpl::GdbSrvControllerImpl(_In_ const std::vector<std::wstring>& coreNumberConnectionParameters) :
        m_pTextHandler(nullptr),
        m_cachedProcessorCount(0),
        m_lastKnownActiveCpu(0),
        m_TargetHaltReason(TARGET_HALTED::TARGET_UNKNOWN),
        m_displayCommands(true),
        m_targetProcessorArch(UNKNOWN_ARCH),
        m_targetProcessorFamilyArch(PROCESSOR_FAMILY_UNK),
        m_ThreadStartIndex(-1),
        m_pRspClient(std::unique_ptr <GdbSrvRspClient<TcpConnectorStream>>
            (new (std::nothrow) GdbSrvRspClient<TcpConnectorStream>(coreNumberConnectionParameters))),
        m_IsForcedPAMemoryMode(false)
    {
        m_cachedKPCRStartAddress.clear();
        m_targetProcessorIds.clear();
        //  Bind the exdi functions
        SetExdiFunctions(exdiComponentFunctionList[0], std::bind(&GdbSrvControllerImpl::AttachGdbSrv,
            this, std::placeholders::_1, std::placeholders::_2));
        SetExdiFunctions(exdiComponentFunctionList[0], std::bind(&GdbSrvControllerImpl::CloseGdbSrvCore,
            this, std::placeholders::_1, std::placeholders::_2));
        ConfigExdiGdbServerHelper& cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        m_IsThrowExceptionEnabled = cfgData.IsExceptionThrowEnabled();
        InitializeSystemRegistersFunctions();
        InitializeInternalGdbClientFunctionMap();
        cfgData.GetGdbServerRegisters(&m_spRegisterVector);
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

        //  Check if this is an internal exdicmd function.
        std::map<wstring, InternalGdbClientFunctions>::const_iterator itFunction =
            m_InternalGdbFunctions.find(TargetArchitectureHelpers::WMakeLowerCase(pCmdToExecute));
        if (itFunction != m_InternalGdbFunctions.end())
        {
            return itFunction->second();
        }

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

        std::string commandMonitor;
        if (strstr(pMonitorCmd, g_RequestGdbSetReadPAmode) != nullptr ||
            strstr(pMonitorCmd, g_RequestGdbSetWritePAmode) != nullptr)
        {
            commandMonitor += pMonitorCmd;
        }
        else
        {
            commandMonitor += "qRcmd,";
            commandMonitor += dataBuffer.c_str();
        }

        std::string reply = ExecuteCommandOnProcessor(commandMonitor.c_str(), true, 0, core);
        size_t messageLength = reply.length();

        //  Is an empty response or an error response 'E NN'?
        if (messageLength == 0 || IsReplyError(reply))
        {
            throw _com_error(E_FAIL);
        }

        ConfigExdiGdbServerHelper& cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
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
                if (messageLength >= (monitorResult.GetCapacity() - (monitorResult.GetLength() + 1)) ) 
                {
                    if (!monitorResult.TryEnsureCapacity((monitorResult.GetLength() + 1) + (4 * C_MAX_MONITOR_CMD_BUFFER)))
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

                if (cfgData.IsGdbMonitorCmdDoNotWaitOnOKEnable())
                {
                    replyDone = true;
                }
                else
                {
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
        functionToExec = TargetArchitectureHelpers::WMakeLowerCase(pFunctionToExecute);
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
    //  This implements only TCP/IP (socket) connection, but the GdbServer
    //  can support serial connection.
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
                pFunction = TargetArchitectureHelpers::DisplayCommDataForChannel;
            }
            else
            {
                pFunction = TargetArchitectureHelpers::DisplayCommData;
            }
            m_displayCommands = false;
        }
        const RSP_CONFIG_COMM_SESSION commSession = 
        {
            static_cast<unsigned int>(cfgData.GetMaxConnectAttempts()), 
            static_cast<unsigned int>(cfgData.GetSendPacketTimeout()),
            static_cast<unsigned int>(cfgData.GetReceiveTimeout()),
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
        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;
        std::wstring wAgentName;
        cfgData.GetExdiComponentAgentNamePacket(wAgentName);
        if (!wAgentName.empty())
        {
            const std::string sAgentName = converter.to_bytes(wAgentName);
            const std::string reply = ExecuteCommand(sAgentName.c_str());
            if (IsReplyError(reply))
            {
                return false;
            }
        }

        //  Send the "qSupported" packet
        wstring wQSupportedConfigPacket;
        cfgData.GetRequestQSupportedPacket(wQSupportedConfigPacket);
        const std::string sQSupportedConfigPacket = converter.to_bytes(wQSupportedConfigPacket);
        const char * qSupported = (sQSupportedConfigPacket.empty()) ? "qSupported" : sQSupportedConfigPacket.c_str();
        string cmdResponse = ExecuteCommand(qSupported);

        //  Set the features supported and update the remote with our supported feature
        bool IsSetFeatureSucceeded = m_pRspClient->UpdateRspPacketFeatures(cmdResponse);
        if (IsSetFeatureSucceeded)
        {
            //  Tell the remote to turn on No ACK mode.
            if (m_pRspClient->IsFeatureEnabled(PACKET_QSTART_NO_ACKMODE))
            {
                const char ClientNoAckMode[] = "QStartNoAckMode";
                std::string noCmdResponse = ExecuteCommand(ClientNoAckMode);
                if (IsReplyError(noCmdResponse))
                {
                    return false;
                }
            }

            //  Process system registers if there are available
            //  Tell the remote to turn on No ACK mode.
            if (m_pRspClient->IsFeatureEnabled(PACKET_TARGET_DESCRIPTION))
            {
                HandleTargetDescriptionPacket(cfgData);
            }
            else if (cfgData.IsSystemRegistersAvailable())
            {
                //  Get the actual system register vector from the table.
                cfgData.GetGdbServerSystemRegisters(&m_spSystemRegisterVector);
            }

            //  Enable extended features that are no advertised by the qSupported GDB server response,
            //  it's needed to read system registers/ARM64 CP15 registers in such GDB servers w/o
            //  having cutomized qSupported responses (i.e. OpenOCD GDB server, but Trace32 has customized 
            //  GDB packets for reading special memory)
            if (cfgData.IsSupportedSystemRegistersGdbMonitor())
            {
                //  Enable accessing target system register
                m_pRspClient->SetFeatureEnable(PACKET_READ_OPENOCD_SPECIAL_REGISTER);
                m_pRspClient->SetFeatureEnable(PACKET_WRITE_OPENOCD_SPECIAL_REGISTER);

            } else {

                wstring targetName;
                cfgData.GetGdbServerTargetName(targetName);
                if (_wcsicmp(targetName.c_str(), g_GdbSrvPaMemoryMode) == 0)
                {
                    //  Enable accessing target system register
                    m_pRspClient->SetFeatureEnable(PACKET_READ_BMC_SMM_PA_MEMORY);
                    m_pRspClient->SetFeatureEnable(PACKET_WRITE_BMC_SMM_PA_MEMORY);
                }
            }
        }
        return IsSetFeatureSucceeded;
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
                        m_lastKnownActiveCpu = coreStopReply.processorNumber;

                    }
                    memcpy(pStopReply, &coreStopReply, sizeof(StopReplyPacketStruct));
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

    //
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
        char setThreadCommand[256] = "H";
        if (m_targetProcessorIds.empty())
        {
            _snprintf_s(setThreadCommand, _TRUNCATE, "%s%s%x", setThreadCommand, pOperation, processorNumber);
        }
        else
        {
            _snprintf_s(setThreadCommand, _TRUNCATE, "%s%s%s", setThreadCommand, pOperation, m_targetProcessorIds[processorNumber].c_str());
        }
        bool isSet = false;
        int retryCounter = 0;
        RSP_Response_Packet replyType = RSP_ERROR;
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
                //  Did the user interrupt?
                if (!m_pRspClient->GetInterruptFlag())
                {
                    //  No, then this is a fatal error or a communication error ocurred
                    m_pRspClient->HandleRspErrors(GdbSrvTextType::CommandError);
                    throw _com_error(HRESULT_FROM_WIN32(m_pRspClient->GetRspLastError()));
                }
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
    //  GetCommandOnProcessor   Get any response pending on the processor
    //
    //  Parameters:
    //  stringSize              Size of the result string. Allows to control the maximum size of the string
    //                          in order to minimize the STL automatically resizing mechanism.
    //  processor               Processor core to send the command.
    //
    //  Return:
    //  The command response.
    //
    std::string GdbSrvControllerImpl::GetResponseOnProcessor(_In_ size_t stringSize, _In_ unsigned processor)
    {
        std::string result;
        if (result.max_size() < stringSize)
        {
            throw _com_error(E_INVALIDARG);
        }

        if (result.capacity() < stringSize)
        {
            result.reserve(stringSize);
        }

        bool isPollingMode = false;
        bool isDone = m_pRspClient->ReceiveRspPacketEx(result, processor, true, isPollingMode, false);
        if (!isDone)
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
        ULARGE_INTEGER result={0};
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
    //  QueryAllRegistersEx     Reads all registers specified by the group type.
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
    std::map<std::string, std::string> GdbSrvControllerImpl::QueryAllRegistersEx(_In_ unsigned processorNumber,
        _In_ RegisterGroupType groupType = CORE_REGS)
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
        std::map<std::string, std::string> result;
        size_t startIdx = 0;
        size_t endIdx = 0;
        size_t replyLength = reply.length();
        for (const_regIterator it = RegistersBegin(groupType);
             it != RegistersEnd(groupType) && startIdx < replyLength;
             ++it)
        {
            //  Each response byte is transmitted as a two-digit hexadecimal ascii number in target order.
            endIdx = (it->registerSize << 1);
            //  Reverse the register value from target order to memory order.
            result[it->name] = TargetArchitectureHelpers::ReverseRegValue(reply.substr(startIdx, endIdx));
            startIdx += endIdx;
        }
        return result;
    }

    //
    //  QueryAllRegisters       Reads all general registers
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
        return QueryAllRegistersEx(processorNumber, CORE_REGS);
    }

    //
    //  SetRegistersEx      Sets all general registers.  
    //
    //  Parameters:
    //  processorNumber     Processor core number.
    //  registerValues      Map containing the registers to be set.
    //  isRegisterValuePtr  Flag telling how the register map second element should be treated (pointer/value).
    //  groupType           Register group type
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
    void GdbSrvControllerImpl::SetRegistersEx(_In_ unsigned processorNumber, 
                                              _In_ const std::map<std::string, AddressType> &registerValues,
                                              _In_ bool isRegisterValuePtr,
                                              _In_ RegisterGroupType groupType = CORE_REGS)
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
            const_regIterator it = FindRegisterVectorEntryEx(kv.first, groupType);

            std::string registerValue;
            const unsigned char * pRawRegBuffer = (isRegisterValuePtr) ? reinterpret_cast<const unsigned char *>(kv.second) : 
                                                                         reinterpret_cast<const unsigned char *>(&kv.second);
            for (size_t idx = 0; idx < it->registerSize; ++idx)
            {
                registerValue.insert(registerValue.end(), 1, NumberToAciiHex(((pRawRegBuffer[idx] >> 4) & 0xf)));
                registerValue.insert(registerValue.end(), 1, NumberToAciiHex((pRawRegBuffer[idx] & 0xf)));
            }
            char command[512];
            _snprintf_s(command, _TRUNCATE, "P%s=%s", it->nameOrder.c_str(), registerValue.c_str());

            std::string reply = ExecuteCommand(command);
            if (!IsReplyOK(reply))
            {
                throw _com_error(E_FAIL);
            }
        }
    }

    void GdbSrvControllerImpl::SetRegisters(_In_ unsigned processorNumber,
        _In_ const std::map<std::string, AddressType>& registerValues,
        _In_ bool isRegisterValuePtr)
    {
        SetRegistersEx(processorNumber, registerValues, isRegisterValuePtr, CORE_REGS);
    }

    //
    //  QueryRegisters      Request reading core registers.
    //
    //  Parameters:
    //  processorNumber     Processor core number.
    //  registerNames       An array containing the list of register names to query.
    //  numberOfElements    Number of elements in the query array.
    //  groupType           Register group type
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
    std::map<std::string, std::string> GdbSrvControllerImpl::QueryRegistersEx(
        _In_ unsigned processorNumber,
        _In_reads_(numberOfElements) const char * registerNames[],
        _In_ const size_t numberOfElements,
        _In_ RegisterGroupType groupType = CORE_REGS) 
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
            const_regIterator it = FindRegisterVectorEntryEx(registerName, groupType);

            char command[512];
            _snprintf_s(command, _TRUNCATE, "p%s", it->nameOrder.c_str());

            std::string reply = ExecuteCommand(command);
            if (IsReplyError(reply) || reply.empty())
            {
                throw _com_error(E_FAIL);
            }
            //  Process the register value returned by the GDBServer
            result[registerName] = TargetArchitectureHelpers::ReverseRegValue(reply);
        }
        return result;
    }

    std::map<std::string, std::string> GdbSrvControllerImpl::QueryRegisters(
        _In_ unsigned processorNumber,
        _In_reads_(numberOfElements) const char * registerNames[],
        _In_ const size_t numberOfElements) 
    {
        return QueryRegistersEx(processorNumber, registerNames, numberOfElements, CORE_REGS);
    }

    //
    //  QueryRegistersByGroup  Request reading all registers for the passed in group type
    //
    //  Parameters:
    //  processorNumber        Processor core number.
    //  groupType              Register group type
    //  maxRegisterNameLength  the maximum length of the register name set
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
    std::map<std::string, std::string> GdbSrvControllerImpl::QueryRegistersByGroup(
        _In_ unsigned processorNumber, _In_ RegisterGroupType groupType,
        _Out_ int & maxRegisterNameLength)
    {
        if (processorNumber != -1)
        {
            //  Set the processor core before setting the register values.
            if (!SetThreadCommand(processorNumber, "g"))
            {
                throw _com_error(E_FAIL);
            }
        }

        maxRegisterNameLength = 0;
        std::map<std::string, std::string> result;
        for (const_regIterator it = RegistersBegin(groupType);
            it != RegistersEnd(groupType); ++it)
        {
            const_regIterator itReg = FindRegisterVectorEntryEx(it->name, groupType);

            char command[512];
            _snprintf_s(command, _TRUNCATE, "p%s", itReg->nameOrder.c_str());

            std::string reply = ExecuteCommand(command);
            if (IsReplyError(reply) || reply.empty())
            {
                throw _com_error(E_FAIL);
            }
            maxRegisterNameLength = (maxRegisterNameLength < itReg->name.length()) ? 
                static_cast<int>(itReg->name.length()) :
                maxRegisterNameLength;
            //  Process the register value returned by the GDBServer
            result[itReg->name] = TargetArchitectureHelpers::ReverseRegValue(reply);
        }
        return result;
    }

    //
    //  ReadSystemRegistersFromGdbMonitor Obtains a register value from the GDB monitor command response.
    //
    //  Parameters:
    //  systemRegIndex  Address pointing to the system register inndex encoded.
    //  maxSize         Size of the expected system register value.
    //  memType         The memory class that will be accessed by the read operation.
    //
    //  Return:
    //  A simple buffer object containing the system register value.
    //
    //
    SimpleCharBuffer GdbSrvControllerImpl::ReadSystemRegistersFromGdbMonitor(
        _In_ AddressType systemRegIndex,
        _In_ size_t maxSize,
        _In_ const memoryAccessType memType)
    {
        // Decode the system register passed in index.
        SystemRegister systemReg = {0};
        if (TargetArchitectureHelpers::SetSystemRegister(m_targetProcessorArch, systemRegIndex, &systemReg) != S_OK)
        {
            throw _com_error(E_NOTIMPL);
        }

        PCSTR pFormat = GetReadMemoryCmd(memType);
        unique_ptr<wchar_t> pWideFormat(new (nothrow) wchar_t[strlen(pFormat) + 1]);
        if (pWideFormat == nullptr)
        {
            throw _com_error(E_OUTOFMEMORY);
        }
        if (MultiByteToWideChar(CP_ACP, 0, pFormat, -1, pWideFormat.get(), static_cast<int>(strlen(pFormat) + 1)) == 0)
        {
            throw _com_error(E_FAIL);
        }

        wchar_t systemRegCmd[256] = {0};
        swprintf_s(systemRegCmd, ARRAYSIZE(systemRegCmd), pWideFormat.get(),
                   systemReg.Op0, systemReg.Op1, systemReg.CRn, systemReg.CRm, systemReg.Op2);

        SimpleCharBuffer monitorString = ExecuteExdiGdbSrvMonitor(GetLastKnownActiveCpu(), systemRegCmd);
        std::string memoryValueStr(monitorString.GetInternalBuffer(), monitorString.GetEndOfData());
        // Validate the response, since GDB can include additional spew in front of the register value
        std::size_t pos = memoryValueStr.find("0x");
        if (pos == string::npos || memoryValueStr.length() <= maxSize)
        {
            throw _com_error(E_FAIL);
        }

        ULONG64 regValue = 0;
        if (sscanf_s(&memoryValueStr[pos], "%I64x", &regValue) != 1)
        {
            throw _com_error(E_INVALIDARG);
        }

        SimpleCharBuffer memoryValue;
        if (!memoryValue.TryEnsureCapacity(memoryValueStr.size()))
        {
            throw _com_error(E_OUTOFMEMORY);
        }

        size_t copySize = min(sizeof(regValue), memoryValue.GetCapacity()); 
        memcpy(memoryValue.GetInternalBuffer(), &regValue, copySize);
        memoryValue.SetLength(copySize);
        return memoryValue; 
    }

    //
    //  ReadSysRegByQueryRegGdbCmd Reads System registers by query register command
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
    //
    //  Response:
    //
    //  Example:
    //  Request:
    //
    //  Response:
    //
    SimpleCharBuffer GdbSrvControllerImpl::ReadSysRegByQueryRegGdbCmd(
        _In_ AddressType address, 
        _In_ size_t maxSize, 
        _In_ const memoryAccessType memType)
    {
        //  Find the system register Name by the register access code
        //  each reg. code is an unique number that identifies the system registers
        const char* arraySystemRegisterToQuery[] = { nullptr };
        arraySystemRegisterToQuery[0] = GetSystemRegNamebyAccessCode(address);

        std::map<std::string, std::string> systemRegMapResult = QueryRegistersEx(GetLastKnownActiveCpu(),
            arraySystemRegisterToQuery, ARRAYSIZE(arraySystemRegisterToQuery), SYSTEM_REGS);
        ULONGLONG systemRegValue = ParseRegisterValue(systemRegMapResult[arraySystemRegisterToQuery[0]]);

        SimpleCharBuffer systemRegBuffer;
        if (!systemRegBuffer.TryEnsureCapacity(C_MAX_MONITOR_CMD_BUFFER))
        {
            throw _com_error(E_OUTOFMEMORY);
        }

        //  Get the system reg. value back to the debugger engine
        size_t copySize = std::min<size_t>(systemRegBuffer.GetCapacity(), sizeof(systemRegValue));
        memcpy(systemRegBuffer.GetInternalBuffer(), &systemRegValue, copySize);
        systemRegBuffer.SetLength(copySize);
        return systemRegBuffer;
    }

    //
    //  ReadSystemRegisters Reads System registers
    //
    //  Parameters:
    //  address            Memory address location to read.
    //  maxSize            Size of the memory chunk to read.
    //  memType            The memory class that will be accessed by the read operation.
    //
    //  Return:
    //  A simple buffer object containing the memory content.
    //
    //  Request:
    //
    //  Response:
    //
    //  Example:
    //  Request:
    //
    //  Response:
    //
    SimpleCharBuffer GdbSrvControllerImpl::ReadSystemRegisters(_In_ AddressType address, _In_ size_t maxSize, _In_ const memoryAccessType memType)
    {
        auto itFunction = m_ReadSystemRegisterFunctions.find(GetSystemRegHandler(memType));
        if (itFunction == m_ReadSystemRegisterFunctions.end())
        {
            //  Unsupported function
            throw _com_error(E_NOTIMPL);
        }

        return itFunction->second(address, maxSize, memType);
    }

    //
    //  WriteSystemRegisters     Writes into a system register
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
    //
    //  Response: 
    //
    bool GdbSrvControllerImpl::WriteSystemRegisters(_In_ AddressType address, _In_ size_t size, _In_ const void* pRawBuffer,
        _Out_ DWORD* pdwBytesWritten, _In_ const memoryAccessType memType,
        _In_ bool fReportWriteError)
    {
        auto itFunction =
            m_WriteSystemRegisterFunctions.find(GetSystemRegHandler(memType));
        if (itFunction == m_WriteSystemRegisterFunctions.end())
        {
            //  Unsupported function
            throw _com_error(E_NOTIMPL);
        }

        return itFunction->second(address, size, pRawBuffer, pdwBytesWritten, memType, fReportWriteError);
    }

    bool GdbSrvControllerImpl::WriteSystemRegBySetRegisterGdbCmd(_In_ AddressType address, _In_ size_t size, 
        _In_ const void* pRawBuffer, _Out_ DWORD* pdwBytesWritten, _In_ const memoryAccessType memType, _In_ bool fReportWriteError)
    {
        if (size != sizeof(ULONGLONG))
        {
            throw _com_error(E_INVALIDARG);
        }
        const char* arraySystemRegisterToQuery[] = { nullptr };
        arraySystemRegisterToQuery[0] = GetSystemRegNamebyAccessCode(address);
        std::map<std::string, ULONGLONG> systemReg;
        systemReg.insert({ arraySystemRegisterToQuery[0], *((ULONGLONG *)(pRawBuffer)) });
        SetRegistersEx(GetLastKnownActiveCpu(), systemReg, false, SYSTEM_REGS);
        *pdwBytesWritten = sizeof(ULONGLONG);
        return true;
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

        //
        // The maxPacketLength is the maximum PACKET length of an RSP packet, not how much memory we can push
        // across.  At worst, the memory encodes each byte as two characters (hex) plus the packet overhead.
        // All read requests need to be clamped to that size.
        //
        ConfigExdiGdbServerHelper& cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        size_t maxPacketLength = cfgData.GetMaxServerPacketLength();
        
        //
        // Add the $ #<2 byte checksum>
        //
        constexpr size_t packetOverhead = 4;
        if (maxPacketLength == 0)
        {
            maxPacketLength = maxSize * 2 + packetOverhead;
        }

        //  We need to support local configuration maximum packet size and packetsize that 
        //  the GdbServer dynamically supports by sending chunk of data until we reach the maximum requested size.
        while (maxSize != 0)
        {
            bool fError = false;

            size_t requestSize = (maxPacketLength - packetOverhead) / 2;
            if (requestSize > maxSize)
            {
                requestSize = maxSize;
            }
            size_t size = requestSize;

            //  We will send a sequence of ?m addr,length? request packets
            //  until we obtain the requested data length from the GdbServer.
            for (;;)
            {
                size_t recvLength = 0;
                char memoryCmd[256] = { 0 };
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
                    //  unless we didn't read anything, in which case fail
                    if (result.GetLength() == 0 && GetThrowExceptionEnabled())
                    {
                        throw _com_error(E_FAIL);
                    }
                    break;
                }

                //  Handle the received memory data
                for (size_t pos = 0; pos < messageLength; pos += 2)
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
                //  Update the parameters for the next packet.
                address += recvLength;
                size -= recvLength;
                //  Are we done with the requested data?
                if (size == 0 || messageLength == 0)
                {
                    break;
                }
            }
            if (fError)
            {
                break;
            }
            maxSize -= requestSize;
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
                m_targetProcessorIds.clear();
                do
                {
                    std::size_t pos = reply.find("m");
                    if (pos != string::npos && reply.length() > 1)
                    {
                        // Store thread/processor Ids from the message
                        TargetArchitectureHelpers::TokenizeThreadId(&reply[pos + 1], ",", 
                            &m_targetProcessorIds);
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
                assert(m_cachedProcessorCount == m_targetProcessorIds.size());
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
    //  FindPcRegisterArrayEntry    Returns the Pc register iterator for the current architecture
    //
    //  Parameters:
    //  
    //  Return:
    //  The register entry for the PC register.
    //
    const_regIterator FindPcRegisterVectorEntry()
    {
        const_regIterator it;
        if (m_targetProcessorArch == X86_ARCH)
        {
            it = FindRegisterVectorEntry("Eip");
        }
        else if (m_targetProcessorArch == AMD64_ARCH)
        {
            it = FindRegisterVectorEntry("rip");
        }
        else if (m_targetProcessorArch == ARM32_ARCH || m_targetProcessorArch == ARM64_ARCH)
        {
            it = FindRegisterVectorEntry("pc");
        }
        else
        {
            assert(false);
        }
        assert(it != RegistersEnd());
        return it;
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

        const_regIterator it = FindPcRegisterVectorEntry();
        std::string pcRegAddress = it->nameOrder + ":";
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
                        *pPcAddress = ParseRegisterValue(TargetArchitectureHelpers::ReverseRegValue(pcAddress));
                    }
                    else
                    {
                        *pPcAddress = ParseRegisterValue32(TargetArchitectureHelpers::ReverseRegValue(pcAddress));
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

            if (cmdResponse[0] == 'O')
            {
                //  this is just a console message to wait for the real stop reply
                //  so exit to handle the console message
                pRspPacket->status.isOXXPacket = true;
                return true;
            }

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
                        if (!m_targetProcessorIds.empty())
                        {
                            std::vector<std::string>::iterator it = std::find(m_targetProcessorIds.begin(), m_targetProcessorIds.end(), processorNumber);
                            if (it != m_targetProcessorIds.end())
                            {
                                processorNumber = *it;
                            }
                        }
                        std::vector<std::string> processorIds;
                        TargetArchitectureHelpers::TokenizeThreadId(processorNumber, ";", &processorIds);
                        assert(processorIds.size() == 1);
                        if (m_targetProcessorIds.empty())
                        {
                            m_targetProcessorIds.push_back(processorIds[0]);
                        }
                        pRspPacket->processorNumber = GetProcessorNumberByThreadId(processorIds[0]);
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
    //  GetTargetThreadId Get the target thread ID from the passed in logical processor number
    //
    //  Parameters:
    //  processorNumber Core processor number
    //
    //  Return:
    //  the string containing the threadId
    //
    //  Note.
    //  The ThreadID is the symbolic representation of the Target GDB server thread identifiers
    //  as these IDs are reported in the initial qThreadinfo command.
    //
    std::string GdbSrvControllerImpl::GetTargetThreadId(_In_ unsigned processorNumber) 
    {
        assert(processorNumber <= m_targetProcessorIds.size());

        return m_targetProcessorIds[processorNumber];
    }

    //
    //  GetProcessorNumberByThreadId Get the virtualized core processor number that is used by the
    //                               client debugger.
    //
    //  Parameters:
    //  threadId                     ThreadId symbolic representation as it kept by the GDB server.
    //
    //  Return:
    //  The core processor number as it's undertood by the client debugger
    //
    //  Note.
    //  This is the symbolic representation of the threadID kept by the
    //  GDB server entity
    //
    DWORD GdbSrvControllerImpl::GetProcessorNumberByThreadId(_In_ const std::string & threadId) 
    {
        std::vector<std::string>::const_iterator it = std::find(m_targetProcessorIds.cbegin(), m_targetProcessorIds.cend(), threadId);
        if (it == m_targetProcessorIds.cend()) 
        {
            throw _com_error(E_INVALIDARG);
        }
        return static_cast<DWORD>(std::distance(m_targetProcessorIds.cbegin(), it));
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
                    const_regIterator it = FindPcRegisterVectorEntry();
                    assert(it != RegistersEnd());

                    std::string pcRegAddress = it->nameOrder + ":";
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

    inline void GdbSrvControllerImpl::SetTargetProcessorFamilyByTargetArch(_In_ TargetArchitecture targetArch) 
    {
        if (targetArch == X86_ARCH || targetArch == AMD64_ARCH)
        {
            m_targetProcessorFamilyArch = PROCESSOR_FAMILY_X86;
        }
        else if (targetArch == ARM64_ARCH)
        {
            m_targetProcessorFamilyArch = PROCESSOR_FAMILY_ARMV8ARCH64;
        }
        else if (targetArch == ARM32_ARCH)
        {
            m_targetProcessorFamilyArch = PROCESSOR_FAMILY_ARM;
        }
        else
        {
            m_targetProcessorFamilyArch = PROCESSOR_FAMILY_UNK;
        }
    }

    inline TargetArchitecture GdbSrvControllerImpl::GetTargetArchitecture() {return m_targetProcessorArch;}

    inline DWORD GdbSrvControllerImpl::GetProcessorFamilyArchitecture() {return m_targetProcessorFamilyArch;}

    inline unsigned GdbSrvControllerImpl::GetLastKnownActiveCpu() {return m_lastKnownActiveCpu;}
    inline void GdbSrvControllerImpl::SetLastKnownActiveCpu(_In_ unsigned cpu) {m_lastKnownActiveCpu = cpu;}

    inline unsigned GdbSrvControllerImpl::GetNumberOfRspConnections() 
    {
        assert(m_pRspClient != nullptr);
        return static_cast<unsigned>(m_pRspClient->GetNumberOfStreamConnections());
    }

    inline void GdbSrvControllerImpl::DisplayLogEntry(_In_reads_bytes_(readSize) const char * pBuffer, _In_ size_t readSize)
    {
        TargetArchitectureHelpers::DisplayTextData(pBuffer, readSize, GdbSrvTextType::CommandError, m_pTextHandler);
    }

    void GdbSrvControllerImpl::CreateNeonRegisterNameArray(_In_ const std::string & registerName,
                                                           _Out_writes_bytes_(numberOfRegArrayElem) std::unique_ptr<char> pRegNameArray[],
                                                           _In_ size_t numberOfRegArrayElem)
    {
        assert(pRegNameArray != nullptr);

        size_t numberOfRegisters = 0;
        const_regIterator it = FindRegisterVectorEntryAndNumberOfElements(registerName, numberOfRegisters);
        if (it == RegistersEnd())
        {
            throw _com_error(E_FAIL);
        }
        assert(numberOfRegArrayElem < numberOfRegisters);

        for (size_t index = 0; index < numberOfRegArrayElem; ++index, ++it)
        {
            pRegNameArray[index] = std::unique_ptr <char>(new (std::nothrow) char[C_MAX_REGISTER_NAME_ARRAY_ELEM]);
            if (pRegNameArray[index] == nullptr)
            {
                throw _com_error(E_OUTOFMEMORY);
            }
            FillNeonRegisterNameArrayEntry(it, pRegNameArray[index].get(), C_MAX_REGISTER_NAME_ARRAY_ELEM);
        }
    }

    inline int GdbSrvControllerImpl::GetFirstThreadIndex() {return m_ThreadStartIndex;}

    void GdbSrvControllerImpl::GetMemoryPacketType(_In_ DWORD64 cpsrRegValue, _Out_ memoryAccessType * pMemType)
    {
        TargetArchitectureHelpers::GetMemoryPacketType(m_targetProcessorArch, cpsrRegValue, pMemType);
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
        statusRegister[0] = TargetArchitectureHelpers::GetProcessorStatusRegByArch(m_targetProcessorArch);
        if (statusRegister[0] != nullptr)
        {
            std::map<std::string, std::string> cpsrRegisterValue = QueryRegisters(dwProcessorNumber, statusRegister, ARRAYSIZE(statusRegister));
            ULONGLONG processorStatusRegValue = ParseRegisterValue(cpsrRegisterValue[statusRegister[0]]);

            memoryAccessType memType = {0};
            hr = TargetArchitectureHelpers::SetSpecialMemoryPacketType(m_targetProcessorArch, 
                processorStatusRegValue, &memType);
            if (hr == S_OK)
            {
                *pValue = 0;
                SimpleCharBuffer buffer = ReadSystemRegisters(dwRegisterIndex, sizeof(*pValue), memType);
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
        statusRegister[0] = TargetArchitectureHelpers::GetProcessorStatusRegByArch(m_targetProcessorArch);
        if (statusRegister[0] != nullptr)
        {
            std::map<std::string, std::string> cpsrRegisterValue = QueryRegisters(dwProcessorNumber, statusRegister, ARRAYSIZE(statusRegister));
            ULONGLONG processorStatusRegValue = ParseRegisterValue(cpsrRegisterValue[statusRegister[0]]);

            memoryAccessType memType = {0};
            hr = TargetArchitectureHelpers::SetSpecialMemoryPacketType(m_targetProcessorArch, 
                processorStatusRegValue, &memType);
            if (hr == S_OK)
            {
                assert(memType.isSpecialRegs);
                DWORD bytesWritten = 0;
                bool isMemoryWritten = WriteSystemRegisters(dwRegisterIndex, sizeof(value), &value, &bytesWritten, memType, true);
                hr = (isMemoryWritten && (bytesWritten != 0)) ? S_OK : E_FAIL;
            }
        }
        return hr;
    }

    //
    //  DisplayConsoleMessage    Display message to the attached console if the text console is available
    //  
    //  Parameters:
    //  message                  String with the Message formatted in hex-ascii.
    //
    //  Return:
    //  Nothing
    //
    void GdbSrvControllerImpl::DisplayConsoleMessage(_In_ const std::string& message)
    {
        if (message.empty() || m_pTextHandler == nullptr)
        {
            return;
        }

        size_t messageLength = message.length();
        std::string consoleMsg;
        size_t pos = (message[0] == 'O') ? 1 : 0;
        for (; pos < messageLength; pos += 2) 
        {
            string part = message.substr(pos, 2);
            char ch = static_cast<char>(stoul(part, nullptr, 16));
            consoleMsg += ch;
        }

        m_pTextHandler->HandleText(GdbSrvTextType::CommandOutput, consoleMsg.c_str(), consoleMsg.length());
    }

    //
    //  SetSystemRegisterXmlFile  Stores the system register xml full path.
    //
    //  Parameters:
    //  pSystemRegFilePath  Pointer to the full path location of the system register mapping xml file
    //
    //  Return:
    //  Nothing
    //
    void GdbSrvControllerImpl::SetSystemRegisterXmlFile(_In_z_ PCWSTR pSystemRegFilePath)
    {
        assert(pSystemRegFilePath != nullptr);

        size_t systemFileLength = wcslen(pSystemRegFilePath);
        if (systemFileLength == 0)
        {
            throw _com_error(E_INVALIDARG);
        }

        m_spSystemRegXmlFile.reset(new(std::nothrow) WCHAR[systemFileLength + 1]);
        if (m_spSystemRegXmlFile == nullptr)
        {
            throw _com_error(E_OUTOFMEMORY);
        }
        wcsncpy_s(m_spSystemRegXmlFile.get(), systemFileLength + 1, pSystemRegFilePath, systemFileLength);
    }

    void GdbSrvControllerImpl::SetInterruptEvent()
    {
        assert(m_pRspClient != nullptr);
        m_pRspClient->SetInterrupt();
    }

    bool GdbSrvControllerImpl::GetPAMemoryMode() const
    {
        return m_IsForcedPAMemoryMode;
    }

    void GdbSrvControllerImpl::SetPAMemoryMode(_In_ bool value)
    {
        m_IsForcedPAMemoryMode = value;
    }

    private:
    IGdbSrvTextHandler * m_pTextHandler;
    unsigned m_cachedProcessorCount;
    unsigned m_lastKnownActiveCpu;
    TARGET_HALTED m_TargetHaltReason;
    bool m_displayCommands;
    TargetArchitecture m_targetProcessorArch;
    DWORD m_targetProcessorFamilyArch;
    std::vector<AddressType> m_cachedKPCRStartAddress;
    int m_ThreadStartIndex;
    std::unique_ptr <GdbSrvRspClient<TcpConnectorStream>> m_pRspClient;
    typedef std::function<bool (const std::wstring &connectionStr, unsigned)> ExdiFunctions;
    std::map<std::wstring, ExdiFunctions> m_exdiFunctions;
    bool m_IsThrowExceptionEnabled;
    std::vector<std::string> m_targetProcessorIds;
    typedef std::function <SimpleCharBuffer (AddressType, size_t, const memoryAccessType)> ReadSystemRegisterFunctions;
    typedef map<SystemRegistersAccessCommand, ReadSystemRegisterFunctions> ReadSystemRegisterFunctionsMap;
    ReadSystemRegisterFunctionsMap m_ReadSystemRegisterFunctions;
    typedef std::function <bool (_In_ AddressType address, _In_ size_t size, _In_ const void * pRawBuffer,
        _Out_ DWORD* pdwBytesWritten, _In_ const memoryAccessType memType,
        _In_ bool fReportWriteError)> WriteSystemRegisterFunctions;
    typedef map<SystemRegistersAccessCommand, WriteSystemRegisterFunctions> WriteSystemRegisterFunctionsMap;
    WriteSystemRegisterFunctionsMap m_WriteSystemRegisterFunctions;
    typedef std::function <SimpleCharBuffer()> InternalGdbClientFunctions;
    typedef map<wstring, InternalGdbClientFunctions> InternalGdbMapFunctions;
    InternalGdbMapFunctions m_InternalGdbFunctions;
    unique_ptr <WCHAR[]> m_spSystemRegXmlFile;
    unique_ptr<vector<RegistersStruct>> m_spRegisterVector;
    unique_ptr<vector<RegistersStruct>> m_spSystemRegisterVector;
    unique_ptr<SystemRegistersMapType> m_spSystemRegAccessCodeMap;
    bool m_IsForcedPAMemoryMode;

    const_regIterator RegistersBegin(_In_ RegisterGroupType type = CORE_REGS) const {return (type == CORE_REGS) ? m_spRegisterVector->begin() : m_spSystemRegisterVector->begin();}
    const_regIterator RegistersEnd(_In_ RegisterGroupType type = CORE_REGS) const {return (type == CORE_REGS) ? m_spRegisterVector->end() : m_spSystemRegisterVector->end();}
    size_t RegistersGroupSize(_In_ RegisterGroupType type = CORE_REGS) const { return (type == CORE_REGS) ? m_spRegisterVector->size() : m_spSystemRegisterVector->size(); }

    inline void GdbSrvControllerImpl::SetExdiFunctions(_In_ PCWSTR pFunctionText, _In_ const ExdiFunctions function)
    {
        m_exdiFunctions[std::wstring(pFunctionText)] = function;
    }

    inline void GdbSrvControllerImpl::InitializeSystemRegistersFunctions()
    {
        // Initialize function adapters for reading system regs.
        m_ReadSystemRegisterFunctions = [this]()->auto {
            ReadSystemRegisterFunctionsMap _m_ReadSystemRegisterFunctions;
            _m_ReadSystemRegisterFunctions.insert({ SystemRegistersAccessCommand::QueryRegCmd,
                std::bind(&GdbSrvControllerImpl::ReadSysRegByQueryRegGdbCmd,
                    this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
            _m_ReadSystemRegisterFunctions.insert({ SystemRegistersAccessCommand::GDBMonitorCmd,
                std::bind(&GdbSrvControllerImpl::ReadSystemRegistersFromGdbMonitor,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
            _m_ReadSystemRegisterFunctions.insert({ SystemRegistersAccessCommand::MemoryCustomizedCmd,
                std::bind(&GdbSrvControllerImpl::ReadMemory,
                this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
            return move(_m_ReadSystemRegisterFunctions);
        }();

        // Initialize function adapters for writing system regs.
        m_WriteSystemRegisterFunctions = [this]()->auto {
            WriteSystemRegisterFunctionsMap _m_WriteSystemRegisterFunctions;
            _m_WriteSystemRegisterFunctions.insert({SystemRegistersAccessCommand::QueryRegCmd,
                std::bind(&GdbSrvControllerImpl::WriteSystemRegBySetRegisterGdbCmd,
                    this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4, std::placeholders::_5, std::placeholders::_6)});
            _m_WriteSystemRegisterFunctions.insert({ SystemRegistersAccessCommand::GDBMonitorCmd,
                std::bind(&GdbSrvControllerImpl::WriteMemory,
                    this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4, std::placeholders::_5, std::placeholders::_6)});
            return move(_m_WriteSystemRegisterFunctions);
        }();
    }

    inline void InitializeInternalGdbClientFunctionMap()
    {
        m_InternalGdbFunctions = [this]()->auto {
            InternalGdbMapFunctions _m_InternalGdbFunctions;
            _m_InternalGdbFunctions[TargetArchitectureHelpers::WMakeLowerCase(g_GdbSrvTelemetryCmd)] =
                std::bind(&GdbSrvControllerImpl::CheckExdiGdbSrv, this);
            _m_InternalGdbFunctions[TargetArchitectureHelpers::WMakeLowerCase(g_GdbSrvPrintSystemRegs)] =
                std::bind(&GdbSrvControllerImpl::PrintSystemRegisters, this);
            _m_InternalGdbFunctions[TargetArchitectureHelpers::WMakeLowerCase(g_GdbSrvPrintSystemRegsVerbose)] =
                std::bind(&GdbSrvControllerImpl::PrintSystemRegistersVerbose, this);
            _m_InternalGdbFunctions[TargetArchitectureHelpers::WMakeLowerCase(g_GdbSrvPrintCoreRegs)] =
                std::bind(&GdbSrvControllerImpl::PrintCoreRegisters, this);
            _m_InternalGdbFunctions[TargetArchitectureHelpers::WMakeLowerCase(g_GdbSrvSetPAMemoryMode)] =
                std::bind(&GdbSrvControllerImpl::SetPhysicalReadMemoryMode, this);
            return _m_InternalGdbFunctions;
        }();
    }

    SimpleCharBuffer GdbSrvControllerImpl::CheckExdiGdbSrv()
    {

        SimpleCharBuffer monitorResult;
        if (!monitorResult.TryEnsureCapacity(C_MAX_MONITOR_CMD_BUFFER))
        {
            throw _com_error(E_OUTOFMEMORY);
        }

        // It's an Internal telemetry command
        // Then return the Gdb Server type that is currently connected
        ConfigExdiGdbServerHelper& cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        LPCSTR pStrGdbSrvType;
        size_t gdbSrvStrTypeLength;
        wstring wGdbServerTarget;
        cfgData.GetGdbServerTargetName(wGdbServerTarget);
        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;
        const std::string sGdbServerTarget = converter.to_bytes(wGdbServerTarget);
        if (!sGdbServerTarget.empty())
        {
            pStrGdbSrvType = sGdbServerTarget.c_str();
            gdbSrvStrTypeLength = sGdbServerTarget.length();
        }
        else
        {
            pStrGdbSrvType = g_GdbSrvGeneric;
            gdbSrvStrTypeLength = strlen(g_GdbSrvGeneric);
        }

        monitorResult.SetLength(monitorResult.GetLength() + gdbSrvStrTypeLength);
        memcpy(&monitorResult[monitorResult.GetLength() - gdbSrvStrTypeLength], 
            pStrGdbSrvType, gdbSrvStrTypeLength);

        return monitorResult;
    }

    SimpleCharBuffer GdbSrvControllerImpl::SetPhysicalReadMemoryMode()
    {

        SimpleCharBuffer monitorResult;
        if (!monitorResult.TryEnsureCapacity(C_MAX_MONITOR_CMD_BUFFER))
        {
            throw _com_error(E_OUTOFMEMORY);
        }

        ConfigExdiGdbServerHelper& cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        wstring wGdbServerTarget;
        cfgData.GetGdbServerTargetName(wGdbServerTarget);
        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;
        const std::string sGdbServerTarget = converter.to_bytes(wGdbServerTarget);
        if (sGdbServerTarget.empty())
        {
            throw _com_error(E_FAIL);
        }

        if (!cfgData.GetServerRequirePAMemoryAccess())
        {
            throw _com_error(E_FAIL);
        }

        // Set the memory command to access via PA memory
        std::string commandMonitor("Qqemu.PhyMemMode:1");
        std::string reply = ExecuteCommandOnProcessor(commandMonitor.c_str(), true, 0, 0);
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
                SetPAMemoryMode(true);
            }
            else
            {
                if (messageLength >= (monitorResult.GetCapacity() - (monitorResult.GetLength() + 1)))
                {
                    if (!monitorResult.TryEnsureCapacity((monitorResult.GetLength() + 1) + (4 * C_MAX_MONITOR_CMD_BUFFER)))
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
                if (m_pRspClient->ReceiveRspPacketEx(reply, 0, true, IsPollingChannelMode, false))
                {
                    messageLength = reply.length();
                }
                else
                {
                    replyDone = true;
                }
            }
        } while (!replyDone);

        return monitorResult;
    }

    inline AddressType GetAccessCodeByRegisterNumber(_In_ const string & regOrder)
    {
        if (m_spSystemRegAccessCodeMap->empty())
        {
            throw _com_error(E_INVALIDARG);
        }

        for (auto it = m_spSystemRegAccessCodeMap->cbegin();
            it != m_spSystemRegAccessCodeMap->cend();
            ++it)
        {
            if (it->second.first == regOrder)
            {
                return it->first;
            }
        }
        return c_InvalidAddress;
    }

    inline const char * GetSystemRegNamebyAccessCode(_In_ AddressType regAccess)
    {
        if (m_spSystemRegAccessCodeMap->empty())
        {
            throw _com_error(E_INVALIDARG);
        }

        auto it = m_spSystemRegAccessCodeMap->find(regAccess);
        if (it != m_spSystemRegAccessCodeMap->end())
        {
            return it->second.second.c_str();
        }
        return "";
    }

    SimpleCharBuffer GdbSrvControllerImpl::PrintSystemRegisters()
    {
        return PrintRegistersGroup(SYSTEM_REGS);
    }

    SimpleCharBuffer GdbSrvControllerImpl::PrintSystemRegistersVerbose()
    {
        return PrintRegistersGroup(SYSTEM_REGS, true);
    }

    SimpleCharBuffer GdbSrvControllerImpl::PrintCoreRegisters()
    {
        return PrintRegistersGroup(CORE_REGS);
    }

    SimpleCharBuffer GdbSrvControllerImpl::PrintRegistersGroup(_In_ RegisterGroupType groupType, _In_ bool verbose = false)
    {
        //  Get the current system register values
        int alignValue;
        std::map<std::string, std::string> registers = QueryRegistersByGroup(GetLastKnownActiveCpu(),
            groupType, alignValue);

        size_t bufferCapacity = (registers.size() > 100) ? (2 * C_MAX_MONITOR_CMD_BUFFER) : 
            C_MAX_MONITOR_CMD_BUFFER;
        SimpleCharBuffer monitorResult;
        if (!monitorResult.TryEnsureCapacity(bufferCapacity))
        {
            throw _com_error(E_OUTOFMEMORY);
        }

        char regPrtBuffer[256] = { 0 };
        int index = 0;
        int operationResult  = sprintf_s(&regPrtBuffer[index], ARRAYSIZE(regPrtBuffer),
            "\nNumberOfRegisters: %zd\n", registers.size());

        if (verbose)
        {
            index += operationResult;
            operationResult = sprintf_s(&regPrtBuffer[index], ARRAYSIZE(regPrtBuffer) - index,
                "\n%*s | %*s | %*s\n", alignValue, g_headerRegisterVerbose[0], -16,
                g_headerRegisterVerbose[1], -6, g_headerRegisterVerbose[2]);
        }

        index += operationResult;
        int numberOfRegs = 0;
        for (const_regIterator it = RegistersBegin(groupType);
             it != RegistersEnd(groupType); ++it, numberOfRegs++)
        {
            if (verbose)
            {
                char buffer[128] = { 0 };
                AddressType accessCode = GetAccessCodeByRegisterNumber(it->nameOrder);
                sprintf_s(buffer, ARRAYSIZE(buffer), "0x%llx", accessCode);
                const char * pAccessCode = (accessCode == c_InvalidAddress) ?
                    "n/a" : buffer;
                operationResult = sprintf_s(&regPrtBuffer[index], ARRAYSIZE(regPrtBuffer) - index,
                    "%*s | %016I64x | %*s", alignValue, it->name.c_str(),
                    ParseRegisterValue(registers[it->name]),
                    -7, pAccessCode);
            }
            else
            {
                operationResult = sprintf_s(&regPrtBuffer[index], ARRAYSIZE(regPrtBuffer) - index,
                    "%*s = %016I64x ", alignValue, it->name.c_str(), 
                    ParseRegisterValue(registers[it->name]));
            }
            if (operationResult == -1)
            {
                throw _com_error(E_FAIL);
            }
            index += operationResult;
            bool isNeededPrint = (verbose) ? true : (((numberOfRegs + 1) % 3) == 0);
            if (isNeededPrint)
            {
                sprintf_s(&regPrtBuffer[index], ARRAYSIZE(regPrtBuffer)- index, "\n");
                size_t currentMsgLength = strlen(regPrtBuffer);
                if ((currentMsgLength + monitorResult.GetLength()) >= monitorResult.GetCapacity()) 
                {
                    if (!monitorResult.TryEnsureCapacity((monitorResult.GetLength() + 
                        ((2 * C_MAX_MONITOR_CMD_BUFFER) + 1))))
                    {
                        throw _com_error(E_OUTOFMEMORY);
                    } 
                }

                monitorResult.SetLength(monitorResult.GetLength() + currentMsgLength);
                memcpy(&monitorResult[monitorResult.GetLength() - currentMsgLength], 
                    regPrtBuffer, currentMsgLength);
                memset(regPrtBuffer, 0x00, sizeof(regPrtBuffer));
                index = 0;
            }
        }

        if (index != 0)
        {
            sprintf_s(&regPrtBuffer[index], ARRAYSIZE(regPrtBuffer) - index, "\n");
            monitorResult.SetLength(monitorResult.GetLength() + strlen(regPrtBuffer));
            memcpy(&monitorResult[monitorResult.GetLength() - strlen(regPrtBuffer)], regPrtBuffer, strlen(regPrtBuffer));
        }

        return monitorResult;
    }

    SystemRegistersAccessCommand GetSystemRegHandler(_In_ const memoryAccessType memType)
    {
        ConfigExdiGdbServerHelper& cfgData = ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(nullptr);
        if (m_spSystemRegisterVector != nullptr && !m_spSystemRegisterVector->empty() &&
            //  we can remove this checking once OpenOCD includes the full set of system registers in the xml target description file
            !(m_pRspClient->IsFeatureEnabled(PACKET_READ_OPENOCD_SPECIAL_REGISTER) && memType.isSpecialRegs))
        {
            // Use the regular "p<register order>" command since the GDB server supports target register description.
            return SystemRegistersAccessCommand::QueryRegCmd;
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_READ_OPENOCD_SPECIAL_REGISTER) && memType.isSpecialRegs)
        {
            // Get the system registers by requesting a specific monitor command.
            return SystemRegistersAccessCommand::GDBMonitorCmd;
        }
        else
        {
            // GDB server supports requesting registers via customized memory command (i.e. Trace32 GDB server)
            return SystemRegistersAccessCommand::MemoryCustomizedCmd;
        }
    }

    const_regIterator GdbSrvControllerImpl::FindRegisterVectorEntryEx(_In_ const std::string regName,
                                                                      _In_ RegisterGroupType regGroup)
    {
        const_regIterator it;
        for (it = RegistersBegin(regGroup); it != RegistersEnd(regGroup); ++it)
        {
            if (it->name == regName)
            {
                return it;
            }
        }
        throw _com_error(E_INVALIDARG);
    }

    const_regIterator GdbSrvControllerImpl::FindRegisterVectorEntry(_In_ const std::string regName)
    {
        return FindRegisterVectorEntryEx(regName, CORE_REGS);
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

    const_regIterator GdbSrvControllerImpl::FindRegisterVectorEntryAndNumberOfElements(_In_ const std::string regName,
        _Out_ size_t& numberOfElements)
    {
        const_regIterator it = FindRegisterVectorEntry(regName);
        if (it != RegistersEnd())
        {
            numberOfElements = m_spRegisterVector->size();
        }
        return it;
    }

    void GdbSrvControllerImpl::FillNeonRegisterNameArrayEntry(_In_ const_regIterator regIt,
        _Out_writes_bytes_(lengthArrayElem) char* pRegNameArray,
        _In_ size_t lengthArrayElem)
    {
        assert(pRegNameArray != nullptr);
        assert(strlen(regIt->name.c_str()) < lengthArrayElem);

        if (strcpy_s(pRegNameArray, lengthArrayElem, regIt->name.c_str()) != 0)
        {
            throw _com_error(E_FAIL);
        }
    }

    PCSTR GetReadMemoryCmd(_In_ memoryAccessType memType)
    {
        PCSTR pFormat = nullptr;

        bool isT32GdbServer = m_pRspClient->IsFeatureEnabled(PACKET_READ_TRACE32_SPECIAL_MEM);
        if (isT32GdbServer)
        {
            // It's a Trace32 request
            pFormat = Trace32GdbServerMemoryHelpers::GetGdbSrvReadMemoryCmd(
                memType, Is64BitArchitecture(), m_targetProcessorArch);
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_READ_OPENOCD_SPECIAL_REGISTER))
        {
            // It's an OpenOCD request
            pFormat = OpenOCDGdbServerMemoryHelpers::GetGdbSrvReadMemoryCmd(
                memType, Is64BitArchitecture());
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_READ_BMC_SMM_PA_MEMORY) &&
            !GetPAMemoryMode())
        {
            // It's an BMC-SMM server request
            pFormat = BmcSmmDGdbServerMemoryHelpers::GetGdbSrvReadMemoryCmd(
                memType, Is64BitArchitecture());
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
        if (isQ32GdbServerCmd)
        {
            pFormat = Trace32GdbServerMemoryHelpers::GetGdbSrvWriteMemoryCmd(
                memType, Is64BitArchitecture(), m_targetProcessorArch);
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_WRITE_OPENOCD_SPECIAL_REGISTER))
        {
            pFormat = OpenOCDGdbServerMemoryHelpers::GetGdbSrvWriteMemoryCmd(
                memType, Is64BitArchitecture());
        }
        else if (m_pRspClient->IsFeatureEnabled(PACKET_WRITE_BMC_SMM_PA_MEMORY))
        {
            // It's an BMC-SMM server request
            pFormat = BmcSmmDGdbServerMemoryHelpers::GetGdbSrvWriteMemoryCmd(
                memType, Is64BitArchitecture());
        }
        else
        {
             pFormat = Is64BitArchitecture() ? "M%I64x," : "M%x,";
             isQ32GdbServerCmd = false;
        }

        return pFormat;
    }

    void RequestXmlFileDescriptionFeature(_In_ ConfigExdiGdbServerHelper & cfgData,
                                          _In_ wstring const & wTargetFileName, 
                                          _In_ const char * requestCmd, 
                                          _In_ const char * startOffset,
                                          _In_ const char * lengthToRead)
    {
        //  Process the target description file
        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;
        const std::string sfileName = converter.to_bytes(wTargetFileName);

        char fileRegCmd[256] = { 0 };
        sprintf_s(fileRegCmd, ARRAYSIZE(fileRegCmd), "%s%s:%s,%s", requestCmd, sfileName.c_str(), startOffset, lengthToRead);

        bool isDone = false;
        std::string descriptionFile;
        descriptionFile.reserve(0xffff);
        size_t fileOffset = 0;
        do
        {
            const std::string reply = ExecuteCommand(fileRegCmd);
            if (IsReplyError(reply) || reply.length() == 0)
            {
                throw _com_error(E_FAIL);
            }

            // Store the received files
            std::size_t pos;
            size_t recvLength = reply.length();
            if (((pos = reply.find("m")) == 0 || (pos = reply.find("l")) == 0) && 
                recvLength > 1)
            {
                descriptionFile += reply.substr(pos + 1);
                if (reply[0] == 'l')
                {
                    isDone = true;
                }
                else
                {
                    fileOffset += (recvLength - 1);
                    sprintf_s(fileRegCmd, ARRAYSIZE(fileRegCmd), "%s%s:%zx,%s", requestCmd, sfileName.c_str(), fileOffset, lengthToRead);
                }
            }
            else if (reply.find("l") == 0 && recvLength == 1)
            {
                isDone = true;
            }
            else
            {
                isDone = true;
            }
        }
        while (!isDone);

        if (descriptionFile.empty())
        {
            throw _com_error(E_FAIL);
        }

        //  Parse the file target description
        std::wstring wTargetFileBuffer(descriptionFile.begin(), descriptionFile.end());
        //  @TODO: find a solution for xmlLite to handle "xi:include" tag, so for now
        //  Replace xi:include with a custom tag, otherwise xmllite reader will fail
        TargetArchitectureHelpers::ReplaceString(wTargetFileBuffer, L"xi:include", L"includeTarget");
        cfgData.SetXmlBufferToParse(wTargetFileBuffer.c_str());
    }

    void ValidateTargetArchitecture(_Inout_ ConfigExdiGdbServerHelper& cfgData)
    {
        //  Validate that the group register included in the target file matches
        //  with the current GDbserver target architecture configuration.
        if (cfgData.GetRegisterGroupArchitecture() != cfgData.GetTargetArchitecture())
        {
            if (cfgData.GetRegisterGroupArchitecture() != UNKNOWN_ARCH)
            {
                //  Set the current saved target architecture
                cfgData.SetTargetArchitecture(cfgData.GetRegisterGroupArchitecture());
                SetTargetArchitecture(cfgData.GetRegisterGroupArchitecture());
                SetTargetProcessorFamilyByTargetArch(cfgData.GetRegisterGroupArchitecture());
                //  Re-Read the core registers since the target GDB architecture changed 
                //  by the GDB server target description file
                cfgData.GetGdbServerRegisters(&m_spRegisterVector);
            }
            else
            {
                throw _com_error(E_INVALIDARG);
            }
        }

    }

    //
    //  HandleTargetDescriptionPacket - Handles the target description xml file.
    //  The QEMU target.xml has the following format:
    //  <target>
    //       <architecture>aarch64</architecture>
    //       <xi:include href="aarch64-core.xml"/>
    //       <xi:include href="aarch64-fpu.xml"/>
    //       <xi:include href="system-registers.xml"/>
    //   </target>
    //
    //  BMC-OpenOCD does not send a header target.xml file containing the list of
    //  subsequent register xml files, and rather it starts with the register file description.
    //
    //  Steps to get the system registers out of the xml file that is sent by the GDB server.
    //  Once we find the system-register.xml file, then extract the system registers described in this file.
    //  If feature enable that means that qSupported response contains "qXfer:features:read+"
    //  then do the following:
    //  _ Send the request to read the target description xml file by "$qXfer:features:read:target.xml:0"
    //  _ Set the xml buffer processing to parse the GDB response containing
    //    the target.xml file (call SetXmlBufferToParse("targetxmlresponse")
    //  - Parse the target.xml (need to be store in a separate config field in the Configspace table,
    //  _ Once it's parsed, then read the system xml file (system-registers.xml) stored in the
    //    config table field.
    //  _ Send the request to read the system register xml file by $qXfer:features:read:system-registers.xml:0,f
    //  _ Read the response into a buffer and then call to set the buffer to be parsed SetXmlBufferToParse("buffer response")
    //  _ Read the xml system registers, and then stores here the vector system registers from the config table.
    //
    void GdbSrvControllerImpl::HandleTargetDescriptionPacket(_In_ ConfigExdiGdbServerHelper & cfgData)
    {
        std::wstring wFileName;
        cfgData.GetTargetDescriptionFileName(wFileName);
        if (wFileName.empty())
        {
            return;
        }

        //  Process the target description file
        RequestXmlFileDescriptionFeature(cfgData, wFileName, g_RequestGdbReadFeatureFile, "0","ffb");

        //  Validate that the group register included in the target file matches
        //  with the current GDbserver target architecture configuration.
        ValidateTargetArchitecture(cfgData);

        //
        //  Does the GDBserver sent a header target file, if so then
        //  processes only the file with the system register group.
        //
        bool checkSystemRegFile = false;
        if (cfgData.IsRegisterGroupFileAvailable(SYSTEM_REGS))
        {
            //  Get the system register file
            cfgData.GetRegisterGroupFile(SYSTEM_REGS, wFileName);
            if (wFileName.empty())
            {
                throw _com_error(E_INVALIDARG);
            }

            //  Process the system register description file
            RequestXmlFileDescriptionFeature(cfgData, wFileName, g_RequestGdbReadFeatureFile, "0", "ffff");

            checkSystemRegFile = true;
        }
        //  Check the case when system registers are not defined by the 
        //  GDB server target xml file, so the core register set contains the system registers
        else if (cfgData.IsSystemRegistersAvailable())
        {
            checkSystemRegFile = true;
        }
        
        if (checkSystemRegFile)
        {
            if (m_spSystemRegXmlFile != nullptr && cfgData.ReadConfigFile(m_spSystemRegXmlFile.get()))
            {
                cfgData.GetSystemRegistersMapAccessCode(&m_spSystemRegAccessCodeMap);
            }

            //  Get the actual system register vector from the table.
            cfgData.GetGdbServerSystemRegisters(&m_spSystemRegisterVector);
        }
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

void GdbSrvController::SetInterruptEvent()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->SetInterruptEvent();    
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

std::string GdbSrvController::GetResponseOnProcessor(_In_ size_t stringSize, _In_ unsigned processor)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetResponseOnProcessor(stringSize, processor);
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

std::map<std::string, std::string> GdbSrvController::QueryRegistersByGroup(_In_ unsigned processorNumber, 
                                                                           _In_ RegisterGroupType groupType,
                                                                           _Out_ int & maxRegisterNameLength)
{
    {
        assert(m_pGdbSrvControllerImpl != nullptr);
        return m_pGdbSrvControllerImpl->QueryRegistersByGroup(processorNumber, groupType, maxRegisterNameLength);
    }
}

SimpleCharBuffer GdbSrvController::ReadMemory(_In_ AddressType address, _In_ size_t size, _In_ const memoryAccessType memType)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->ReadMemory(address, size, memType);
}

SimpleCharBuffer GdbSrvController::ReadSystemRegisters(_In_ AddressType address, _In_ size_t size, _In_ const memoryAccessType memType)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->ReadSystemRegisters(address, size, memType);
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

void GdbSrvController::SetTargetProcessorFamilyByTargetArch(_In_ TargetArchitecture targetArch) 
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    m_pGdbSrvControllerImpl->SetTargetProcessorFamilyByTargetArch(targetArch);
}

TargetArchitecture GdbSrvController::GetTargetArchitecture()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetTargetArchitecture();
}

DWORD GdbSrvController::GetProcessorFamilyArchitecture()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetProcessorFamilyArchitecture();
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

std::string GdbSrvController::GetTargetThreadId(_In_ unsigned processorNumber)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetTargetThreadId(processorNumber);
}

DWORD GdbSrvController::GetProcessorNumberByThreadId(_In_ const std::string & threadId)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetProcessorNumberByThreadId(threadId);
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

void GdbSrvController::DisplayConsoleMessage(_In_ const std::string& message)
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->DisplayConsoleMessage(message);
}

void GdbSrvController::SetSystemRegisterXmlFile(PCWSTR pSystemRegFilePath)
{
    assert(m_pGdbSrvControllerImpl != nullptr && pSystemRegFilePath != nullptr);
    m_pGdbSrvControllerImpl->SetSystemRegisterXmlFile(pSystemRegFilePath);
}

bool GdbSrvController::GetPAMemoryMode()
{
    assert(m_pGdbSrvControllerImpl != nullptr);
    return m_pGdbSrvControllerImpl->GetPAMemoryMode();
}

