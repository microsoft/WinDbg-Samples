//----------------------------------------------------------------------------
//
// XmlDataHelpers.h
//
// Helpers to handle xml tag and attributes.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#pragma once
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

using namespace GdbSrvControllerLib;
using namespace std;
// Tag-attribute maximum length.
const size_t C_MAX_ATTR_LENGTH = (256 + 1);

//  This type indicate the exdi target description
//  Used to select the current target that will be debugged
//  This type indicate the Exdi component configuration data.
typedef struct
{
    WCHAR currentTargetName[C_MAX_ATTR_LENGTH];      //  Name of the current target selected for debugging
                                                     //  from the set of all GDB targets supported by GDB server client
} ConfigExdiTargets;

//  Target to be processed
typedef struct
{
    WCHAR targetName[C_MAX_ATTR_LENGTH];             //  HW debugger Target Name that contains the GDB server as front end
    bool isTargetSelected;                           //  Flag to indicate if the target that needs to be debugged has been selected
} ConfigExdiTarget;

//  This type indicate the Exdi component configuration data.
typedef struct
{
    wstring agentNamePacket;        //  Agent name
    wstring uuid;                   //  Class identifier.
    bool fDisplayCommPackets;       //  Flag if set then we display the communication packets characters.
    bool fDebuggerSessionByCore;	//	Flag if set then we do debug only by core processor, so step and continue
                                    //	commands happen only on one core at the time. If not set then we 
                                    //	let all cores run when we do step/continue commands.
    bool fExceptionThrowEnabled;    //  Allow throwing exception by the Exdi COM server.
                                    //  Used to disallow throwing exceptions when memory failures occur.
    wstring qSupportedPacket;       //  GDB server supported, if this empty then will send the default "qsupported" packet
} ConfigExdiData;

//  This type indicates the Target data.
typedef struct
{
    TargetArchitecture targetArchitecture; //  The target architecture.
    DWORD targetFamily;             //  The target architecture.
    unsigned numberOfCores;         //  Number of cores of the target processor CPU.
    bool fEnabledIntelFpSseContext; //  Flag if set then the Intel floating SSE context is processed.
    DWORD64 heuristicChunkSize;     //  Chunk size used by the heurisitic scanning memory mechanism.
    wstring targetDescriptionFileName; //  Target description file name.
} ConfigExdiTargetData;

//  This type indicates the GdbServer specific data.
typedef struct
{
    bool fMultiCoreGdbServer;       //  Flag if set then we support multi-core connections with the DbgServer, so
                                    //  there will be one GdbServer launched for each core CPU.
    size_t maxServerPacketLength;   //  Maximum GdbServer packet length.
    int maxConnectAttempts;         //  Connect session maximum attempts
    int sendTimeout;                //  Send RSP packet timeout
    int receiveTimeout;             //  Receive timeout
    vector<wstring> coreConnectionParameters;  //  Connection string (hostname-ip:port) for each GdbServer core instance.
} ConfigGdbServerData;

//  This type indicates the GdbServer memory extended command
typedef struct
{
    bool fGdbSpecialMemoryCommand;      //  if Flag set then GDB server support extended commands implemented by the GDB server
    bool fGdbPhysicalMemoryCommand;     //  if Flag set then GDB server support an extended command for reading physical memory
    bool fGdbSupervisorMemoryCommand;   //  if Flag set then GDB server support an extended command for reading supervisor memory
    bool fGdbHypervisorMemoryCommand;   //  if Flag set then GDB server support an extended command for reading hypervisor memory
    bool fGdbSpecialMemoryRegister;     //  if Flag set then GDB server support an extended command for reading special registers
    bool fGdbSystemRegistersGdbMonitor; //  if Flag set then GDB server support an extended command for reading system registers via GDB monitor command
    bool fGdbSystemRegisterDecoding;    //  if Flag set then the GDB server support reading system registers w/o encoding format.
} ConfigGdbServerMemoryCommands;

//  Type describes the vector Register structure
typedef vector<RegistersStruct> vectorRegister;

//  This type indicates the GDB server registers
typedef struct
{
    TargetArchitecture registerSet; //  The register set architecture.
    std::unique_ptr<wstring> featureNameSupported;  //  Identifier for the feature name supported, so we can avoid failing
                                        //  processing in case that there is an item that is not supported in the feature
                                        //  registers, all - means all feature tags will be processed, other
                                        //  sys/banked - process only system register feature, this matches with the name
                                        //  populated in the feature name sent by the GDB server file.
    std::wstring featureName;       //  This field will be filled only for register files sent by the GDB server.
                                    //  and this describe the name of the GDBserver-entity-arch-reg.type
    std::unique_ptr <vectorRegister> spRegisterCoreSet; //  Vector containing the target architecture core registers
    std::unique_ptr <vectorRegister> spRegisterSystemSet; //  Vector containing the target architecture system registers
} ConfigExdiGdServerRegisters;

//
//  This type indicates the system register mapping between
//  Register name and access code.
//
typedef struct
{
    TargetArchitecture systemRegArchitecture; //  The register set architecture.
    std::unique_ptr <systemRegistersMapType> spSysRegisterMap; //  Map containing the system register mapping to reg. access code
} ConfigSystemRegMapAccessCode;

//  Type describe the target description xml file
//  contain the list of the files with the register description
typedef struct
{
    TargetArchitecture registerGroupArchitecture; //  The name of the target architecture for the received target description file.
    bool isTargetTagEmpty;                        //  Flag indicates if the name of the target architecture tag has been processed.
    std::unique_ptr <targetDescriptionFilesMap> registerGroupFiles; //  Map containing the target description files
} ConfigTargetDescriptionFile;

//  This type indicates the data structure that will be created after processing
//  the Exdi-GdbServer input xml config file.
typedef struct
{
    ConfigExdiTargets gdbCurrentTargetName;  //  Target name that needs to be selected for debugging
    ConfigExdiTarget gdbTargetName;          //  Name of the processed GDB target
    ConfigExdiData component;                //  Component data
    ConfigExdiTargetData target;             //  Target data
    ConfigGdbServerData gdbServer;           //  Server data
    ConfigGdbServerMemoryCommands gdbMemoryCommands; //  Extended memory commands supported by the GDB server
    ConfigTargetDescriptionFile file;                //  List of register files contained in the target description.
    ConfigExdiGdServerRegisters gdbServerRegisters;  //  Register data
    ConfigSystemRegMapAccessCode systemRegisterMap;  //  Map system register to its access code.
} ConfigExdiGdbSrvData;

//  Attribute name handler types.
typedef  bool (*pXmlAttrValueHandler)(_In_ PCWSTR pAttrValue,
    _In_ size_t fieldOffset,
    _In_ size_t totalSizeOfStruct,
    _In_ size_t numberOfElements,
    _Out_ void* pInputData);

//  List-node element structure.
typedef struct
{
    LIST_ENTRY  token;
    //  Attribute pair localName="value"
    wstring localName;
    wstring value;
} AttrList_NodeElem_Struct, * PAttrList_NodeElem_Struct;

//  List Tag-Attributes
typedef struct
{
    PCWSTR tagName;
    //  Each list node will be allocated as a AttrList_NodeElem_Struct structure.
    LIST_ENTRY attrPair;
} TAG_ATTR_LIST;

//  This structure indicates the mapping between the local name attribute value and its corresponding handler.
typedef struct
{
    PCWSTR pTagName;                //  Tag name
    PCWSTR pLocalName;              //  Attribute name
    pXmlAttrValueHandler pHandler;  //  Handler for processing the attribute value
    size_t outStructFieldOffset;    //  This is the field offset used for updating the out structure
    size_t structFieldNumberOfElements; // This is the filed number of elements
} XML_ATTRNAME_HANDLER_STRUCT;

#pragma region XML general helpers

//  Functions to process tag & attributes for internally defined xml files
class XmlDataHelpers
{
public:

    static inline bool IsExdiGdbTargetsDataTag(_In_ PCWSTR pTagName);
    static inline bool IsExdiGdbTargetDataTag(_In_ PCWSTR pTagName);
    static inline bool IsCurrentTarget(_In_ PCWSTR pTargetToSelect, _In_ PCWSTR pCurrentTarget);
    static inline bool IsExdiGdbServerConfigDataTag(_In_ PCWSTR pTagName);
    static inline bool IsExdiGdbServerTargetDataTag(_In_ PCWSTR pTagName);
    static inline bool IsGdbServerConnectionParametersTag(_In_ PCWSTR pTagName);
    static inline bool IsGdbServerRegistersTag(_In_ PCWSTR pTagName);
    static inline bool IsGdbServerValueTag(_In_ PCWSTR pTagName);
    static inline bool IsGdbServerMemoryCommands(_In_ PCWSTR pTagName);
    static inline bool IsGdbRegisterEntryTag(_In_ PCWSTR pTagName);
    static void ReportXmlExceptionCode(_In_ PCWSTR pMessage, _In_ DWORD exceptCode);
    static void ReportExceptionError(_In_ PCSTR pMessage);
    static bool XmlGetStringValue(_In_z_ PCWSTR pAttrValue, _In_ size_t fieldOffset,
        _In_ size_t totalSizeOfStruct, _In_ size_t numberOfElements,
        _Out_writes_(totalSizeOfStruct) void* pOutData);
    static TargetArchitecture GetTargetGdbServerArchitecture(_In_ PCWSTR pDataString);
    static DWORD GetTargetGdbServerFamily(_In_ PCWSTR pDataString);
    static PCWSTR GetXmlErrorMsg(HRESULT hr);
    static void ReportXmlError(_In_ PCWSTR pMessage);
    static HRESULT GetXmlTagAttributeValues(_In_ TAG_ATTR_LIST* const pTagAttrList,
        _In_ const XML_ATTRNAME_HANDLER_STRUCT* pMap,
        _In_ size_t mapSize, size_t maxSizeOfOutStructData,
        _Out_writes_bytes_(maxSizeOfOutStructData) void* pOutData);
    static HRESULT HandleTagAttributeList(_In_ TAG_ATTR_LIST* const pTagAttrList,
        _Out_ ConfigExdiGdbSrvData* pConfigTable);
};

//  Functions to process tag & attributes for GDB server received xml register related files
class XmlDataGdbServerRegisterFile
{
public:
    static inline bool IsTargetDescriptionFile(_In_ PCWSTR pTagName);
    static inline bool IsRegisterFileReference(_In_ PCWSTR pTagName);
    static inline bool IsFeatureRegisterFile(_In_ PCWSTR pTagName);
    static inline bool IsRegisterFileEntry(_In_ PCWSTR pTagName);
    static bool SetFileTargetArchitecture(_In_ PCWSTR pTagValue, _Out_ ConfigExdiGdbSrvData* pConfigTable);
    static bool SetRegistersByTargetFile(_In_ TAG_ATTR_LIST* const pTagAttrList,
        _Out_ ConfigExdiGdbSrvData* pConfigTable);
    static bool HandleTargetFileTags(_In_ TAG_ATTR_LIST* const pTagAttrList,
        _Out_ ConfigExdiGdbSrvData* pConfigTable);
};

//  Functions to process tag & attributes for the system register mapping file
class XmlDataSystemRegister
{
public:
    static inline bool IsSystemRegisterMapElement(_In_ PCWSTR pTagName);
    static inline bool IsSystemRegisterMapTag(_In_ PCWSTR pTagName);
    static inline bool IsSystemRegistersTag(_In_ PCWSTR pTagName);
    static inline bool IsSystemRegisterEntryTag(_In_ PCWSTR pTagName);
    static bool HandleMapSystemRegAccessCode(_In_ TAG_ATTR_LIST* const pTagAttrList,
        _Out_ ConfigExdiGdbSrvData* pConfigTable);
private:
    static const ULONG c_numberOfAccessCodeFields = 5;
    static inline bool IsRegisterPresent(_In_ const string& regOrder,
        _In_ std::unique_ptr <systemRegistersMapType>& spSysRegisterMap);
};

#pragma endregion

