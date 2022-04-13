//----------------------------------------------------------------------------
//
// KDCommandLogger.h
//
// A helper class that shows KD.EXE commands being executed.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once
#include "KDController.h"

class KDCommandLogger : public KDControllerLib::IKDTextHandler
{
public:
    KDCommandLogger(bool allocateConsole)
        : m_consoleAllocated(allocateConsole)
    {
        if (allocateConsole)
        {
            AllocConsole();
        }

        SetConsoleCP(CP_ACP);
        m_standardOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTitle(_T("Blind KD - please close when done debugging"));
        
        COORD newConsoleSize = {100, SHRT_MAX - 1};
        SetConsoleScreenBufferSize(m_standardOutput, newConsoleSize);

        SMALL_RECT windowSize = {0, 0, newConsoleSize.X - 1, 49};
        SetConsoleWindowInfo(m_standardOutput, TRUE, &windowSize);
    }

    ~KDCommandLogger()
    {
        if (m_consoleAllocated)
        {
            FreeConsole();
        }
    }

public:
    void HandleText(KDControllerLib::KDTextType textType, _In_z_ const char *pText)
    {
        if (textType == KDControllerLib::KDTextType::Command)
        {
            SetConsoleTextAttribute(m_standardOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        }
        else
        {
            SetConsoleTextAttribute(m_standardOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }

        assert(pText != nullptr);
        DWORD done;
        WriteFile(m_standardOutput, pText, static_cast<DWORD>(strlen(pText)), &done, nullptr);
        assert(done == strlen(pText));
        WriteFile(m_standardOutput, "\n", 1, &done, nullptr);
        assert(done == 1);
    }


private:
    bool m_consoleAllocated;
    HANDLE m_standardOutput;
};