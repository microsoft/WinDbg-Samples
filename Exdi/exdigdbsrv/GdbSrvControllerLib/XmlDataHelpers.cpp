//----------------------------------------------------------------------------
//
// XmlDataHelpers.cpp
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
#include "TargetArchitectureHelpers.h"
#include "XmlDataHelpers.h"

using namespace GdbSrvControllerLib;
using namespace std;

//  This type indicates the xmllite error code notification structure.
typedef struct
{
    HRESULT hr;
    PCWSTR pMessage;
} XMLLITE_ERROR_STRUCT;

//  This type indicate the exdi target description
//  Used to select the current target that will be debugged
//  This type indicate the Exdi component configuration data.
typedef struct
{
    WCHAR currentGdbTargetName[C_MAX_ATTR_LENGTH];      //  Name of the current target selected for debugging
                                                        //  from the set of all GDB targets supported by GDB server client
} ConfigExdiGdbTargetsDataEntry;

//  Target to be processed
typedef struct
{
    WCHAR gdbTargetName[C_MAX_ATTR_LENGTH];             //  HW debugger Target Name that contains the GDB server as front end
} ConfigExdiGdbTargetDataEntry;

typedef struct
{
    WCHAR fMultiCoreGdbServer[C_MAX_ATTR_LENGTH];       //  Flag if set then we support multi-core connections with the DbgServer, so
                                                        //  There will be one GdbServer launched for each core CPU.
    WCHAR maxServerPacketLength[C_MAX_ATTR_LENGTH];     //  Maximum GdbServer packet length.
    WCHAR maxConnectAttempts[C_MAX_ATTR_LENGTH];        //  Connect session maximum attempts
    WCHAR sendTimeout[C_MAX_ATTR_LENGTH];               //  Send RSP packet timeout
    WCHAR receiveTimeout[C_MAX_ATTR_LENGTH];            //  Receive timeout
    WCHAR coreConnectionParameter[C_MAX_ATTR_LENGTH];   //  Connection string (hostname-ip:port) for each GdbServer core instance.
} ConfigGdbServerDataEntry;

//  This type indicates the GdbServer memory extended command
typedef struct
{
    WCHAR fGdbSpecialMemoryCommand[C_MAX_ATTR_LENGTH];      //  if Flag set then GDB server support extended commands implemented by the GDB server
    WCHAR fGdbPhysicalMemoryCommand[C_MAX_ATTR_LENGTH];     //  if Flag set then GDB server support an extended command for reading physical memory
    WCHAR fGdbSupervisorMemoryCommand[C_MAX_ATTR_LENGTH];   //  if Flag set then GDB server support an extended command for reading supervisor memory
    WCHAR fGdbHypervisorMemoryCommand[C_MAX_ATTR_LENGTH];   //  if Flag set then GDB server support an extended command for reading hypervisor memory
    WCHAR fGdbSpecialMemoryRegister[C_MAX_ATTR_LENGTH];     //  if Flag set then GDB server support an extended command for reading special registers
    WCHAR fGdbSystemRegistersGdbMonitor[C_MAX_ATTR_LENGTH]; //  if Flag set then GDB server support an extended command for reading system registers via GDB monitor command
    WCHAR fGdbSystemRegisterDecoding[C_MAX_ATTR_LENGTH];    //  if Flag set then the GDB server support reading system registers w/o encoding format.
} ConfigExdiGdbServerMemoryCommandsEntry;

typedef struct
{
    WCHAR RegisterArchitecture[C_MAX_ATTR_LENGTH];      //  The architecture of the set of registers that follows
    WCHAR FeatureNameSupported[C_MAX_ATTR_LENGTH];      //  The feature identifier : all-all feature files, 
                                                        //  sys/banked-system registers, core/general-main set of core registers, 
    WCHAR SystemRegistersStart[C_MAX_ATTR_LENGTH];      //  First Core Register order/number that identifies a System register
                                                        //  used to create the system register map for
                                                        //  GDB server that sends the System register together with Core registers
    WCHAR SystemRegistersEnd[C_MAX_ATTR_LENGTH];        //  Last Core Register order/number that identifies the last System register
                                                        //  The System registers would be grouped in order in the core register layout.
                                                        //  simdfp/float-floating point
    WCHAR RegisterName[C_MAX_ATTR_LENGTH];              //  Register name
    WCHAR RegisterOrder[C_MAX_ATTR_LENGTH];             //  Register order
    WCHAR RegisterSize[C_MAX_ATTR_LENGTH];              //  Register size
} ConfigExdiGdServerRegistersEntry;

typedef struct
{
    WCHAR agentNamePacket[C_MAX_ATTR_LENGTH];           //  Agent name
    WCHAR uuid[C_MAX_ATTR_LENGTH];                      //  Class identifier.
    WCHAR fDisplayCommPackets[C_MAX_ATTR_LENGTH];       //  Flag if set then we display the communication packets characters.
    WCHAR fDebuggerSessionByCore[C_MAX_ATTR_LENGTH];    //  Flag if set then we do debug only by core processor, so step and continue
                                                        //  commands happen only on one core at the time. If not set then we 
                                                        //  let all cores run when we do step/continue commands.
    WCHAR fExceptionThrowEnabled[C_MAX_ATTR_LENGTH];    //  Allow throwing exception by the Exdi COM server.
                                                        //  Used to disallow throwing exceptions when memory failures occur.
    WCHAR qSupportedPacket[C_MAX_ATTR_LENGTH];          //  qSupported packet to send to the dbg server, if empty then just "qSupported"
} ConfigExdiDataEntry;

typedef struct
{
    WCHAR targetArchitecture[C_MAX_ATTR_LENGTH];        //  The target architecture.
    WCHAR targetFamily[C_MAX_ATTR_LENGTH];              //  The target family
    WCHAR numberOfCores[C_MAX_ATTR_LENGTH];             //  Number of cores of the target processor CPU.
    WCHAR fEnabledIntelFpSseContext[C_MAX_ATTR_LENGTH]; //  Flag if set then the Intel floating SSE context is processed.
    WCHAR heuristicChunkSize[C_MAX_ATTR_LENGTH];        //  Chunk Size used by the heuristic scanning memory mechanism.
    WCHAR targetDescriptionFileName[C_MAX_ATTR_LENGTH]; //  Target description filename
} ConfigExdiTargetDataEntry;

typedef struct
{
    WCHAR targetArchitecture[C_MAX_ATTR_LENGTH]; //  Target architecture described by the register files (i.e format target aarch64,x86-x64)
    WCHAR registerFile[C_MAX_ATTR_LENGTH]; //  System register file
} ConfigTargetDescriptionFileEntry;

//
//  Target file register description file.
//    Each register is represented as an element with this form:
//
//    <reg name = "name"
//        bitsize = "size"
//        [regnum = "num"]
//        [save - restore = "save-restore"]
//        [type = "type"]
//        [group = "group"] / >
//
//
typedef struct
{
    WCHAR featureName[C_MAX_ATTR_LENGTH];               //  Feature Name: Describes the org. that implements the register target file
    WCHAR RegisterName[C_MAX_ATTR_LENGTH];              //  Name:The register’s name; it must be unique within the target description.
    WCHAR RegistBitSize[C_MAX_ATTR_LENGTH];             //  Bitsize: The register’s size, in bits.
    WCHAR RegisterNum[C_MAX_ATTR_LENGTH];               //  Number: The number is equal to the element index for a register vector
    WCHAR RegisterSaveRestore[C_MAX_ATTR_LENGTH];       //  SaveRestore:Whether the register should be preserved across inferior function calls; 
                                                        //              this must be either yes or no.  default is yes, which is appropriate for most 
                                                        //              registers except for some system control registers; 
    WCHAR RegisterType[C_MAX_ATTR_LENGTH];              //  Type: The type of the register.It may be a predefined type, a type defined in the current feature, or 
                                                        //        one of the special types intand float. int is an integer type of the correct size for bitsize, 
                                                        //        and float is a floating point type(in the architecture’s normal floating point format) of the 
                                                        //        correct size for bitsize.The default is int.
    WCHAR RegisterGroup[C_MAX_ATTR_LENGTH];             //  Group: The register group to which this register belongs.It can be one of the standard register groups general, float, 
                                                        //         vector or an arbitrary string.Group names should be limited to alphanumeric characters.
                                                        //         If a group name is made up of multiple words the words may be separated by hyphens; 
                                                        //         e.g.special - group or ultra - special - group.
                                                        //         If no group is specified, GDB will not display the register in info registers
} ConfigTargetRegisterDescriptionFileEntry;

//
//  Map between system register to access code.
//
typedef struct
{
    WCHAR RegisterArchitecture[C_MAX_ATTR_LENGTH];      //  The architecture of the set of registers that follows
    WCHAR RegisterName[C_MAX_ATTR_LENGTH];              //  Register name
    WCHAR AccessCode[C_MAX_ATTR_LENGTH];                //  Register access code
} ConfigSystemRegMapAccessCodeEntry;

//  Temporal buffer
unique_ptr<vector<size_t>> XmlDataHelpers::m_spSystemRegsRange;

//  Xml file tags and Attributes 
const WCHAR exdiTargets[] = L"ExdiTargets";
const WCHAR exdiTarget[] = L"ExdiTarget";
const WCHAR currentTarget[] = L"CurrentTarget";
const WCHAR targetName[] = L"Name";
const WCHAR exdiGdbServerConfigData[] = L"ExdiGdbServerConfigData";
const WCHAR exdiGdbServerTargetData[] = L"ExdiGdbServerTargetData";
const WCHAR gdbServerConnectionParameters[] = L"GdbServerConnectionParameters";
const WCHAR gdbServerConnectionValue[] = L"Value";
const WCHAR gdbServerAgentNamePacket[] = L"agentNamePacket";
const WCHAR gdbQSupportedPacket[] = L"qSupportedPacket";
const WCHAR gdbServerUuid[] = L"uuid";
const WCHAR displayCommPackets[] = L"displayCommPackets";
const WCHAR debuggerSessionByCore[] = L"debuggerSessionByCore";
const WCHAR enableThrowExceptions[] = L"enableThrowExceptionOnMemoryErrors";
const WCHAR targetArchitectureName[] = L"targetArchitecture";
const WCHAR targetFamilyName[] = L"targetFamily";
const WCHAR numberOfCoresName[] = L"numberOfCores";
const WCHAR enableSseContextName[] = L"enableSseContext";
const WCHAR heuristicChunkSizeName[] = L"heuristicScanSize";
const WCHAR targetDescriptionFileName[] = L"targetDescriptionFile";
const WCHAR multiCoreGdbServer[] = L"MultiCoreGdbServerSessions";
const WCHAR maximumGdbServerPacketLength[] = L"MaximumGdbServerPacketLength";
const WCHAR hostNameAndPort[] = L"HostNameAndPort";
const WCHAR maximumConnectAttempts[] = L"MaximumConnectAttempts";
const WCHAR sendPacketTimeout[] = L"SendPacketTimeout";
const WCHAR receivePacketTimeout[] = L"ReceivePacketTimeout";
const WCHAR gdbServerRegisters[] = L"ExdiGdbServerRegisters";
const WCHAR gdbRegisterArchitecture[] = L"Architecture";
const WCHAR gdbFeatureNameSupported[] = L"FeatureNameSupported";
const WCHAR gdbSystemRegistersStart[] = L"SystemRegistersStart";
const WCHAR gdbSystemRegistersEnd[] = L"SystemRegistersEnd";
const WCHAR gdbRegisterEntry[] = L"Entry";
const WCHAR registerName[] = L"Name";
const WCHAR registerOrder[] = L"Order";
const WCHAR registerSize[] = L"size";
const WCHAR gdbMemoryCommands[] = L"ExdiGdbServerMemoryCommands";
const WCHAR gdbSpecialMemory[] = L"GdbSpecialMemoryCommand";
const WCHAR gdbPhysicalMemory[] = L"PhysicalMemory";
const WCHAR gdbSupervisorMemory[] = L"SupervisorMemory";
const WCHAR gdbHypervisorMemory[] = L"HypervisorMemory";
const WCHAR gdbSpecialMemoryRegister[] = L"SpecialMemoryRegister";
const WCHAR gdbSystemRegistersGdbMonitor[] = L"SystemRegistersGdbMonitor";
const WCHAR gdbSystemRegisterDecoding[] = L"SystemRegisterDecoding";
const WCHAR targetFileArchitectureName[] = L"architecture";
//const WCHAR includeTargetFile[] = L"xi:include";
const WCHAR includeTargetFile[] = L"includeTarget";
const WCHAR hrefTargetFile[] = L"href";
const WCHAR featureTag[] = L"feature";
const WCHAR featureName[] = L"name";
const WCHAR regTag[] = L"reg";
const WCHAR regAttrName[] = L"name";
const WCHAR regAttrBitsize[] = L"bitsize";
const WCHAR regAttrNumber[] = L"regnum";
const WCHAR regAttrSaveRestore[] = L"save-restore";
const WCHAR regAttrtype[] = L"type";
const WCHAR regAttrGroup[] = L"group";
//
//  system register map file
//
const WCHAR tagSystemRegisterMap[] = L"SystemRegisterMap";
const WCHAR tagSystemRegisters[] = L"SystemRegisters";
const WCHAR tagRegisterEntry[] = L"register";
const WCHAR attributeAccessCode[] = L"AccessCode";

//  Target that needs to be selected - handler map
const XML_ATTRNAME_HANDLER_STRUCT attrExdiTargetsHandlerMap[] =
{
    {exdiTargets, currentTarget, XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdbTargetsDataEntry, currentGdbTargetName), C_MAX_ATTR_LENGTH},
};

//  Target GDB name - handler map
const XML_ATTRNAME_HANDLER_STRUCT attrExdiTargetHandlerMap[] =
{
    {exdiTarget, targetName, XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdbTargetDataEntry, gdbTargetName), C_MAX_ATTR_LENGTH},
};

//  General debugger information - handler map
const XML_ATTRNAME_HANDLER_STRUCT attrExdiServerHandlerMap[] =
{
    {exdiGdbServerConfigData, gdbServerAgentNamePacket, XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, agentNamePacket), C_MAX_ATTR_LENGTH},
    {exdiGdbServerConfigData, gdbServerUuid,            XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, uuid), C_MAX_ATTR_LENGTH},
    {exdiGdbServerConfigData, displayCommPackets,       XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, fDisplayCommPackets), C_MAX_ATTR_LENGTH},
    {exdiGdbServerConfigData, debuggerSessionByCore,    XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, fDebuggerSessionByCore), C_MAX_ATTR_LENGTH},
    {exdiGdbServerConfigData, enableThrowExceptions,    XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, fExceptionThrowEnabled), C_MAX_ATTR_LENGTH},
    {exdiGdbServerConfigData, gdbQSupportedPacket,      XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, qSupportedPacket), C_MAX_ATTR_LENGTH},
};

//  Attribute name - handler map for the GdbServer server tag info
const XML_ATTRNAME_HANDLER_STRUCT attrNameServerTarget[] =
{
    {exdiGdbServerTargetData, targetArchitectureName, XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, targetArchitecture), C_MAX_ATTR_LENGTH},
    {exdiGdbServerTargetData, targetFamilyName,       XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, targetFamily), C_MAX_ATTR_LENGTH},
    {exdiGdbServerTargetData, numberOfCoresName,      XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, numberOfCores), C_MAX_ATTR_LENGTH},
    {exdiGdbServerTargetData, enableSseContextName,   XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, fEnabledIntelFpSseContext), C_MAX_ATTR_LENGTH},
    {exdiGdbServerTargetData, heuristicChunkSizeName, XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, heuristicChunkSize), C_MAX_ATTR_LENGTH},
    {exdiGdbServerTargetData, targetDescriptionFileName, XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, targetDescriptionFileName), C_MAX_ATTR_LENGTH},
};

//  General debugger information - handler map
const XML_ATTRNAME_HANDLER_STRUCT attrExdiServerConnection[] =
{
    {gdbServerConnectionParameters, multiCoreGdbServer,           XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, fMultiCoreGdbServer), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionParameters, maximumGdbServerPacketLength, XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, maxServerPacketLength), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionParameters, maximumConnectAttempts,       XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, maxConnectAttempts), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionParameters, sendPacketTimeout,            XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, sendTimeout), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionParameters, receivePacketTimeout,         XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, receiveTimeout), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionValue, hostNameAndPort,                   XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, coreConnectionParameter), C_MAX_ATTR_LENGTH},
};

//  General GDB server memory command attributes
const XML_ATTRNAME_HANDLER_STRUCT attrExdiGdbServerMemoryCommands[] =
{
    {gdbMemoryCommands, gdbSpecialMemory,              XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdbServerMemoryCommandsEntry, fGdbSpecialMemoryCommand), C_MAX_ATTR_LENGTH},
    {gdbMemoryCommands, gdbPhysicalMemory,             XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdbServerMemoryCommandsEntry, fGdbPhysicalMemoryCommand), C_MAX_ATTR_LENGTH},
    {gdbMemoryCommands, gdbSupervisorMemory,           XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdbServerMemoryCommandsEntry, fGdbSupervisorMemoryCommand), C_MAX_ATTR_LENGTH},
    {gdbMemoryCommands, gdbHypervisorMemory,           XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdbServerMemoryCommandsEntry, fGdbHypervisorMemoryCommand), C_MAX_ATTR_LENGTH},
    {gdbMemoryCommands, gdbSpecialMemoryRegister,      XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdbServerMemoryCommandsEntry, fGdbSpecialMemoryRegister), C_MAX_ATTR_LENGTH},
    {gdbMemoryCommands, gdbSystemRegistersGdbMonitor,  XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdbServerMemoryCommandsEntry, fGdbSystemRegistersGdbMonitor), C_MAX_ATTR_LENGTH},
    {gdbMemoryCommands, gdbSystemRegisterDecoding,     XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdbServerMemoryCommandsEntry, fGdbSystemRegisterDecoding), C_MAX_ATTR_LENGTH},
};

// Attribute array describing the registers entries
const XML_ATTRNAME_HANDLER_STRUCT attrGdbServerRegisters[] =
{
    {gdbServerRegisters, gdbRegisterArchitecture,  XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdServerRegistersEntry, RegisterArchitecture), C_MAX_ATTR_LENGTH},
    {gdbServerRegisters, gdbFeatureNameSupported,  XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdServerRegistersEntry, FeatureNameSupported), C_MAX_ATTR_LENGTH},
    {gdbServerRegisters, gdbSystemRegistersStart,  XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdServerRegistersEntry, SystemRegistersStart), C_MAX_ATTR_LENGTH},
    {gdbServerRegisters, gdbSystemRegistersEnd,    XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdServerRegistersEntry, SystemRegistersEnd), C_MAX_ATTR_LENGTH},
    {gdbRegisterEntry,   registerName,             XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdServerRegistersEntry, RegisterName), C_MAX_ATTR_LENGTH},
    {gdbRegisterEntry,   registerOrder,            XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdServerRegistersEntry, RegisterOrder), C_MAX_ATTR_LENGTH},
    {gdbRegisterEntry,   registerSize,             XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigExdiGdServerRegistersEntry, RegisterSize), C_MAX_ATTR_LENGTH},
};

//  Attribute name - describe the target description file as it is received by the GDB server (there is no default value included for this item in the exdiConfigData.xml file)
const XML_ATTRNAME_HANDLER_STRUCT attrTargetDescriptionArchitectureName[] =
{
    {targetFileArchitectureName, targetFileArchitectureName, XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigTargetDescriptionFileEntry, targetArchitecture), C_MAX_ATTR_LENGTH},
};

const XML_ATTRNAME_HANDLER_STRUCT attrTargetDescriptionRegisterFile[] =
{
    {includeTargetFile, hrefTargetFile, XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigTargetDescriptionFileEntry, registerFile), C_MAX_ATTR_LENGTH},
};

//  Attribute array describing the target file registers entries
const XML_ATTRNAME_HANDLER_STRUCT attrRegistersFile[] =
{
    {featureTag,         featureName,              XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigTargetRegisterDescriptionFileEntry, featureName), C_MAX_ATTR_LENGTH},
    {regTag,             regAttrName,              XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigTargetRegisterDescriptionFileEntry, RegisterName), C_MAX_ATTR_LENGTH},
    {regTag,             regAttrBitsize,           XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigTargetRegisterDescriptionFileEntry, RegistBitSize), C_MAX_ATTR_LENGTH},
    {regTag,             regAttrNumber,            XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigTargetRegisterDescriptionFileEntry, RegisterNum), C_MAX_ATTR_LENGTH},
    {regTag,             regAttrSaveRestore,       XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigTargetRegisterDescriptionFileEntry, RegisterSaveRestore), C_MAX_ATTR_LENGTH},
    {regTag,             regAttrtype,              XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigTargetRegisterDescriptionFileEntry, RegisterType), C_MAX_ATTR_LENGTH},
    {regTag,             regAttrGroup,             XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigTargetRegisterDescriptionFileEntry, RegisterGroup), C_MAX_ATTR_LENGTH},
};

// Attribute array describing the mapping between system register and access code
const XML_ATTRNAME_HANDLER_STRUCT attrMapSystemRegisterAccessCode[] =
{
    {tagSystemRegisters, gdbRegisterArchitecture,  XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigSystemRegMapAccessCodeEntry, RegisterArchitecture), C_MAX_ATTR_LENGTH},
    {tagRegisterEntry,   registerName,             XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigSystemRegMapAccessCodeEntry, RegisterName), C_MAX_ATTR_LENGTH},
    {tagRegisterEntry,   attributeAccessCode,      XmlDataHelpers::XmlGetStringValue, FIELD_OFFSET(ConfigSystemRegMapAccessCodeEntry, AccessCode), C_MAX_ATTR_LENGTH},
};


//  XmlLite error code - text map
const XMLLITE_ERROR_STRUCT xmlLiteErrorMap[] =
{
    { S_OK,                           L"S_OK" },
    { S_FALSE,                        L"S_FALSE" },
    { E_FAIL,                         L"E_FAIL" },
    { E_INVALIDARG,                   L"E_INVALIDARG" },
    { E_OUTOFMEMORY,                  L"E_OUTOFMEMORY" },
    { MX_E_MX,                        L"0xC00CEE00 MX_E_MX ???" },
    { MX_E_INPUTEND,                  L"0xC00CEE01 MX_E_INPUTEND unexpected end of input" },
    { MX_E_ENCODING,                  L"0xC00CEE02 MX_E_ENCODING unrecognized encoding" },
    { MX_E_ENCODINGSWITCH,            L"0xC00CEE03 MX_E_ENCODINGSWITCH unable to switch the encoding" },
    { MX_E_ENCODINGSIGNATURE,         L"0xC00CEE04 MX_E_ENCODINGSIGNATURE unrecognized input signature" },
    { WC_E_WC,                        L"0xC00CEE20 WC_E_WC ???" },
    { WC_E_WHITESPACE,                L"0xC00CEE21 WC_E_WHITESPACE whitespace expected" },
    { WC_E_SEMICOLON,                 L"0xC00CEE22 WC_E_SEMICOLON semicolon expected" },
    { WC_E_GREATERTHAN,               L"0xC00CEE23 WC_E_GREATERTHAN '>' expected" },
    { WC_E_QUOTE,                     L"0xC00CEE24 WC_E_QUOTE quote expected" },
    { WC_E_EQUAL,                     L"0xC00CEE25 WC_E_EQUAL equal expected" },
    { WC_E_LESSTHAN,                  L"0xC00CEE26 WC_E_LESSTHAN wfc: no '<' in attribute value" },
    { WC_E_HEXDIGIT,                  L"0xC00CEE27 WC_E_HEXDIGIT hexadecimal digit expected" },
    { WC_E_DIGIT,                     L"0xC00CEE28 WC_E_DIGIT decimal digit expected" },
    { WC_E_LEFTBRACKET,               L"0xC00CEE29 WC_E_LEFTBRACKET '[' expected" },
    { WC_E_LEFTPAREN,                 L"0xC00CEE2A WC_E_LEFTPAREN '(' expected" },
    { WC_E_XMLCHARACTER,              L"0xC00CEE2B WC_E_XMLCHARACTER illegal xml character" },
    { WC_E_NAMECHARACTER,             L"0xC00CEE2C WC_E_NAMECHARACTER illegal name character" },
    { WC_E_SYNTAX,                    L"0xC00CEE2D WC_E_SYNTAX incorrect document syntax" },
    { WC_E_CDSECT,                    L"0xC00CEE2E WC_E_CDSECT incorrect CDATA section syntax" },
    { WC_E_COMMENT,                   L"0xC00CEE2F WC_E_COMMENT incorrect comment syntax" },
    { WC_E_CONDSECT,                  L"0xC00CEE30 WC_E_CONDSECT incorrect conditional section syntax" },
    { WC_E_DECLATTLIST,               L"0xC00CEE31 WC_E_DECLATTLIST incorrect ATTLIST declaration syntax" },
    { WC_E_DECLDOCTYPE,               L"0xC00CEE32 WC_E_DECLDOCTYPE incorrect DOCTYPE declaration syntax" },
    { WC_E_DECLELEMENT,               L"0xC00CEE33 WC_E_DECLELEMENT incorrect ELEMENT declaration syntax" },
    { WC_E_DECLENTITY,                L"0xC00CEE34 WC_E_DECLENTITY incorrect ENTITY declaration syntax" },
    { WC_E_DECLNOTATION,              L"0xC00CEE35 WC_E_DECLNOTATION incorrect NOTATION declaration syntax" },
    { WC_E_NDATA,                     L"0xC00CEE36 WC_E_NDATA NDATA expected" },
    { WC_E_PUBLIC,                    L"0xC00CEE37 WC_E_PUBLIC PUBLIC expected" },
    { WC_E_SYSTEM,                    L"0xC00CEE38 WC_E_SYSTEM SYSTEM expected" },
    { WC_E_NAME,                      L"0xC00CEE39 WC_E_NAME name expected" },
    { WC_E_ROOTELEMENT,               L"0xC00CEE3A WC_E_ROOTELEMENT one root element " },
    { WC_E_ELEMENTMATCH,              L"0xC00CEE3B WC_E_ELEMENTMATCH wfc: element type match" },
    { WC_E_UNIQUEATTRIBUTE,           L"0xC00CEE3C WC_E_UNIQUEATTRIBUTE wfc: unique attribute spec" },
    { WC_E_TEXTXMLDECL,               L"0xC00CEE3D WC_E_TEXTXMLDECL text/xmldecl not at the beginning of input" },
    { WC_E_LEADINGXML,                L"0xC00CEE3E WC_E_LEADINGXML leading 'xml' " },
    { WC_E_TEXTDECL,                  L"0xC00CEE3F WC_E_TEXTDECL incorrect text declaration syntax" },
    { WC_E_XMLDECL,                   L"0xC00CEE40 WC_E_XMLDECL incorrect xml declaration syntax" },
    { WC_E_ENCNAME,                   L"0xC00CEE41 WC_E_ENCNAME incorrect encoding name syntax" },
    { WC_E_PUBLICID,                  L"0xC00CEE42 WC_E_PUBLICID incorrect public identifier syntax" },
    { WC_E_PESINTERNALSUBSET,         L"0xC00CEE43 WC_E_PESINTERNALSUBSET wfc: pes in internal subset" },
    { WC_E_PESBETWEENDECLS,           L"0xC00CEE44 WC_E_PESBETWEENDECLS wfc: pes between declarations" },
    { WC_E_NORECURSION,               L"0xC00CEE45 WC_E_NORECURSION wfc: no recursion" },
    { WC_E_ENTITYCONTENT,             L"0xC00CEE46 WC_E_ENTITYCONTENT entity content not well formed" },
    { WC_E_UNDECLAREDENTITY,          L"0xC00CEE47 WC_E_UNDECLAREDENTITY wfc: undeclared entity " },
    { WC_E_PARSEDENTITY,              L"0xC00CEE48 WC_E_PARSEDENTITY wfc: parsed entity" },
    { WC_E_NOEXTERNALENTITYREF,       L"0xC00CEE49 WC_E_NOEXTERNALENTITYREF wfc: no external entity references" },
    { WC_E_PI,                        L"0xC00CEE4A WC_E_PI incorrect processing instruction syntax" },
    { WC_E_SYSTEMID,                  L"0xC00CEE4B WC_E_SYSTEMID incorrect system identifier syntax" },
    { WC_E_QUESTIONMARK,              L"0xC00CEE4C WC_E_QUESTIONMARK '?' expected" },
    { WC_E_CDSECTEND,                 L"0xC00CEE4D WC_E_CDSECTEND no ']]>' in element content" },
    { WC_E_MOREDATA,                  L"0xC00CEE4E WC_E_MOREDATA not all chunks of value have been read" },
    { WC_E_DTDPROHIBITED,             L"0xC00CEE4F WC_E_DTDPROHIBITED DTD was found but is prohibited" },
    { WC_E_INVALIDXMLSPACE,           L"0xC00CEE50 WC_E_INVALIDXMLSPACE Invalid xml:space value" },
    { NC_E_NC,                        L"0xC00CEE60 NC_E_NC ???" },
    { NC_E_QNAMECHARACTER,            L"0xC00CEE61 NC_E_QNAMECHARACTER illegal qualified name character" },
    { NC_E_QNAMECOLON,                L"0xC00CEE62 NC_E_QNAMECOLON multiple colons in qualified name" },
    { NC_E_NAMECOLON,                 L"0xC00CEE63 NC_E_NAMECOLON colon in name" },
    { NC_E_DECLAREDPREFIX,            L"0xC00CEE64 NC_E_DECLAREDPREFIX declared prefix" },
    { NC_E_UNDECLAREDPREFIX,          L"0xC00CEE65 NC_E_UNDECLAREDPREFIX undeclared prefix" },
    { NC_E_EMPTYURI,                  L"0xC00CEE66 NC_E_EMPTYURI non default namespace with empty uri" },
    { NC_E_XMLPREFIXRESERVED,         L"0xC00CEE67 NC_E_XMLPREFIXRESERVED \"xml\" prefix is reserved and must have the http://www.w3.org/XML/1998/namespace URI" },
    { NC_E_XMLNSPREFIXRESERVED,       L"0xC00CEE68 NC_E_XMLNSPREFIXRESERVED \"xmlns\" prefix is reserved for use by XML" },
    { NC_E_XMLURIRESERVED,            L"0xC00CEE69 xml namespace URI (http://www.w3.org/XML/1998/namespace) must be assigned only to prefix \"xml\"" },
    { NC_E_XMLNSURIRESERVED,          L"0xC00CEE6A xmlns namespace URI (http://www.w3.org/2000/xmlns/) is reserved and must not be used" },
    { SC_E_SC,                        L"0xC00CEE80 SC_E_SC ???" },
    { SC_E_MAXELEMENTDEPTH,           L"0xC00CEE81 SC_E_MAXELEMENTDEPTH max element depth was exceeded" },
    { SC_E_MAXENTITYEXPANSION,        L"0xC00CEE82 SC_E_MAXENTITYEXPANSION max number of expanded entities was exceeded" },
};

#pragma region XML general helpers

//
//  General XML functions for processing tag & attributes in internally defined xml files
//
inline bool XmlDataHelpers::IsExdiGdbTargetsDataTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, exdiTargets) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataHelpers::IsExdiGdbTargetDataTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, exdiTarget) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataHelpers::IsCurrentTarget(_In_ PCWSTR pTargetToSelect, _In_ PCWSTR pCurrentTarget)
{
    assert(pTargetToSelect != nullptr && pCurrentTarget != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTargetToSelect, pCurrentTarget) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataHelpers::IsExdiGdbServerConfigDataTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, exdiGdbServerConfigData) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataHelpers::IsExdiGdbServerTargetDataTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, exdiGdbServerTargetData) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataHelpers::IsGdbServerConnectionParametersTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, gdbServerConnectionParameters) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataHelpers::IsGdbServerRegistersTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, gdbServerRegisters) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataHelpers::IsGdbServerValueTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, gdbServerConnectionValue) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataHelpers::IsGdbServerMemoryCommands(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);

    bool isDone = false;

    if (_wcsicmp(pTagName, gdbMemoryCommands) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataHelpers::IsGdbRegisterEntryTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, gdbRegisterEntry) == 0)
    {
        isDone = true;
    }
    return isDone;
}

PCWSTR XmlDataHelpers::GetXmlErrorMsg(HRESULT hr)
{
    for (size_t i = 0; i < ARRAYSIZE(xmlLiteErrorMap); ++i)
    {
        if (xmlLiteErrorMap[i].hr == hr)
        {
            return xmlLiteErrorMap[i].pMessage;
        }
    }
    static wchar_t errorMsgBuf[128] = {};

    StringCchPrintfW(errorMsgBuf, 127, L"0x%.8X !! Unknown Error !!", hr);
    return errorMsgBuf;
}

void XmlDataHelpers::ReportXmlError(_In_ PCWSTR pMessage)
{
    assert(pMessage != nullptr);

    MessageBox(0, reinterpret_cast<LPCTSTR>(pMessage), nullptr, MB_ICONERROR);
}

void XmlDataHelpers::ReportXmlExceptionCode(_In_ PCWSTR pMessage, _In_ DWORD exceptCode)
{
    WCHAR message[C_MAX_ATTR_LENGTH] = L"";
    swprintf_s(message, ARRAYSIZE(message), L"%s (exception Code: 0x%x)\n", pMessage, exceptCode);
    ReportXmlError(message);
}

void XmlDataHelpers::ReportExceptionError(_In_ PCSTR pMessage)
{
    assert(pMessage != nullptr);

    size_t messageLength = strlen(pMessage) + 1;
    unique_ptr<WCHAR> pUnicodeMsg(new (nothrow) wchar_t[messageLength]);

    if (pUnicodeMsg != nullptr)
    {
        if (MultiByteToWideChar(CP_ACP, 0, pMessage, -1, pUnicodeMsg.get(), static_cast<int>(messageLength)) != 0)
        {
            ReportXmlError(pUnicodeMsg.get());
        }
        else
        {
            ReportXmlError(L"Failed converting the exception ASCII string to UNICODE\n");
        }
    }
    else
    {
        ReportXmlError(L"Failed allocating memory for the exception message\n");
    }
}

bool XmlDataHelpers::XmlGetStringValue(_In_z_ PCWSTR pAttrValue, _In_ size_t fieldOffset,
    _In_ size_t totalSizeOfStruct, _In_ size_t numberOfElements,
    _Out_writes_(totalSizeOfStruct) void* pOutData)
{
    bool isGet = false;

    try
    {
        assert(pAttrValue != nullptr && pOutData != nullptr);
        BYTE* pStart = (static_cast<BYTE*>(pOutData)) + fieldOffset;
        BYTE* pEnd = (static_cast<BYTE*>(pOutData)) + totalSizeOfStruct;
        PWSTR pString = reinterpret_cast<PWSTR>(pStart);
        size_t leftElements = (reinterpret_cast<PWSTR>(pEnd) - pString);
        if (fieldOffset > totalSizeOfStruct || numberOfElements > leftElements)
        {
            throw exception("The structure field offset does not belong to the current passed in structure");
        }
        wcscpy_s(pString, numberOfElements, pAttrValue);
        isGet = true;
    }
    catch (exception& ex)
    {
        ReportExceptionError(ex.what());
    }
    return isGet;
}

TargetArchitecture XmlDataHelpers::GetTargetGdbServerArchitecture(_In_ PCWSTR pDataString)
{
    assert(pDataString != nullptr);

    TargetArchitecture targetData = UNKNOWN_ARCH;
    if (_wcsicmp(pDataString, L"X86") == 0)
    {
        targetData = X86_ARCH;
    }
    else if (_wcsicmp(pDataString, L"X64") == 0)
    {
        targetData = AMD64_ARCH;
    }
    else if (_wcsicmp(pDataString, L"ARM") == 0)
    {
        targetData = ARM32_ARCH;
    }
    else if (_wcsicmp(pDataString, L"ARM64") == 0)
    {
        targetData = ARM64_ARCH;
    }
    else
    {
        MessageBox(0,
            _T("The configuration file contains an unsupported target architecture."),
            _T("EXDI-GdbServer"),
            MB_ICONERROR);
    }
    return targetData;
}

DWORD XmlDataHelpers::GetTargetGdbServerFamily(_In_ PCWSTR pDataString)
{
    assert(pDataString != nullptr);

    DWORD targetData = PROCESSOR_FAMILY_UNK;
    if (_wcsicmp(pDataString, L"ProcessorFamilyX86") == 0)
    {
        targetData = PROCESSOR_FAMILY_X86;
    }
    else if (_wcsicmp(pDataString, L"ProcessorFamilyX64") == 0)
    {
        targetData = PROCESSOR_FAMILY_X86;
    }
    else if (_wcsicmp(pDataString, L"ProcessorFamilyARM") == 0)
    {
        targetData = PROCESSOR_FAMILY_ARM;
    }
    else if (_wcsicmp(pDataString, L"ProcessorFamilyARM64") == 0)
    {
        targetData = PROCESSOR_FAMILY_ARMV8ARCH64;
    }
    else
    {
        MessageBox(0,
            _T("The configuration file contains an unsupported family target type."),
            _T("EXDI-GdbServer"),
            MB_ICONERROR);
    }
    return targetData;
}

//  Validates the XML tag-Attribute value and get the value from the xml file
HRESULT XmlDataHelpers::GetXmlTagAttributeValues(_In_ TAG_ATTR_LIST* const pTagAttrList,
    _In_ const XML_ATTRNAME_HANDLER_STRUCT* pMap,
    _In_ size_t mapSize, size_t maxSizeOfOutStructData,
    _Out_writes_bytes_(maxSizeOfOutStructData) void* pOutData)
{
    assert(pOutData != nullptr && pMap != nullptr && pTagAttrList != nullptr);
    HRESULT hr = E_FAIL;

    try
    {
        for (size_t i = 0; i < mapSize; ++i, ++pMap)
        {
            if (_wcsicmp(pMap->pTagName, pTagAttrList->tagName) == 0)
            {
                //  Walk through the list of attributes
                PLIST_ENTRY next = pTagAttrList->attrPair.Flink;
                if (next != nullptr)
                {
                    while (next != &pTagAttrList->attrPair)
                    {
                        PAttrList_NodeElem_Struct pElem = CONTAINING_RECORD(next, AttrList_NodeElem_Struct, token);
                        assert(pElem != nullptr);

                        if (_wcsicmp(pMap->pLocalName, pElem->localName.c_str()) == 0)
                        {
                            bool isDone = pMap->pHandler(pElem->value.c_str(),
                                pMap->outStructFieldOffset,
                                maxSizeOfOutStructData,
                                pMap->structFieldNumberOfElements,
                                pOutData);
                            if (isDone)
                            {
                                hr = S_OK;
                            }
                            break;
                        }
                        next = pElem->token.Flink;
                    }
                }
            }
        }
    }
    catch (exception& ex)
    {
        ReportExceptionError(ex.what());
        hr = E_FAIL;
    }
    catch (...)
    {
        ReportXmlError(L"Unrecognized exception happened in ConfigExdiGdbServerHelperImpl::GetXmlTagAttributeValues()\n");
        hr = E_FAIL;
    }
    return hr;
}

HRESULT XmlDataHelpers::HandleTagAttributeList(_In_ TAG_ATTR_LIST* const pTagAttrList,
    _Out_ ConfigExdiGdbSrvData* pConfigTable)
{
    assert(pTagAttrList != nullptr);
    HRESULT hr = E_FAIL;

    try
    {
        bool isSet = false;

        if (IsExdiGdbTargetsDataTag(pTagAttrList->tagName))
        {
            ConfigExdiGdbTargetsDataEntry selectTarget = {};
            hr = GetXmlTagAttributeValues(pTagAttrList, attrExdiTargetsHandlerMap, 
                ARRAYSIZE(attrExdiTargetsHandlerMap),
                sizeof(selectTarget), static_cast<void*>(&selectTarget));
            if (SUCCEEDED(hr))
            {
                wcscpy_s(pConfigTable->gdbCurrentTargetName.currentTargetName,
                    C_MAX_ATTR_LENGTH, selectTarget.currentGdbTargetName);
                isSet = true;
            }
        }
        else if (IsExdiGdbTargetDataTag(pTagAttrList->tagName))
        {
            ConfigExdiGdbTargetDataEntry target = {};
            hr = GetXmlTagAttributeValues(pTagAttrList, attrExdiTargetHandlerMap, 
                ARRAYSIZE(attrExdiTargetHandlerMap),
                sizeof(target), static_cast<void*>(&target));
            if (SUCCEEDED(hr))
            {
                if (IsCurrentTarget(target.gdbTargetName, pConfigTable->gdbCurrentTargetName.currentTargetName))
                {
                    pConfigTable->gdbTargetName.isTargetSelected = true;
                    wcscpy_s(pConfigTable->gdbTargetName.targetName, C_MAX_ATTR_LENGTH, target.gdbTargetName);
                }
                else
                {
                    pConfigTable->gdbTargetName.isTargetSelected = false;
                }
                isSet = true;
            }
        }
        else if (XmlDataGdbServerRegisterFile::IsTargetDescriptionFile(pTagAttrList->tagName) ||
            XmlDataGdbServerRegisterFile::IsRegisterFileReference(pTagAttrList->tagName))
        {
            isSet = XmlDataGdbServerRegisterFile::HandleTargetFileTags(pTagAttrList, pConfigTable);
        }
        else if (XmlDataGdbServerRegisterFile::IsFeatureRegisterFile(pTagAttrList->tagName) ||
            XmlDataGdbServerRegisterFile::IsRegisterFileEntry(pTagAttrList->tagName))
        {
            isSet = XmlDataGdbServerRegisterFile::SetRegistersByTargetFile(pTagAttrList, pConfigTable);
        }
        else if (XmlDataSystemRegister::IsSystemRegisterMapElement(pTagAttrList->tagName))
        {
            isSet = XmlDataSystemRegister::HandleMapSystemRegAccessCode(pTagAttrList, pConfigTable);
        }

        if (pConfigTable->gdbTargetName.isTargetSelected)
        {
            if (IsExdiGdbServerConfigDataTag(pTagAttrList->tagName))
            {
                ConfigExdiDataEntry exdiData = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrExdiServerHandlerMap, ARRAYSIZE(attrExdiServerHandlerMap),
                    sizeof(ConfigExdiDataEntry), static_cast<void*>(&exdiData));
                if (SUCCEEDED(hr))
                {
                    pConfigTable->component.agentNamePacket = exdiData.agentNamePacket;
                    pConfigTable->component.uuid = exdiData.uuid;
                    pConfigTable->component.fDisplayCommPackets = (_wcsicmp(exdiData.fDisplayCommPackets, L"yes") == 0) ? true : false;
                    pConfigTable->component.fDebuggerSessionByCore = (_wcsicmp(exdiData.fDebuggerSessionByCore, L"yes") == 0) ? true : false;
                    pConfigTable->component.fExceptionThrowEnabled = (_wcsicmp(exdiData.fExceptionThrowEnabled, L"yes") == 0) ? true : false;
                    pConfigTable->component.qSupportedPacket = exdiData.qSupportedPacket;
                    isSet = true;
                }
            }
            else if (IsExdiGdbServerTargetDataTag(pTagAttrList->tagName))
            {
                ConfigExdiTargetDataEntry targetData = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrNameServerTarget, ARRAYSIZE(attrNameServerTarget),
                    sizeof(ConfigExdiTargetDataEntry), static_cast<void*>(&targetData));
                if (SUCCEEDED(hr))
                {
                    pConfigTable->target.targetArchitecture = GetTargetGdbServerArchitecture(targetData.targetArchitecture);
                    pConfigTable->target.targetFamily = GetTargetGdbServerFamily(targetData.targetFamily);
                    pConfigTable->target.numberOfCores = _wtoi(targetData.numberOfCores);
                    pConfigTable->target.fEnabledIntelFpSseContext = (_wcsicmp(targetData.fEnabledIntelFpSseContext, L"yes") == 0) ? true : false;
                    if (swscanf_s(targetData.heuristicChunkSize, L"%I64x", &pConfigTable->target.heuristicChunkSize) != 1)
                    {
                        throw _com_error(E_INVALIDARG);
                    }
                    pConfigTable->target.targetDescriptionFileName = targetData.targetDescriptionFileName;
                    isSet = true;
                }
            }
            else if (IsGdbServerConnectionParametersTag(pTagAttrList->tagName))
            {
                ConfigGdbServerDataEntry gdbServer = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrExdiServerConnection, ARRAYSIZE(attrExdiServerConnection),
                    sizeof(ConfigGdbServerDataEntry), static_cast<void*>(&gdbServer));
                if (SUCCEEDED(hr))
                {
                    pConfigTable->gdbServer.fMultiCoreGdbServer = (_wcsicmp(gdbServer.fMultiCoreGdbServer, L"yes") == 0) ? true : false;
                    pConfigTable->gdbServer.maxServerPacketLength = _wtoi(gdbServer.maxServerPacketLength);
                    pConfigTable->gdbServer.maxConnectAttempts = _wtoi(gdbServer.maxConnectAttempts);
                    pConfigTable->gdbServer.sendTimeout = _wtoi(gdbServer.sendTimeout);
                    pConfigTable->gdbServer.receiveTimeout = _wtoi(gdbServer.receiveTimeout);
                    isSet = true;
                }
            }
            else if (IsGdbServerValueTag(pTagAttrList->tagName))
            {
                assert(pConfigTable->gdbServer.coreConnectionParameters.size() <= pConfigTable->target.numberOfCores);
                ConfigGdbServerDataEntry gdbServer = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrExdiServerConnection, ARRAYSIZE(attrExdiServerConnection),
                    sizeof(gdbServer), static_cast<void*>(&gdbServer));
                if (SUCCEEDED(hr))
                {
                    pConfigTable->gdbServer.coreConnectionParameters.push_back(gdbServer.coreConnectionParameter);
                    isSet = true;
                }
            }
            else if (IsGdbServerMemoryCommands(pTagAttrList->tagName))
            {
                ConfigExdiGdbServerMemoryCommandsEntry gdbMemoryCmds = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrExdiGdbServerMemoryCommands, ARRAYSIZE(attrExdiGdbServerMemoryCommands),
                    sizeof(ConfigExdiGdbServerMemoryCommandsEntry), static_cast<void*>(&gdbMemoryCmds));
                if (SUCCEEDED(hr))
                {
                    pConfigTable->gdbMemoryCommands.fGdbSpecialMemoryCommand = (_wcsicmp(gdbMemoryCmds.fGdbSpecialMemoryCommand, L"yes") == 0) ? true : false;
                    pConfigTable->gdbMemoryCommands.fGdbPhysicalMemoryCommand = (_wcsicmp(gdbMemoryCmds.fGdbPhysicalMemoryCommand, L"yes") == 0) ? true : false;
                    pConfigTable->gdbMemoryCommands.fGdbSupervisorMemoryCommand = (_wcsicmp(gdbMemoryCmds.fGdbSupervisorMemoryCommand, L"yes") == 0) ? true : false;
                    pConfigTable->gdbMemoryCommands.fGdbHypervisorMemoryCommand = (_wcsicmp(gdbMemoryCmds.fGdbHypervisorMemoryCommand, L"yes") == 0) ? true : false;
                    pConfigTable->gdbMemoryCommands.fGdbSpecialMemoryRegister = (_wcsicmp(gdbMemoryCmds.fGdbSpecialMemoryRegister, L"yes") == 0) ? true : false;
                    pConfigTable->gdbMemoryCommands.fGdbSystemRegistersGdbMonitor = (_wcsicmp(gdbMemoryCmds.fGdbSystemRegistersGdbMonitor, L"yes") == 0) ? true : false;
                    pConfigTable->gdbMemoryCommands.fGdbSystemRegisterDecoding = (_wcsicmp(gdbMemoryCmds.fGdbSystemRegisterDecoding, L"yes") == 0) ? true : false;
                    isSet = true;
                }
            }
            else if (IsGdbServerRegistersTag(pTagAttrList->tagName))
            {
                ConfigExdiGdServerRegistersEntry registerExdiGdbData = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrGdbServerRegisters, ARRAYSIZE(attrGdbServerRegisters),
                    sizeof(ConfigExdiGdServerRegistersEntry), static_cast<void*>(&registerExdiGdbData));
                if (SUCCEEDED(hr))
                {
                    pConfigTable->gdbServerRegisters.registerSet.push_back(move(GetTargetGdbServerArchitecture(registerExdiGdbData.RegisterArchitecture)));
                    if (pConfigTable->gdbServerRegisters.featureNameSupported == nullptr)
                    {
                        pConfigTable->gdbServerRegisters.featureNameSupported.reset(new(std::nothrow) GdbServerRegFeatureSupportedMap());
                        if (pConfigTable->gdbServerRegisters.featureNameSupported == nullptr)
                        {
                            throw _com_error(E_OUTOFMEMORY);
                        }

                    }
                    pConfigTable->gdbServerRegisters.featureNameSupported->emplace(
                        pConfigTable->gdbServerRegisters.registerSet.back(),
                        new(std::nothrow) wstring(registerExdiGdbData.FeatureNameSupported));

                    if (pConfigTable->gdbServerRegisters.spRegisterCoreSet.get() == nullptr)
                    {
                        pConfigTable->gdbServerRegisters.spRegisterCoreSet.reset(new(std::nothrow) GdbServerRegisterMap());
                        if (pConfigTable->gdbServerRegisters.spRegisterCoreSet == nullptr)
                        {
                            throw _com_error(E_OUTOFMEMORY);
                        }
                    }
                    pConfigTable->gdbServerRegisters.spRegisterCoreSet->emplace(
                        pConfigTable->gdbServerRegisters.registerSet.back(),
                        new(std::nothrow) RegisterVector());

                    //  Are system registers available via Core registers?
                    if (registerExdiGdbData.SystemRegistersStart != nullptr &&
                        registerExdiGdbData.SystemRegistersEnd != nullptr &&
                        registerExdiGdbData.SystemRegistersStart[0] != '\x0' &&
                        registerExdiGdbData.SystemRegistersEnd[0] != '\x0')
                    {
                        if (pConfigTable->gdbServerRegisters.spRegisterSystemSet.get() == nullptr)
                        {
                            pConfigTable->gdbServerRegisters.spRegisterSystemSet.reset(new(std::nothrow) GdbServerRegisterMap());
                            if (pConfigTable->gdbServerRegisters.spRegisterSystemSet == nullptr)
                            {
                                throw _com_error(E_OUTOFMEMORY);
                            }
                            pConfigTable->gdbServerRegisters.spRegisterSystemSet->emplace(
                                pConfigTable->gdbServerRegisters.registerSet.back(),
                                new(std::nothrow) RegisterVector());

                            m_spSystemRegsRange.reset(new(std::nothrow) vector<size_t>());
                            if (m_spSystemRegsRange == nullptr)
                            {
                                throw _com_error(E_OUTOFMEMORY);
                            }
                            size_t rangeStart;
                            if (swscanf_s(registerExdiGdbData.SystemRegistersStart, L"%zx", &rangeStart) != 1)
                            {
                                throw _com_error(E_INVALIDARG);
                            }
                            size_t rangeEnd;
                            if (swscanf_s(registerExdiGdbData.SystemRegistersEnd, L"%zx", &rangeEnd) != 1)
                            {
                                throw _com_error(E_INVALIDARG);
                            }
                            m_spSystemRegsRange->push_back(rangeStart);
                            m_spSystemRegsRange->push_back(rangeEnd);
                        }
                    }
                    isSet = true;
                }
            }
            else if (IsGdbRegisterEntryTag(pTagAttrList->tagName))
            {
                ConfigExdiGdServerRegistersEntry registerData = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrGdbServerRegisters, ARRAYSIZE(attrGdbServerRegisters),
                    sizeof(ConfigExdiGdServerRegistersEntry), static_cast<void*>(&registerData));
                if (SUCCEEDED(hr))
                {
                    char nameBuf[128] = {};
                    if (WideCharToMultiByte(CP_ACP, 0, registerData.RegisterName, static_cast<int>(wcslen(registerData.RegisterName)),
                        nameBuf, sizeof(nameBuf), nullptr, nullptr) == 0)
                    {
                        throw _com_error(E_INVALIDARG);
                    }

                    char nameOrderBuf[128] = {};
                    if (WideCharToMultiByte(CP_ACP, 0, registerData.RegisterOrder, static_cast<int>(wcslen(registerData.RegisterOrder)),
                        nameOrderBuf, sizeof(nameOrderBuf), nullptr, nullptr) == 0)
                    {
                        throw _com_error(E_INVALIDARG);
                    }

                    RegistersStruct registerSet = {};
                    registerSet.name = nameBuf;
                    registerSet.nameOrder = nameOrderBuf;
                    registerSet.registerSize = static_cast<size_t>(_wtoi(registerData.RegisterSize));
                    registerSet.group = "core";
                    auto it = pConfigTable->gdbServerRegisters.spRegisterCoreSet->find(pConfigTable->gdbServerRegisters.registerSet.back());
                    if (it == pConfigTable->gdbServerRegisters.spRegisterCoreSet->end())
                    {
                        throw _com_error(E_INVALIDARG);
                    }
                    it->second->push_back(move(registerSet));

                    //  check if there is a core register that needs to be reported as
                    //  system register
                    if (pConfigTable->gdbServerRegisters.spRegisterSystemSet != nullptr &&
                        m_spSystemRegsRange != nullptr)
                    {
                        auto itSystem = pConfigTable->gdbServerRegisters.spRegisterSystemSet->find(pConfigTable->gdbServerRegisters.registerSet.back());
                        if (itSystem != pConfigTable->gdbServerRegisters.spRegisterSystemSet->end())
                        {
                            size_t coreRegister;
                            if (swscanf_s(registerData.RegisterOrder, L"%zx", &coreRegister) != 1)
                            {
                                throw _com_error(E_INVALIDARG);
                            }

                            size_t rangeLow = (*m_spSystemRegsRange.get())[0];
                            size_t rangeHigh = (*m_spSystemRegsRange.get())[1];
                            if (rangeLow <= coreRegister && coreRegister <= rangeHigh)
                            {
                                RegistersStruct systemRegisterSet = {};
                                systemRegisterSet.name = nameBuf;
                                systemRegisterSet.nameOrder = nameOrderBuf;
                                systemRegisterSet.registerSize = static_cast<size_t>(_wtoi(registerData.RegisterSize));
                                systemRegisterSet.group = "system";
                                itSystem->second->push_back(move(systemRegisterSet));
                            }
                        }
                    }
                    isSet = true;
                }
            }
        }
        else
        {
            isSet = true;
            hr = S_OK;
        }

        if (!isSet)
        {
            ReportXmlError(L"Failed setting a value in ConfigExdiGdbServerHelperImpl::HandleTagAttributeList()\n");
            hr = E_FAIL;
        }
    }
    catch (exception& ex)
    {
        ReportExceptionError(ex.what());
    }
    catch (...)
    {
        ReportXmlError(L"Unrecognized exception happened in ConfigExdiGdbServerHelperImpl::ProcessAttributeList()\n");
    }
    return hr;
}

//
//  Gdb register file related functions
//
inline bool XmlDataGdbServerRegisterFile::IsTargetDescriptionFile(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, targetFileArchitectureName) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataGdbServerRegisterFile::IsRegisterFileReference(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, includeTargetFile) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataGdbServerRegisterFile::IsFeatureRegisterFile(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, featureTag) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataGdbServerRegisterFile::IsRegisterFileEntry(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, regTag) == 0)
    {
        isDone = true;
    }
    return isDone;
}


bool XmlDataGdbServerRegisterFile::SetFileTargetArchitecture(_In_ PCWSTR pTagValue,
    _Out_ ConfigExdiGdbSrvData* pConfigTable)
{
    bool isSet = true;
    if (_wcsicmp(pTagValue, L"aarch64") == 0)
    {
        pConfigTable->file.registerGroupArchitecture = ARM64_ARCH;
    }
    else if (_wcsicmp(pTagValue, L"x86-x64") == 0 ||
             _wcsicmp(pTagValue, L"i386:x86-64") == 0 ||
             _wcsicmp(pTagValue, L"X64") == 0)
    {
        pConfigTable->file.registerGroupArchitecture = AMD64_ARCH;
    }
    else if (_wcsicmp(pTagValue, L"ARM") == 0)
    {
        pConfigTable->file.registerGroupArchitecture = ARM32_ARCH;
    }
    else if (_wcsicmp(pTagValue, L"x86") == 0)
    {
        pConfigTable->file.registerGroupArchitecture = X86_ARCH;
    }
    else
    {
        isSet = false;
    }
    return isSet;
}

bool XmlDataGdbServerRegisterFile::SetRegistersByTargetFile(_In_ TAG_ATTR_LIST* const pTagAttrList,
    _Out_ ConfigExdiGdbSrvData* pConfigTable)
{
    HRESULT hr;
    bool isSet = false;

    ConfigTargetRegisterDescriptionFileEntry registerFileData = { 0 };
    if (IsFeatureRegisterFile(pTagAttrList->tagName))
    {
        hr = XmlDataHelpers::GetXmlTagAttributeValues(pTagAttrList, attrRegistersFile, ARRAYSIZE(attrRegistersFile),
            sizeof(ConfigTargetRegisterDescriptionFileEntry), static_cast<void*>(&registerFileData));
        if (SUCCEEDED(hr))
        {
            //  Check if the feature is supported
            auto it = pConfigTable->gdbServerRegisters.featureNameSupported->find(pConfigTable->file.registerGroupArchitecture);
            if (it == pConfigTable->gdbServerRegisters.featureNameSupported->end())
            {
                throw _com_error(E_INVALIDARG);
            }

            if ((it->second->compare(L"all") == 0) ||
                (wcsstr(registerFileData.featureName, it->second->c_str()) != nullptr))
            {
                pConfigTable->gdbServerRegisters.featureName = registerFileData.featureName;
            }

            pConfigTable->gdbServerRegisters.spRegisterSystemSet.reset(new(std::nothrow) GdbServerRegisterMap());
            if (pConfigTable->gdbServerRegisters.spRegisterSystemSet == nullptr)
            {
                throw _com_error(E_OUTOFMEMORY);
            }
            pConfigTable->gdbServerRegisters.spRegisterSystemSet->emplace(
                pConfigTable->file.registerGroupArchitecture,
                new(std::nothrow) RegisterVector());

            isSet = true;
        }
    }
    else if (!pConfigTable->gdbServerRegisters.featureName.empty() &&
        IsRegisterFileEntry(pTagAttrList->tagName))
    {
        hr = XmlDataHelpers::GetXmlTagAttributeValues(pTagAttrList, attrRegistersFile, ARRAYSIZE(attrRegistersFile),
            sizeof(ConfigTargetRegisterDescriptionFileEntry), static_cast<void*>(&registerFileData));
        if (SUCCEEDED(hr))
        {
            char nameBuf[256] = {};
            if (WideCharToMultiByte(CP_ACP, 0, registerFileData.RegisterName, static_cast<int>(wcslen(registerFileData.RegisterName)),
                nameBuf, sizeof(nameBuf), nullptr, nullptr) == 0)
            {
                throw _com_error(E_INVALIDARG);
            }

            size_t registerSize = static_cast<size_t>(_wtoi(registerFileData.RegistBitSize));
            size_t leftBits = registerSize % 8;
            registerSize /= 8;
            if (leftBits != 0)
            {
                registerSize++;
            }

            size_t regOrder = static_cast<size_t>(_wtoi(registerFileData.RegisterNum));
            char nameOrderBuf[128] = {};
            sprintf_s(nameOrderBuf, _countof(nameOrderBuf), "%zx", regOrder);

            char groupBuf[128] = {};
            if (WideCharToMultiByte(CP_ACP, 0, registerFileData.RegisterGroup, static_cast<int>(wcslen(registerFileData.RegisterGroup)),
                groupBuf, sizeof(groupBuf), nullptr, nullptr) == 0)
            {
                throw _com_error(E_INVALIDARG);
            }

            RegistersStruct registerSet = {};
            registerSet.name = nameBuf;
            registerSet.nameOrder = nameOrderBuf;
            registerSet.registerSize = registerSize;
            registerSet.group = groupBuf;
            auto it = pConfigTable->gdbServerRegisters.spRegisterSystemSet->find(pConfigTable->file.registerGroupArchitecture);
            if (it == pConfigTable->gdbServerRegisters.spRegisterSystemSet->end())
            {
                throw _com_error(E_INVALIDARG);
            }
            it->second->push_back(move(registerSet));

            isSet = true;
        }
    }

    return isSet;
}

bool XmlDataGdbServerRegisterFile::HandleTargetFileTags(_In_ TAG_ATTR_LIST* const pTagAttrList,
    _Out_ ConfigExdiGdbSrvData* pConfigTable)
{
    HRESULT hr;
    bool isDone = false;

    ConfigTargetDescriptionFileEntry targetFileData = { 0 };
    if (IsTargetDescriptionFile(pTagAttrList->tagName))
    {
        hr = XmlDataHelpers::GetXmlTagAttributeValues(pTagAttrList, attrTargetDescriptionArchitectureName, ARRAYSIZE(attrTargetDescriptionArchitectureName),
            sizeof(ConfigTargetDescriptionFileEntry), static_cast<void*>(&targetFileData));
        if (SUCCEEDED(hr))
        {
            isDone = SetFileTargetArchitecture(targetFileData.targetArchitecture, pConfigTable);
            if (!isDone)
            {
                if (targetFileData.targetArchitecture[0] == '\x0')
                {
                    pConfigTable->file.isTargetTagEmpty = true;
                }
                isDone = true;
            }
        }
    }

    if (IsRegisterFileReference(pTagAttrList->tagName))
    {
        hr = XmlDataHelpers::GetXmlTagAttributeValues(pTagAttrList, attrTargetDescriptionRegisterFile, ARRAYSIZE(attrTargetDescriptionRegisterFile),
            sizeof(ConfigTargetDescriptionFileEntry), static_cast<void*>(&targetFileData));
        if (SUCCEEDED(hr))
        {
            if (pConfigTable->file.registerGroupFiles.get() == nullptr)
            {
                pConfigTable->file.registerGroupFiles.reset(new(std::nothrow) targetDescriptionFilesMap());
                if (pConfigTable->file.registerGroupFiles == nullptr)
                {
                    throw _com_error(E_OUTOFMEMORY);
                }
            }

            isDone = true;
            if (wcsstr(targetFileData.registerFile, L"core") != nullptr || wcsstr(targetFileData.registerFile, L"general") != nullptr ||
                wcsstr(targetFileData.registerFile, L"i386-64") != nullptr)
            {
                pConfigTable->file.registerGroupFiles->insert({ RegisterGroupType::CORE_REGS, move(targetFileData.registerFile) });
            }
            else if (wcsstr(targetFileData.registerFile, L"system") != nullptr || wcsstr(targetFileData.registerFile, L"banked") != nullptr)
            {
                pConfigTable->file.registerGroupFiles->insert({ RegisterGroupType::SYSTEM_REGS, move(targetFileData.registerFile) });
            }
            else if (wcsstr(targetFileData.registerFile, L"fpu") != nullptr || wcsstr(targetFileData.registerFile, L"simdfp") != nullptr)
            {
                pConfigTable->file.registerGroupFiles->insert({ RegisterGroupType::FPU_REGS, move(targetFileData.registerFile) });
            }
            else
            {
                assert(false);
                isDone = false;
            }
        }
    }
    return isDone;
}

//
//  System register map tag
//
inline bool XmlDataSystemRegister::IsSystemRegisterMapElement(_In_ PCWSTR pTagName)
{
    return IsSystemRegisterMapTag(pTagName) || 
        IsSystemRegistersTag(pTagName) || 
        IsSystemRegisterEntryTag(pTagName);
}

inline bool XmlDataSystemRegister::IsSystemRegisterMapTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, tagSystemRegisterMap) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataSystemRegister::IsSystemRegistersTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, tagSystemRegisters) == 0)
    {
        isDone = true;
    }
    return isDone;
}

inline bool XmlDataSystemRegister::IsSystemRegisterEntryTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;

    if (_wcsicmp(pTagName, tagRegisterEntry) == 0)
    {
        isDone = true;
    }
    return isDone;
}

bool XmlDataSystemRegister::HandleMapSystemRegAccessCode(_In_ TAG_ATTR_LIST* const pTagAttrList,
    _Inout_ ConfigExdiGdbSrvData * pConfigTable)
{
    HRESULT hr;
    bool isDone = false;

    ConfigSystemRegMapAccessCodeEntry systemRegMapData = { 0 };
    if (IsSystemRegistersTag(pTagAttrList->tagName))
    {
        hr = XmlDataHelpers::GetXmlTagAttributeValues(pTagAttrList, attrMapSystemRegisterAccessCode, ARRAYSIZE(attrMapSystemRegisterAccessCode),
            sizeof(ConfigSystemRegMapAccessCodeEntry), static_cast<void*>(&systemRegMapData));
        if (SUCCEEDED(hr))
        {
            TargetArchitecture arch = XmlDataHelpers::GetTargetGdbServerArchitecture(systemRegMapData.RegisterArchitecture);
            //  Process only map for architecture with the system registers have been created
            if (pConfigTable->gdbServerRegisters.spRegisterSystemSet == nullptr ||
                pConfigTable->gdbServerRegisters.spRegisterSystemSet->empty())
            {
                return isDone;
            }

            auto itVector = pConfigTable->gdbServerRegisters.spRegisterSystemSet->find(arch);
            if (itVector == pConfigTable->gdbServerRegisters.spRegisterSystemSet->end())
            {
                //  This is not the architecture of the current built system register map.
                return true;
            }

            pConfigTable->systemRegisterMap.systemRegArchitecture.push_back(arch);

            if (pConfigTable->systemRegisterMap.spSysRegisterMap.get() == nullptr)
            {
                pConfigTable->systemRegisterMap.spSysRegisterMap.reset(new(std::nothrow) SystemRegCodeMap());
                if (pConfigTable->systemRegisterMap.spSysRegisterMap == nullptr)
                {
                    throw _com_error(E_OUTOFMEMORY);
                }
                pConfigTable->systemRegisterMap.spSysRegisterMap->emplace(
                    pConfigTable->systemRegisterMap.systemRegArchitecture.back(),
                    new(std::nothrow) SystemRegistersMapType());
            }
            isDone = true;
        }
    }
    else if (IsSystemRegisterEntryTag(pTagAttrList->tagName))
    {
        hr = XmlDataHelpers::GetXmlTagAttributeValues(pTagAttrList, attrMapSystemRegisterAccessCode, ARRAYSIZE(attrMapSystemRegisterAccessCode),
            sizeof(ConfigSystemRegMapAccessCodeEntry), static_cast<void*>(&systemRegMapData));
        if (SUCCEEDED(hr))
        {
            if (pConfigTable->systemRegisterMap.spSysRegisterMap.get() == nullptr ||
                pConfigTable->systemRegisterMap.systemRegArchitecture.empty())
            {
                //  Ignore since the map has not been created for the current architecture
                return true;
            }

            char nameBuf[128] = {};
            if (WideCharToMultiByte(CP_ACP, 0, systemRegMapData.RegisterName, 
                static_cast<int>(wcslen(systemRegMapData.RegisterName)),
                nameBuf, sizeof(nameBuf), nullptr, nullptr) == 0)
            {
                throw _com_error(E_INVALIDARG);
            }

            std::vector<int> accessCodeVector;
            TargetArchitectureHelpers::TokenizeAccessCodeByArch(
                pConfigTable->systemRegisterMap.systemRegArchitecture.back(),
                systemRegMapData.AccessCode,
                L",", &accessCodeVector);
            if (accessCodeVector.size() != c_numberOfAccessCodeFields)
            {
                throw _com_error(E_INVALIDARG);
            }

            //  Encode accesscode 
            AddressType encodedValue = TargetArchitectureHelpers::EncodeAccessCode(
                pConfigTable->systemRegisterMap.systemRegArchitecture.back(),
                accessCodeVector[0],
                accessCodeVector[1],
                accessCodeVector[2],
                accessCodeVector[3],
                accessCodeVector[4]);
            if (encodedValue == c_InvalidAddress)
            {
                throw _com_error(E_INVALIDARG);
            }

            //  Generate the map entry pair taking into account the Register order as well
            SystemPairRegOrderNameType regNameOrder = {};
            regNameOrder.second = string(nameBuf);
            auto itVector = pConfigTable->gdbServerRegisters.spRegisterSystemSet->find(pConfigTable->systemRegisterMap.systemRegArchitecture.back());
            if (itVector == pConfigTable->gdbServerRegisters.spRegisterSystemSet->end())
            {
                throw _com_error(E_INVALIDARG);
            }

            auto itMap = pConfigTable->systemRegisterMap.spSysRegisterMap->find(pConfigTable->systemRegisterMap.systemRegArchitecture.back());
            if (itMap == pConfigTable->systemRegisterMap.spSysRegisterMap->end())
            {
                throw _com_error(E_INVALIDARG);
            }

            for (auto it = itVector->second->cbegin();
                 it != itVector->second->cend(); ++it)
            {
                if (it->name == regNameOrder.second &&
                    !IsRegisterPresent(it->nameOrder, itMap->second))
                {
                    regNameOrder.first = string(it->nameOrder);
                    break;
                }
            }

            if (regNameOrder.first.empty())
            {
                regNameOrder.first = string("n/a");
            }
            itMap->second->insert({(encodedValue & 0xffffffff), move(regNameOrder)});
            isDone = true;
        }
    }
    return isDone;
}

inline bool XmlDataSystemRegister::IsRegisterPresent(_In_ const string& regOrder,
    _In_ std::unique_ptr <SystemRegistersMapType>& spSysRegisterMap)
{
    for (auto it = spSysRegisterMap->cbegin();
        it != spSysRegisterMap->cend();
        ++it)
    {
        if (it->second.first == regOrder)
        {
            return true;
        }
    }
    return false;
}

#pragma endregion
