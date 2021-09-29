//----------------------------------------------------------------------------
//
// AsynchronousGdbSrvController.h
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#pragma once
#include <vector>

#include "ExdiGdbSrv.h"
#include "GdbSrvControllerLib.h"

namespace GdbSrvControllerLib
{
    //  Waiting times, to handle GDB servers over slow HW debugger interfaces.
    const int c_attemptsWaitingOnPendingResponse = 690; // 15 seconds to finish break/step command (150 * 20ms wait)
    const int c_asyncResponsePauseMs = 20;              // milliseconds to sleep when waiting async packet arrival
    const int c_maximumReplyPacketsInResponse = 1000;   // when halting, GDB server may send per core console data (OpenOCD does this)
    class AsynchronousGdbSrvController : public GdbSrvController
    {
    public:
        static AsynchronousGdbSrvController * Create(_In_ const std::vector<std::wstring> &coreConnectionParameters);

        typedef struct
        {
            AsynchronousGdbSrvController * pController;
            bool isRspNeeded;
            bool isReqNeeded;
        } startAsynchronousCommandStruct;

        virtual std::string ExecuteCommand(_In_ LPCSTR pCommand) override;
        virtual std::string ExecuteCommandEx(_In_ LPCSTR pCommand,_In_ bool isExecCmd,  _In_ size_t size) override;
        virtual std::string ExecuteCommandOnProcessor(_In_ LPCSTR pCommand, _In_ bool isExecCmd, 
                                                      _In_ size_t size, _In_ unsigned currentActiveProcessor);
        virtual std::string GetResponseOnProcessor(_In_ size_t size, _In_ unsigned currentActiveProcessor);
        void StartAsynchronousCommand(_In_ LPCSTR pCommand, _In_ bool isRspNeeded, _In_ bool isReqNeeded);
        bool IsAsynchronousCommandInProgress();
        bool GetAsynchronousCommandResult(_In_ DWORD timeoutInMilliseconds, _Out_opt_ std::string * pResult);

        //High-level commands
        void StartStepCommand(unsigned processorNumber);
        void StartRunCommand();

        unsigned CreateCodeBreakpoint(_In_ AddressType address);
        void DeleteCodeBreakpoint(_In_ unsigned breakpointNumber, _In_ AddressType address);
        unsigned CreateDataBreakpoint(_In_ AddressType address, _In_ BYTE accessWidth, _In_ DATA_ACCESS_TYPE dataAccessType);
        void DeleteDataBreakpoint(_In_ unsigned breakpointNumber, _In_ AddressType address,
                                  _In_ BYTE accessWidth, _In_ DATA_ACCESS_TYPE dataAccessType);


        std::string & GetCommandResult() {return m_currentAsynchronousCommandResult;}

        bool HandleInterruptTarget(_Inout_ AddressType * pPcAddress, _Out_ DWORD *pProcessorNumberOfLastEvent,
                                   _Out_ bool * pEventNotification);
        void SetAsynchronousCmdStopReplyPacket() {m_isAsynchronousCmdStopReplyPacket = true;}
        void ResetAsynchronousCmdStopReplyPacket() {m_isAsynchronousCmdStopReplyPacket = false;}
        bool GetAsynchronousCmdStopReplyPacket() {return m_isAsynchronousCmdStopReplyPacket;}
        void ContinueWaitingOnStopReplyPacket();
        void HandleStopReply(_In_ const std::string reply, _In_ StopReplyPacketStruct& stopReply,
            _Inout_ AddressType* pPcAddress, _Out_ DWORD* pProcessorNumber, _Out_ bool* pEventNotification);
        ~AsynchronousGdbSrvController();
        void StopTargetAtRun();

    protected:
        AsynchronousGdbSrvController(_In_ const std::vector<std::wstring> &coreConnectionParameters);

    private:
        HANDLE m_asynchronousCommandThread;
        std::string m_currentAsynchronousCommand;
        std::string m_currentAsynchronousCommandResult;
        startAsynchronousCommandStruct m_AsynchronousCmd;

        static DWORD CALLBACK AsynchronousCommandThreadBody(LPVOID p);
        int GetBreakPointSize();

        std::vector<bool> m_breakpointSlots;
        std::vector<bool> m_dataBreakpointSlots;
        bool m_isAsynchronousCmdStopReplyPacket;
    };
}