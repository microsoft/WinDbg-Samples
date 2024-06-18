//----------------------------------------------------------------------------
//
// AsynchronousKdController.cpp
//
// An extension of the KDController class that allows running certain commands
// (e.g. running target) asynchronously.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "AsynchronousKDController.h"

#include "KDController.h"
#include "ExceptionHelpers.h"
#include "HandleHelpers.h"

using namespace KDControllerLib;

AsynchronousKDController *AsynchronousKDController::Create(_In_ LPCTSTR pDebuggingToolsPath, 
                                                           _In_ LPCTSTR pConnectionArguments)
{
    if (pDebuggingToolsPath == nullptr || pConnectionArguments == nullptr)
    {
        throw _com_error(E_POINTER);
    }

    TCHAR kdExe[MAX_PATH];
    TCHAR kdCommandLine[MAX_PATH];
    _sntprintf_s(kdExe, _TRUNCATE, _T("%s\\kd.exe"), pDebuggingToolsPath);
    _sntprintf_s(kdCommandLine, _TRUNCATE, _T("\"%s\\kd.exe\" %s"), pDebuggingToolsPath, pConnectionArguments);

    HandleWrapper stdInputHandle;
    HandleWrapper stdOutputHandle;

    HandleWrapper remoteStdInputHandle;
    HandleWrapper remoteStdOutputHandle;

    SECURITY_ATTRIBUTES allowHandleInheritanceAttributes = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    if (!CreatePipe(&remoteStdInputHandle, &stdInputHandle, &allowHandleInheritanceAttributes, 0))
    {
        throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
    }

    if (!CreatePipe(&stdOutputHandle, &remoteStdOutputHandle, &allowHandleInheritanceAttributes, 0))
    {
        throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
    }

    STARTUPINFO startupInformation = {sizeof(STARTUPINFO), };
    PROCESS_INFORMATION processInformation;

    startupInformation.dwFlags |= STARTF_USESTDHANDLES;
    startupInformation.hStdError = INVALID_HANDLE_VALUE;

    startupInformation.hStdInput = remoteStdInputHandle.Get();
    startupInformation.hStdOutput = remoteStdOutputHandle.Get();

    if (!CreateProcess(nullptr, kdCommandLine, nullptr, nullptr, TRUE, 0, 
                       nullptr, nullptr, &startupInformation, &processInformation))
    {
        throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
    }

    HandleWrapper::CloseAndInvalidate(&processInformation.hThread);

    AsynchronousKDController *pResult = new AsynchronousKDController(processInformation.hProcess, stdInputHandle.Detach(), stdOutputHandle.Detach());
    pResult->WaitForInitialPrompt();
    return pResult;
}

AsynchronousKDController::AsynchronousKDController(_In_ HANDLE processHandle, _In_ HANDLE stdInput, _In_ HANDLE stdOutput)
    : KDController(processHandle, stdInput, stdOutput)
    , m_asynchronousCommandThread(nullptr)
{
}

AsynchronousKDController::~AsynchronousKDController()
{
    if (IsAsynchronousCommandInProgress())
    {
        ShutdownKD();
        WaitForSingleObject(m_asynchronousCommandThread, INFINITE);
    }

    if (m_asynchronousCommandThread != nullptr)
    {
        CloseHandle(m_asynchronousCommandThread);
        m_asynchronousCommandThread = nullptr;
    }
}

unsigned AsynchronousKDController::CreateCodeBreakpoint(_In_ AddressType address)
{
    unsigned slot = static_cast<unsigned>(-1);
    for(unsigned i = 0; i < m_breakpointSlots.size(); i++)
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

    char command[256];
    _snprintf_s(command, _TRUNCATE, "bp%d 0x%I64x ; .echo", slot, address);
    std::string reply = ExecuteCommand(command);

    //KD does not seem to report any error conditions when setting breakpoints by addresses
    UNREFERENCED_PARAMETER(reply);

    m_breakpointSlots[slot] = true;
    return slot;
}

void AsynchronousKDController::DeleteCodeBreakpoint(_In_ unsigned breakpointNumber)
{
    if (breakpointNumber >= m_breakpointSlots.size() || !m_breakpointSlots[breakpointNumber])
    {
        throw std::exception("Trying to delete non-existing breakpoint");
    }

    char command[256];
    _snprintf_s(command, _TRUNCATE, "bc %d ; .echo", breakpointNumber);
    ExecuteCommand(command);
    m_breakpointSlots[breakpointNumber] = false;
}

std::string AsynchronousKDController::ExecuteCommand(_In_ LPCSTR pCommand)
{
    if (IsAsynchronousCommandInProgress())
    {
        throw std::exception("Cannot execute KD command while an asynchronous command is in progress (e.g. target is running)\r\n");
    }

    return KDController::ExecuteCommand(pCommand);
}

void AsynchronousKDController::StartAsynchronousCommand(_In_ LPCSTR pCommand)
{
    assert(pCommand != nullptr);
    if (IsAsynchronousCommandInProgress())
    {
        throw std::exception("Cannot execute KD command while an asynchronous command is in progress (e.g. target is running).");
    }

    if (m_asynchronousCommandThread != nullptr)
    {
        CloseHandle(m_asynchronousCommandThread);
    }

    //At this point no other thread is using these, so no lock is needed
    m_currentAsynchronousCommand = pCommand;
    m_currentAsynchronousCommandResult.clear();

    DWORD threadId = 0;
    m_asynchronousCommandThread = CreateThread(nullptr, 0, AsynchronousCommandThreadBody, this, 0, &threadId);
    if (m_asynchronousCommandThread == nullptr)
    {
        throw std::exception("Failed to start asynchronous command thread.");
    }
}

bool AsynchronousKDController::IsAsynchronousCommandInProgress()
{
    return m_asynchronousCommandThread != nullptr &&
           WaitForSingleObject(m_asynchronousCommandThread, 0) != WAIT_OBJECT_0;
}

bool AsynchronousKDController::GetAsynchronousCommandResult(_In_ DWORD timeoutInMilliseconds, _Out_opt_ std::string *pResult)
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

DWORD AsynchronousKDController::AsynchronousCommandThreadBody(LPVOID p)
{
    AsynchronousKDController *pController = reinterpret_cast<AsynchronousKDController *>(p);
    assert(pController != nullptr);

    pController->m_currentAsynchronousCommandResult = 
        pController->KDController::ExecuteCommand(pController->m_currentAsynchronousCommand.c_str());
    return 0;
}

void AsynchronousKDController::StartStepCommand(unsigned processorNumber)
{
    if (processorNumber != -1)
    {
        char processorSelectionCommand[32];
        _snprintf_s(processorSelectionCommand, _TRUNCATE, "~%ds ; .echo", processorNumber);
        ExecuteCommand(processorSelectionCommand);
    }
    StartAsynchronousCommand("t");
}

void AsynchronousKDController::StartRunCommand()
{
    StartAsynchronousCommand("g");
}

