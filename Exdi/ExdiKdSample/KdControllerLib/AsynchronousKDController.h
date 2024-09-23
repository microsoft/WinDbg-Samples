//----------------------------------------------------------------------------
//
// AsynchronousKdController.h
//
// An extension of the KDController class that allows running certain commands
// (e.g. running target) asynchronously.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once
#include <vector>

#include "KDController.h"

namespace KDControllerLib
{
    class AsynchronousKDController : public KDController
    {
    public:
        static AsynchronousKDController *Create(_In_ LPCTSTR pDebuggingToolsPath, 
                                                _In_ LPCTSTR pConnectionArguments);

        virtual std::string ExecuteCommand(_In_ LPCSTR pCommand) override;
        
        void StartAsynchronousCommand(_In_ LPCSTR pCommand);
        bool IsAsynchronousCommandInProgress();
        bool GetAsynchronousCommandResult(_In_ DWORD timeoutInMilliseconds, _Out_opt_ std::string *pResult);

        //High-level commands
        void StartStepCommand(unsigned processorNumber);
        void StartRunCommand();

        unsigned CreateCodeBreakpoint(_In_ AddressType address);
        void DeleteCodeBreakpoint(_In_ unsigned breakpointNumber);

        ~AsynchronousKDController();
    protected:
        AsynchronousKDController(_In_ HANDLE processHandle, _In_ HANDLE stdInput, _In_ HANDLE stdOutput);

    private:
        HANDLE m_asynchronousCommandThread;
        std::string m_currentAsynchronousCommand;
        std::string m_currentAsynchronousCommandResult;

        static DWORD CALLBACK AsynchronousCommandThreadBody(LPVOID p);

        std::vector<bool> m_breakpointSlots;
    };
}