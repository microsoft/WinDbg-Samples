//----------------------------------------------------------------------------
//
// cfgExdiGdbSrvHelper.h
//
// Helper for reading the Exdi-GdbServer configuration file.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------
#pragma once
#ifndef CFGEXDIGDBSRVHELPER_H
#define CFGEXDIGDBSRVHELPER_H
#include <atlbase.h> 
#include <string>
#include <memory>
#include <vector>
#include "GdbSrvControllerLib.h"

using namespace std;
using namespace GdbSrvControllerLib;

//=============================================================================
// Public defines and typedefs
//=============================================================================

class ConfigExdiGdbServerHelper final
{
    public:
        static ConfigExdiGdbServerHelper & GetInstanceCfgExdiGdbServer(_In_opt_ PCWSTR pXmlConfigFile);
        TargetArchitecture GetTargetArchitecture();
        DWORD GetTargetFamily();
        bool GetDisplayCommPacketsCharacters();
        bool GetDebuggerSessionByCore();
        bool GetIntelSseContext();
        void GetExdiComponentUuid(_Out_ wstring & uuid);
        unsigned GetNumberOfCores();
        bool GetMultiCoreGdbServer();
        size_t GetMaxServerPacketLength();
        int GetMaxConnectAttempts();
        int GetSendPacketTimeout();
        int GetReceiveTimeout();
        void GetGdbServerConnectionParameters(_Out_ vector<wstring> & coreConnections);
        void GetExdiComponentAgentNamePacket(_Out_ wstring & uuid);
        bool IsExceptionThrowEnabled();

    private:
        ConfigExdiGdbServerHelper(_In_opt_ PCWSTR pXmlConfigFile);
        ~ConfigExdiGdbServerHelper();
        class ConfigExdiGdbServerHelperImpl;
        std::unique_ptr <ConfigExdiGdbServerHelperImpl> m_pConfigExdiGdbServerHelperImpl;
};

#endif // CFGEXDIGDBSRVHELPER
