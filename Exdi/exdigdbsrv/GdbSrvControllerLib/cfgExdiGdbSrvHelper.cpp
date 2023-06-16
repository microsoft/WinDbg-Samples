//----------------------------------------------------------------------------
//
// cfgExdiGdbSrvHelper.cpp
//
// Helper for reading the Exdi-GdbServer configuration file.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include <tchar.h>
#include <Strsafe.h>
#include <vector>
#include <exception>
#include <comdef.h> 
#include <xmllite.h>
#include "HandleHelpers.h"
#include "GdbSrvControllerLib.h"
#include "XmlDataHelpers.h"
#include "cfgExdiGdbSrvHelper.h"

using namespace GdbSrvControllerLib;
using namespace std;

//=============================================================================
// Private defines and typedefs
//=============================================================================

class ConfigExdiGdbServerHelper::ConfigExdiGdbServerHelperImpl
{
    public:
    ConfigExdiGdbServerHelperImpl::ConfigExdiGdbServerHelperImpl(): m_XmlLiteReader(nullptr), m_IStream(nullptr), m_XmlConfigBuffer(nullptr)
    {
        ZeroMemory(&m_ExdiGdbServerData, sizeof(ConfigExdiGdbSrvData));
    }

    ConfigExdiGdbServerHelperImpl::~ConfigExdiGdbServerHelperImpl()
    {
    }

    //  This function will read the file/default memory and load the 
    //  table with the values.
    bool ConfigExdiGdbServerHelperImpl::ReadConfigFile(_In_opt_ PCWSTR pXmlConfigFile)
    {
        bool isReadDone = false;

        __try
        {
            //  Create the XML reader
            HRESULT hr = InitHelper();
            if (SUCCEEDED(hr))
            {
                //  Create the stream (file/memory)
                hr = CreateStream(pXmlConfigFile);
                if (SUCCEEDED(hr))
                {
                    hr = ReadStream();
                    if (SUCCEEDED(hr))
                    {
                        isReadDone = true;
                    }
                    TerminateHelper();
                }
            }
            if (!isReadDone)
            {
                //  Report the HRESULT error here
                XmlDataHelpers::ReportXmlError(XmlDataHelpers::GetXmlErrorMsg(hr));
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            XmlDataHelpers::ReportXmlExceptionCode(L"A Windows Exception was catched in the function ConfigExdiGdbServerHelper::ReadConfigFile().\n",
                GetExceptionCode());
            throw;
        }
        return isReadDone;
    }

    inline void ConfigExdiGdbServerHelperImpl::GetTargetDescriptionFileName(_Out_ wstring & fileName)
    {
        fileName = m_ExdiGdbServerData.target.targetDescriptionFileName;
    }

    inline void ConfigExdiGdbServerHelperImpl::GetRegisterGroupFile(_In_ RegisterGroupType fileType, _Out_ wstring & fileName)
    {
        auto it = m_ExdiGdbServerData.file.registerGroupFiles->find(fileType);
        if (it == m_ExdiGdbServerData.file.registerGroupFiles->end())
        {
            throw _com_error(E_INVALIDARG);
        }
        fileName = it->second;
    }

    inline bool ConfigExdiGdbServerHelperImpl::IsRegisterGroupFileAvailable(_In_ RegisterGroupType fileType)
    {
        if (m_ExdiGdbServerData.file.registerGroupFiles.get() == nullptr)
        {
            return false;
        }
        auto it = m_ExdiGdbServerData.file.registerGroupFiles->find(fileType);
        if (it == m_ExdiGdbServerData.file.registerGroupFiles->end())
        {
            return false;
        }
        return true;
    }

    inline TargetArchitecture ConfigExdiGdbServerHelperImpl::GetRegisterGroupArchitecture()
    {
        return  m_ExdiGdbServerData.file.registerGroupArchitecture;
    }

    inline void ConfigExdiGdbServerHelperImpl::GetExdiComponentAgentNamePacket(_Out_ wstring & packetName)
    {
        packetName = m_ExdiGdbServerData.component.agentNamePacket;
    }

    inline void ConfigExdiGdbServerHelperImpl::GetRequestQSupportedPacket(_Out_ wstring& requestPacket)
    {
        requestPacket = m_ExdiGdbServerData.component.qSupportedPacket;
    }

    inline void ConfigExdiGdbServerHelperImpl::GetExdiComponentUuid(_Out_ wstring & uuid)
    {
        uuid = m_ExdiGdbServerData.component.uuid;
    }

    inline bool ConfigExdiGdbServerHelperImpl::GetDisplayCommPacketsCharacters()
    {
        return m_ExdiGdbServerData.component.fDisplayCommPackets;
    }

    inline bool ConfigExdiGdbServerHelperImpl::GetDebuggerSessionByCore()
    {
        return m_ExdiGdbServerData.component.fDebuggerSessionByCore;
    }

    inline TargetArchitecture ConfigExdiGdbServerHelperImpl::GetTargetArchitecture()
    {
        return m_ExdiGdbServerData.target.targetArchitecture;
    }

    inline void ConfigExdiGdbServerHelperImpl::SetTargetArchitecture(_In_ TargetArchitecture targetArch)
    {
        m_ExdiGdbServerData.target.targetArchitecture = targetArch;
    }

    inline DWORD ConfigExdiGdbServerHelperImpl::GetTargetFamily()
    {
        return m_ExdiGdbServerData.target.targetFamily;
    }

    inline unsigned ConfigExdiGdbServerHelperImpl::GetNumberOfCores()
    {
        return m_ExdiGdbServerData.target.numberOfCores;
    }

    inline bool ConfigExdiGdbServerHelperImpl::GetIntelSseContext()
    {
        return m_ExdiGdbServerData.target.fEnabledIntelFpSseContext;
    }

    inline DWORD64 ConfigExdiGdbServerHelperImpl::GetHeuristicScanMemorySize()
    {
        return m_ExdiGdbServerData.target.heuristicChunkSize;
    }

    inline bool ConfigExdiGdbServerHelperImpl::GetMultiCoreGdbServer()
    {
        return m_ExdiGdbServerData.gdbServer.fMultiCoreGdbServer;
    }

    inline size_t ConfigExdiGdbServerHelperImpl::GetMaxServerPacketLength()
    {
        return m_ExdiGdbServerData.gdbServer.maxServerPacketLength;
    }

    inline int ConfigExdiGdbServerHelperImpl::GetMaxConnectAttempts()
    {
        return m_ExdiGdbServerData.gdbServer.maxConnectAttempts;
    }

    inline int ConfigExdiGdbServerHelperImpl::GetSendPacketTimeout()
    {
        return m_ExdiGdbServerData.gdbServer.sendTimeout;
    }

    inline int ConfigExdiGdbServerHelperImpl::GetReceiveTimeout()
    {
        return m_ExdiGdbServerData.gdbServer.receiveTimeout;
    }

    inline void ConfigExdiGdbServerHelperImpl::GetGdbServerConnectionParameters(_Out_ vector<wstring> & coreConnections)
    {
        coreConnections = m_ExdiGdbServerData.gdbServer.coreConnectionParameters;
    }

    inline bool ConfigExdiGdbServerHelperImpl::IsExceptionThrowEnabled()
    {
        return m_ExdiGdbServerData.component.fExceptionThrowEnabled;
    }

    inline bool ConfigExdiGdbServerHelperImpl::IsForcedLegacyResumeStepMode()
    {
        return m_ExdiGdbServerData.component.fForcedLegacyResumeStepCommands;
    }

    inline TargetArchitecture ConfigExdiGdbServerHelperImpl::GetLastGdbServerRegisterArchitecture() const 
    {
        return m_ExdiGdbServerData.gdbServerRegisters.registerSet.back();
    }

    inline void ConfigExdiGdbServerHelperImpl::GetGdbServerRegisters(_Out_ unique_ptr<vector<RegistersStruct>> * spRegisters)
    {
        for (auto const & arch : m_ExdiGdbServerData.gdbServerRegisters.registerSet)
        {
            if (arch == m_ExdiGdbServerData.target.targetArchitecture)
            {
                auto it = m_ExdiGdbServerData.gdbServerRegisters.spRegisterCoreSet->find(arch);
                if (it == m_ExdiGdbServerData.gdbServerRegisters.spRegisterCoreSet->end())
                {
                    break;
                }
                *spRegisters = move(it->second);
                break;
            }
        }
   }

    inline void ConfigExdiGdbServerHelperImpl::GetGdbServerSystemRegisters(
        _Out_ unique_ptr<vector<RegistersStruct>> * spRegisters)
    {

        for (auto const& arch : m_ExdiGdbServerData.gdbServerRegisters.registerSet)
        {
            if (arch == m_ExdiGdbServerData.target.targetArchitecture)
            {
                auto it = m_ExdiGdbServerData.gdbServerRegisters.spRegisterSystemSet->find(arch);
                if (it == m_ExdiGdbServerData.gdbServerRegisters.spRegisterSystemSet->end())
                {
                    break;
                }
                *spRegisters = move(it->second);
                break;
            }
        }
    }

    inline bool ConfigExdiGdbServerHelperImpl::IsSystemRegistersAvailable()
    {
        if (m_ExdiGdbServerData.gdbServerRegisters.spRegisterSystemSet != nullptr)
        {
            for (auto const& arch : m_ExdiGdbServerData.gdbServerRegisters.registerSet)
            {
                if (arch == m_ExdiGdbServerData.target.targetArchitecture)
                {
                    auto it = m_ExdiGdbServerData.gdbServerRegisters.spRegisterSystemSet->find(arch);
                    if (it != m_ExdiGdbServerData.gdbServerRegisters.spRegisterSystemSet->end())
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void ConfigExdiGdbServerHelperImpl::GetGdbServerTargetName(_Out_ wstring& targetName) const
    {
        targetName = m_ExdiGdbServerData.gdbTargetName.targetName;
    }

    inline bool ConfigExdiGdbServerHelperImpl::IsSupportedSpecialMemoryCommand() const
    {
        return m_ExdiGdbServerData.gdbMemoryCommands.fGdbSpecialMemoryCommand;
    }

    inline bool ConfigExdiGdbServerHelperImpl::IsSupportedPhysicalMemoryCommand() const
    {
        return m_ExdiGdbServerData.gdbMemoryCommands.fGdbPhysicalMemoryCommand;
    }
    
    inline bool ConfigExdiGdbServerHelperImpl::IsSupportedSupervisorMemoryCommand() const
    {
        return m_ExdiGdbServerData.gdbMemoryCommands.fGdbSupervisorMemoryCommand;
    }
    
    inline bool ConfigExdiGdbServerHelperImpl::IsSupportedHypervisorMemoryCommand() const
    {
        return m_ExdiGdbServerData.gdbMemoryCommands.fGdbHypervisorMemoryCommand;
    }

    inline bool ConfigExdiGdbServerHelperImpl::IsSupportedSpecialMemoryRegister() const
    {
        return m_ExdiGdbServerData.gdbMemoryCommands.fGdbSpecialMemoryRegister;
    }

    inline bool ConfigExdiGdbServerHelperImpl::IsSupportedSystemRegistersGdbMonitor() const
    {
        return m_ExdiGdbServerData.gdbMemoryCommands.fGdbSystemRegistersGdbMonitor;
    }

    inline bool ConfigExdiGdbServerHelperImpl::IsSupportedSystemRegisterDecoding() const
    {
        return m_ExdiGdbServerData.gdbMemoryCommands.fGdbSystemRegisterDecoding;
    }

    //  set an XML buffer to parse
    inline void SetXmlBufferToParse(_In_ PCWSTR pXmlConfigBuffer)
    {
        m_XmlConfigBuffer = pXmlConfigBuffer; 
    }

    inline void ConfigExdiGdbServerHelperImpl::GetSystemRegistersMapAccessCode(
        _Out_ unique_ptr<SystemRegistersMapType> * spMapSystemRegs)
    {

        for (auto const& arch : m_ExdiGdbServerData.systemRegisterMap.systemRegArchitecture)
        {
            if (arch == m_ExdiGdbServerData.target.targetArchitecture)
            {
                auto it = m_ExdiGdbServerData.systemRegisterMap.spSysRegisterMap->find(arch);
                if (it == m_ExdiGdbServerData.systemRegisterMap.spSysRegisterMap->end())
                {
                    break;
                }
                *spMapSystemRegs = move(it->second);
                break;
            }
        }
    }

    inline bool ConfigExdiGdbServerHelperImpl::GetTreatSwBpAsHwBp() const
    {
        return m_ExdiGdbServerData.component.fTreatSwBpAsHwBp;
    }

    private:
    CComPtr<IXmlReader> m_XmlLiteReader;
    CComPtr<IStream> m_IStream;
    ConfigExdiGdbSrvData m_ExdiGdbServerData;
    PCWSTR m_XmlConfigBuffer;

    //  Xml helper related functionality
    inline bool IsMemoryXmlBuffer(_In_opt_ PCWSTR pXmlConfigFile) {return (pXmlConfigFile == nullptr);}

    //  set an XML buffer to parse
    inline PCWSTR GetXmlBufferToParse() {return m_XmlConfigBuffer;}

    bool GetPrevProcessTagElementStatus() { return m_ExdiGdbServerData.file.isTargetTagEmpty; }
    void SetPrevProcessTagElementDone() { m_ExdiGdbServerData.file.isTargetTagEmpty = false; }

    //  ConfigExdiGdbServerHelper function
    HRESULT ConfigExdiGdbServerHelperImpl::InitHelper()
    {
        // Create IXmlReader object.
        HRESULT hr = ::CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(&m_XmlLiteReader), nullptr);
        if (SUCCEEDED(hr))
        {
            // Set IXmlReader properties.
            hr = m_XmlLiteReader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Parse);
        }
        if (!SUCCEEDED(hr))
        {
            TerminateHelper();
        }
        return hr;
    }

    //  Terminate the helper
    HRESULT ConfigExdiGdbServerHelperImpl::TerminateHelper()
    {
        m_XmlLiteReader.Release();
        m_IStream.Release();
        return S_OK;
    }

    LPCWSTR ConfigExdiGdbServerHelperImpl::GetXmlBuffer()
    {
        //  We won't implement it by using a memory xml buffer.
        return GetXmlBufferToParse();
    }

    HRESULT ConfigExdiGdbServerHelperImpl::CreateStream(_In_opt_ PCWSTR pXmlConfigFile)
    {
        HRESULT hr = S_OK;

        //  Is there any passed file path?
        if (IsMemoryXmlBuffer(pXmlConfigFile))
        {
            //  Create a memory stream
            LPCWSTR pBuffer = GetXmlBuffer();
            assert(pBuffer != nullptr);

            size_t bufferSize = 0;
            // Find buffer length.
            hr = StringCbLengthW(pBuffer,(STRSAFE_MAX_CCH * sizeof(WCHAR)), &bufferSize);
            if (SUCCEEDED(hr))
            {
                // Create stream from buffer.
                m_IStream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(pBuffer),
                                 static_cast<UINT>(bufferSize)));
                if (m_IStream == nullptr)
                {
                    hr = E_FAIL;
                }
            }
        }
        else
        {
            //  Create a file stream
            hr = SHCreateStreamOnFileW(pXmlConfigFile, STGM_READ, &m_IStream);
        }
        if (SUCCEEDED(hr))
        {
            // Set the input source of the XML document to be parsed.
            hr = m_XmlLiteReader->SetInput(m_IStream);
            if (SUCCEEDED(hr))
            {
                // Set maximum dept allowed levels of nesting in elements.
                hr = m_XmlLiteReader->SetProperty(XmlReaderProperty_MaxElementDepth, 50); 
            }
        }
        if (!SUCCEEDED(hr))
        {
            TerminateHelper();
        }
        return hr;
    }

    //  Clear the nodes from the list. 
    void ConfigExdiGdbServerHelperImpl::ClearTagAttributesList(_Inout_ TAG_ATTR_LIST * const pTagAttrList)
    {
        assert(pTagAttrList != nullptr);

        PLIST_ENTRY next = pTagAttrList->attrPair.Flink;
        if (next != nullptr) 
        {
            while (next != &pTagAttrList->attrPair) 
            {
                PAttrList_NodeElem_Struct pElem = CONTAINING_RECORD(next, AttrList_NodeElem_Struct, token);
                assert(pElem != nullptr);
                RemoveEntryList(next);
                next = pElem->token.Flink;
                delete pElem;
            }
        }
    }

    HRESULT ConfigExdiGdbServerHelperImpl::ProcessAttributeList(_In_ TAG_ATTR_LIST * const pTagAttrList)
    {
        HRESULT hr = E_FAIL;
        __try
        {
            hr = XmlDataHelpers::HandleTagAttributeList(pTagAttrList, &m_ExdiGdbServerData);
        }
        __finally
        {
            ClearTagAttributesList(pTagAttrList);
        }
        return hr;
    }

    //  Parses the attributes for the current tag
    HRESULT ConfigExdiGdbServerHelperImpl::ParseAttributes(_Inout_ TAG_ATTR_LIST * pTagAttrList)
    {
        assert(pTagAttrList != nullptr);
        HRESULT hr = E_FAIL;
    
        try
        {
            hr = m_XmlLiteReader->MoveToFirstAttribute();
            while (SUCCEEDED(hr))
            {
                PCWSTR pAttribName = nullptr;
                PCWSTR pAttribValue = nullptr;

                HRESULT hr = m_XmlLiteReader->GetLocalName(&pAttribName, nullptr);
                if (SUCCEEDED(hr))
                {
                    hr = m_XmlLiteReader->GetValue(&pAttribValue, nullptr);
                    if (SUCCEEDED(hr))
                    {
                        //  Add a new element to the list
                        PAttrList_NodeElem_Struct pListNode = new (nothrow) AttrList_NodeElem_Struct;
                        assert(pListNode != nullptr);
                        pListNode->localName += pAttribName;
                        pListNode->value += pAttribValue;
                        //  Insert the element in the list
                        InsertTailList(&pTagAttrList->attrPair, &pListNode->token);
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
                if (SUCCEEDED(hr))
                {
                    hr = m_XmlLiteReader->MoveToNextAttribute();
                    if (hr == S_FALSE)
                    {
                        break;
                    }
                }
            } 
        }
        catch (exception & ex)
        {
            XmlDataHelpers::ReportExceptionError(ex.what());
            throw;
        }
        catch(...)
        {
            XmlDataHelpers::ReportXmlError(L"Unrecognized exception happened in ConfigExdiGdbServerHelperImpl::ParseAttributes()\n");
            throw;
        }
        return hr;
    }

    HRESULT ConfigExdiGdbServerHelperImpl::ReadStream()
    {
        HRESULT hr = E_FAIL;
        bool isForcedEnd = false;

        while (!m_XmlLiteReader->IsEOF() && !isForcedEnd)
        {
            XmlNodeType nodeType;
            hr = m_XmlLiteReader->Read(&nodeType);
            if (FAILED(hr) || (hr == S_FALSE))
            {
                break;
            }
            switch(nodeType)
            {
                case XmlNodeType_XmlDeclaration:
                case XmlNodeType_ProcessingInstruction:
                case XmlNodeType_Comment:
                case XmlNodeType_DocumentType:
                    break;
                case XmlNodeType_Text:
                {
                    if (GetPrevProcessTagElementStatus())
                    {
                        PCWSTR pAttribValue = nullptr;

                        hr = m_XmlLiteReader->GetValue(&pAttribValue, nullptr);
                        if (SUCCEEDED(hr))
                        {
                            XmlDataGdbServerRegisterFile::SetFileTargetArchitecture(pAttribValue, &m_ExdiGdbServerData);
                            SetPrevProcessTagElementDone();
                        }
                    }
                }
                break;
                case XmlNodeType_Element:
                {
                    PCWSTR pTagName = nullptr;
                    hr = m_XmlLiteReader->GetLocalName(&pTagName, nullptr);
                    if (SUCCEEDED(hr) && (pTagName != nullptr))
                    {
                        TAG_ATTR_LIST tagAttrList = {};
                        InitializeListHead(&tagAttrList.attrPair);
                        size_t copyLength = wcslen(pTagName) + 1;
                        unique_ptr<WCHAR> pTagNameCopy(new (nothrow) WCHAR[copyLength]);
                        if (pTagNameCopy != nullptr)
                        {
                            ZeroMemory(pTagNameCopy.get(), sizeof(WCHAR) * (copyLength));
                            wcscpy_s(pTagNameCopy.get(), copyLength, pTagName);
                            tagAttrList.tagName = pTagNameCopy.get();

                            //  Parse the XML tag
                            hr = ParseAttributes(&tagAttrList);
                            if (SUCCEEDED(hr))
                            {
                                hr = ProcessAttributeList(&tagAttrList);
                                if (FAILED(hr))
                                {
                                    isForcedEnd = true;
                                    break;
                                }
                            }
                            else
                            {
                                //  Try removing any added element to the list 
                                ClearTagAttributesList(&tagAttrList);
                                isForcedEnd = true;
                            }
                        }
                        else
                        {
                            isForcedEnd = true;
                            hr = E_OUTOFMEMORY;
                        }
                    }
                }
                break;
            }
        }
        return hr;
    }
};

ConfigExdiGdbServerHelper & ConfigExdiGdbServerHelper::GetInstanceCfgExdiGdbServer(_In_opt_ PCWSTR pXmlConfigFile) 
{
    static ConfigExdiGdbServerHelper * pInstance = nullptr;
    if (pInstance == nullptr)
    {
        pInstance = new (nothrow) ConfigExdiGdbServerHelper(pXmlConfigFile);
        if (pInstance == nullptr)
        {
            throw _com_error(ERROR_NOT_ENOUGH_MEMORY);
        }
    }
    return *pInstance;
}

ConfigExdiGdbServerHelper::ConfigExdiGdbServerHelper(_In_opt_ PCWSTR pXmlConfigFile)
{
    m_pConfigExdiGdbServerHelperImpl = unique_ptr<ConfigExdiGdbServerHelperImpl>(new (nothrow) ConfigExdiGdbServerHelperImpl());
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);

    if (!m_pConfigExdiGdbServerHelperImpl->ReadConfigFile(pXmlConfigFile))
    {
        m_pConfigExdiGdbServerHelperImpl = nullptr;
        throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
    }
}

ConfigExdiGdbServerHelper::~ConfigExdiGdbServerHelper(){}

TargetArchitecture ConfigExdiGdbServerHelper::GetTargetArchitecture()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetTargetArchitecture();
}

DWORD ConfigExdiGdbServerHelper::GetTargetFamily()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetTargetFamily();
}

bool ConfigExdiGdbServerHelper::GetDisplayCommPacketsCharacters()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetDisplayCommPacketsCharacters();
}

bool ConfigExdiGdbServerHelper::GetDebuggerSessionByCore()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetDebuggerSessionByCore();
}

bool ConfigExdiGdbServerHelper::GetIntelSseContext()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetIntelSseContext();
}

DWORD64 ConfigExdiGdbServerHelper::GetHeuristicScanMemorySize()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetHeuristicScanMemorySize();
}

void ConfigExdiGdbServerHelper::GetTargetDescriptionFileName(_Out_ wstring & fileName)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetTargetDescriptionFileName(fileName);
}

void ConfigExdiGdbServerHelper::GetExdiComponentAgentNamePacket(_Out_ wstring & packetName)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetExdiComponentAgentNamePacket(packetName);
}

void ConfigExdiGdbServerHelper::GetRequestQSupportedPacket(_Out_ wstring& requestPacket)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetRequestQSupportedPacket(requestPacket);
}


void ConfigExdiGdbServerHelper::GetExdiComponentUuid(_Out_ wstring & uuid)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetExdiComponentUuid(uuid);
}

unsigned ConfigExdiGdbServerHelper::GetNumberOfCores()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetNumberOfCores();
}

bool ConfigExdiGdbServerHelper::GetMultiCoreGdbServer()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetMultiCoreGdbServer();
}

size_t ConfigExdiGdbServerHelper::GetMaxServerPacketLength()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetMaxServerPacketLength();
}

int ConfigExdiGdbServerHelper::GetMaxConnectAttempts()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetMaxConnectAttempts();
}

int ConfigExdiGdbServerHelper::GetSendPacketTimeout()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetSendPacketTimeout();
}

int ConfigExdiGdbServerHelper::GetReceiveTimeout()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetReceiveTimeout();
}

void ConfigExdiGdbServerHelper::GetGdbServerConnectionParameters(_Out_ vector<wstring> & coreConnections)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetGdbServerConnectionParameters(coreConnections);
}

bool ConfigExdiGdbServerHelper::IsExceptionThrowEnabled()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsExceptionThrowEnabled();
}

bool ConfigExdiGdbServerHelper::IsForcedLegacyResumeStepMode()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsForcedLegacyResumeStepMode();
}

void ConfigExdiGdbServerHelper::GetGdbServerRegisters(_Out_ unique_ptr<vector<RegistersStruct>> * spRegisters)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetGdbServerRegisters(spRegisters);
}

void ConfigExdiGdbServerHelper::GetGdbServerSystemRegisters(_Out_ unique_ptr<vector<RegistersStruct>> * spSystemRegisters)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetGdbServerSystemRegisters(spSystemRegisters);
}

TargetArchitecture ConfigExdiGdbServerHelper::GetLastGdbServerRegisterArchitecture()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetLastGdbServerRegisterArchitecture();
}

void ConfigExdiGdbServerHelper::GetGdbServerTargetName(_Out_ wstring& targetName)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetGdbServerTargetName(targetName);
}

bool ConfigExdiGdbServerHelper::IsSupportedSpecialMemoryCommand()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsSupportedSpecialMemoryCommand();
}

bool ConfigExdiGdbServerHelper::IsSupportedPhysicalMemoryCommand()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsSupportedPhysicalMemoryCommand();
}

bool ConfigExdiGdbServerHelper::IsSupportedSupervisorMemoryCommand()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsSupportedSupervisorMemoryCommand();
}

bool ConfigExdiGdbServerHelper::IsSupportedHypervisorMemoryCommand()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsSupportedHypervisorMemoryCommand();
}

bool ConfigExdiGdbServerHelper::IsSupportedSpecialMemoryRegister()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsSupportedSpecialMemoryRegister();
}

bool ConfigExdiGdbServerHelper::IsSupportedSystemRegistersGdbMonitor()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsSupportedSystemRegistersGdbMonitor();
}

bool ConfigExdiGdbServerHelper::IsSupportedSystemRegisterDecoding()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsSupportedSystemRegisterDecoding();
}

void ConfigExdiGdbServerHelper::SetXmlBufferToParse(_In_ PCWSTR pXmlConfigFile)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->SetXmlBufferToParse(pXmlConfigFile);
    if (!m_pConfigExdiGdbServerHelperImpl->ReadConfigFile(nullptr))
    {
        throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
    }
}

bool ConfigExdiGdbServerHelper::ReadConfigFile(_In_ PCWSTR pXmlConfigFile)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr && pXmlConfigFile != nullptr);

    if (!m_pConfigExdiGdbServerHelperImpl->ReadConfigFile(pXmlConfigFile))
    {
        throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
    }
    return true;
}

bool ConfigExdiGdbServerHelper::IsRegisterGroupFileAvailable(_In_ RegisterGroupType fileType)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsRegisterGroupFileAvailable(fileType);
}

void ConfigExdiGdbServerHelper::GetRegisterGroupFile(_In_ RegisterGroupType fileType, _Out_ wstring & fileName)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetRegisterGroupFile(fileType, fileName);
}

TargetArchitecture ConfigExdiGdbServerHelper::GetRegisterGroupArchitecture()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetRegisterGroupArchitecture();
}

bool ConfigExdiGdbServerHelper::IsSystemRegistersAvailable()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->IsSystemRegistersAvailable();
}

void ConfigExdiGdbServerHelper::GetSystemRegistersMapAccessCode(_Out_ unique_ptr<SystemRegistersMapType> * spMapSystemRegs)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetSystemRegistersMapAccessCode(spMapSystemRegs);
}

bool ConfigExdiGdbServerHelper::GetTreatSwBpAsHwBp()
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    return m_pConfigExdiGdbServerHelperImpl->GetTreatSwBpAsHwBp();
}

void ConfigExdiGdbServerHelper::SetTargetArchitecture(_In_ TargetArchitecture targetArch)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->SetTargetArchitecture(targetArch);
}