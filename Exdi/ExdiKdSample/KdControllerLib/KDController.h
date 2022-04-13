//----------------------------------------------------------------------------
//
// KDController.cpp
//
// A class allowing running KD.EXE and sending commands to it.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once
#include "BufferedStreamReader.h"
#include <string>
#include <map>
#include <regex>
#include "HandleHelpers.h"

namespace KDControllerLib
{
    enum class KDTextType
    {
        Command,
        CommandOutput,
    };

    class IKDTextHandler
    {
    public:
        virtual void HandleText(_In_ KDTextType textType, _In_z_ const char *pText)=0;
        virtual ~IKDTextHandler(){}
    };

    class KDController
    {
    public:
        virtual ~KDController();

        typedef ULONGLONG AddressType;

        //The class will own the text handler
        void SetTextHandler(_In_ IKDTextHandler *pHandler);

        virtual std::string ExecuteCommand(_In_ LPCSTR pCommand);

		//Specify -1 to use 'current' processor as defined by KD
        std::map<std::string, std::string> QueryAllRegisters(_In_ unsigned processorNumber);
        void SetRegisters(_In_ unsigned processorNumber, _In_ const std::map<std::string, AddressType> &registerValues);

        SimpleCharBuffer ReadMemory(_In_ AddressType address, _In_ size_t size);

		unsigned GetProcessorCount();
		AddressType GetKPCRAddress(_In_ unsigned processorNumber);

        static ULONGLONG ParseRegisterValue(_In_ const std::string &stringValue);

        std::string GetEffectiveMachine(_Out_opt_ std::string * pTargetResponse);

        unsigned GetLastKnownActiveCpu() { return m_lastKnownActiveCpu; }

        void ShutdownKD();

    protected:
        //Initializes the KD Controller given handles to a running KD.EXE process.
        //Do not invoke directly. Use Create() method instead.
        //NOTE: The created object will own the handles.
        KDController(_In_ HANDLE processHandle, _In_ HANDLE stdInput, _In_ HANDLE stdOutput);

        void WaitForInitialPrompt();

    private:
        HandleWrapper m_jobHandle;
        ValidHandleWrapper m_processHandle;
        ValidHandleWrapper m_stdInput;
        ValidHandleWrapper m_stdOutput;

        BufferedStreamReader m_stdoutReader;

        IKDTextHandler *m_pTextHandler;

        std::regex m_kdPromptRegex;

		unsigned m_cachedProcessorCount;
        unsigned m_lastKnownActiveCpu;

        std::string ReadStdoutUntilDelimiter();
    };
}
