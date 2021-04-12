//----------------------------------------------------------------------------
//
// commandLogger.h
//
// A helper class that shows commands being executed.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#pragma once
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <assert.h>
#include "GdbSrvControllerLib.h"

class CommandLogger : public GdbSrvControllerLib::IGdbSrvTextHandler
{
public:
    CommandLogger(bool allocateConsole) : m_consoleAllocated(allocateConsole)
    {
        if (allocateConsole)
        {
            AllocConsole();
        }

        SetConsoleCP(CP_ACP);
        m_standardOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTitle(_T("ExdiGdbServer"));
        
        COORD newConsoleSize = {100, SHRT_MAX - 1};
        SetConsoleScreenBufferSize(m_standardOutput, newConsoleSize);

        SMALL_RECT windowSize = {0, 0, newConsoleSize.X - 1, 49};
        SetConsoleWindowInfo(m_standardOutput, TRUE, &windowSize);
    }

    ~CommandLogger()
    {
        if (m_consoleAllocated)
        {
            FreeConsole();
        }
    }

public:
    void HandleText(_In_ GdbSrvControllerLib::GdbSrvTextType textType, _In_reads_bytes_(readSize) const char * pText,
                    _In_ size_t readSize)
    {
        if (textType == GdbSrvControllerLib::GdbSrvTextType::Command)
        {
            SetConsoleTextAttribute(m_standardOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        }
        else if (textType == GdbSrvControllerLib::GdbSrvTextType::CommandOutput)
        {
            SetConsoleTextAttribute(m_standardOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
        else
        {
            SetConsoleTextAttribute(m_standardOutput, FOREGROUND_RED | FOREGROUND_RED | FOREGROUND_INTENSITY);
        }

        assert(pText != nullptr);
        DWORD done;
        WriteFile(m_standardOutput, pText, static_cast<DWORD>(readSize), &done, nullptr);
        assert(done == readSize);
        WriteFile(m_standardOutput, "\n", 1, &done, nullptr);
        assert(done == 1);
    }


private:
    bool m_consoleAllocated;
    HANDLE m_standardOutput;
};