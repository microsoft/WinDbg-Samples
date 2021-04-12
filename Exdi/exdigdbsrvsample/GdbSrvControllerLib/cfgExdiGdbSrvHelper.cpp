//----------------------------------------------------------------------------
//
// cfgExdiGdbSrvHelper.cpp
//
// Helper for reading the Exdi-GdbServer configuration file.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#include "ExdiGdbSrvSample.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include <tchar.h>
#include <Strsafe.h>
#include <exception>
#include <comdef.h> 
#include <xmllite.h>
#include "HandleHelpers.h"
#include "GdbSrvControllerLib.h"
#include "cfgExdiGdbSrvHelper.h"

using namespace GdbSrvControllerLib;
using namespace std;

//=============================================================================
// Private defines and typedefs
//=============================================================================
// Tag-attribute maximum length.
const size_t C_MAX_ATTR_LENGTH = (256 + 1);

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
} ConfigExdiData;

//  This type indicates the Target data.
typedef struct
{
    TargetArchitecture targetArchitecture; //  The target architecture.
    DWORD targetFamily;             //  The target architecture.
    unsigned numberOfCores;         //  Number of cores of the target processor CPU.
    bool fEnabledIntelFpSseContext; //  Flag if set then the Intel floating SSE context is processed.
} ConfigExdiTargetData;

//  This type indicates the GdbServer specific data.
typedef struct
{
    bool fMultiCoreGdbServer;       //  Flag if set then we support multi-core connections with the DbgServer, so
                                    //  there will be one GdbServer launched for each core CPU.
    size_t maxServerPacketLength;   //  Maximum GdbServer packet length.
    unsigned int maxConnectAttempts; //  Connect session maximum attempts
    unsigned int sendTimeout;      //  Send RSP packet timeout
    unsigned int receiveTimeout;   //  Receive timeout
    vector<wstring> coreConnectionParameters;  //  Connection string (hostname-ip:port) for each GdbServer core instance.
} ConfigGdbServerData;

//  This type indicates the data structure that will be created after processing
//  the Exdi-GdbServer input xml config file.
typedef struct
{
    ConfigExdiData component;       //  Component data
    ConfigExdiTargetData target;    //  Target data
    ConfigGdbServerData gdbServer;  //  Server data
} ConfigExdiGdbSrvData; 

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

typedef struct
{
    WCHAR agentNamePacket[C_MAX_ATTR_LENGTH];           //  Agent name
    WCHAR uuid[C_MAX_ATTR_LENGTH];                      //  Class identifier.
    WCHAR fDisplayCommPackets[C_MAX_ATTR_LENGTH];       //  Flag if set then we display the communication packets characters.
	WCHAR fDebuggerSessionByCore[C_MAX_ATTR_LENGTH];    //	Flag if set then we do debug only by core processor, so step and continue
									                    //	commands happen only on one core at the time. If not set then we 
									                    //	let all cores run when we do step/continue commands.
    WCHAR fExceptionThrowEnabled[C_MAX_ATTR_LENGTH];    //  Allow throwing exception by the Exdi COM server.
                                                        //  Used to disallow throwing exceptions when memory failures occur.
} ConfigExdiDataEntry;

typedef struct
{
    WCHAR targetArchitecture[C_MAX_ATTR_LENGTH];        //  The target architecture.
    WCHAR targetFamily[C_MAX_ATTR_LENGTH];              //  The target family
    WCHAR numberOfCores[C_MAX_ATTR_LENGTH];             //  Number of cores of the target processor CPU.
    WCHAR fEnabledIntelFpSseContext[C_MAX_ATTR_LENGTH]; //  Flag if set then the Intel floating SSE context is processed.
} ConfigExdiTargetDataEntry;

//  List-node element structure.
typedef struct
{
    LIST_ENTRY  token;
    //  Attribute pair localName="value"
	wstring localName;         
	wstring value;
} AttrList_NodeElem_Struct, *PAttrList_NodeElem_Struct;

//  List Tag-Attributes
typedef struct
{        
    PCWSTR tagName;
    //  Each list node will be allocated as a AttrList_NodeElem_Struct structure.
    LIST_ENTRY attrPair;
} TAG_ATTR_LIST;   

//  This type indicates the xmllite error code notification structure.
typedef struct
{
    HRESULT hr;
    PCWSTR pMessage;
} XMLLITE_ERROR_STRUCT;

//  Attribute name handler types.
typedef  bool (* pXmlAttrValueHandler)(_In_ PCWSTR pAttrValue, 
                                       _In_ size_t fieldOffset,
                                       _In_ size_t totalSizeOfStruct,
                                       _In_ size_t numberOfElements,
                                       _Out_ void * pInputData);

//  This structure indicates the mapping between the local name attribute value and its corresponding handler.
typedef struct
{
    PCWSTR pTagName;                //  Tag name
    PCWSTR pLocalName;              //  Attribute name
    pXmlAttrValueHandler pHandler;  //  Handler for processing the attribute value
    size_t outStructFieldOffset;    //  This is the field offset used for updating the out structure
    size_t structFieldNumberOfElements; // This is the filed number of elements
} XML_ATTRNAME_HANDLER_STRUCT;

//  Function helper for reading the attributes value from the xml file.
bool XmlGetStringValue(_In_z_ PCWSTR pAttrValue, _In_ size_t fieldOffset,
                       _In_ size_t totalSizeOfStruct, _In_ size_t numberOfElements,
                       _Out_writes_(totalSizeOfStruct) void * pOutData);

//=============================================================================
// Private data definitions
//=============================================================================    
const WCHAR exdiGdbServerConfigData[] = L"ExdiGdbServerConfigData";
const WCHAR exdiGdbServerTargetData[] = L"ExdiGdbServerTargetData";
const WCHAR gdbServerConnectionParameters[] = L"GdbServerConnectionParameters";
const WCHAR gdbServerConnectionValue[] = L"Value";
const WCHAR gdbServerAgentNamePacket[] = L"agentNamePacket";
const WCHAR gdbServerUuid[] = L"uuid";
const WCHAR displayCommPackets[] = L"displayCommPackets";
const WCHAR debuggerSessionByCore[] = L"debuggerSessionByCore";
const WCHAR enableThrowExceptions[] = L"enableThrowExceptionOnMemoryErrors";
const WCHAR targetArchitectureName[] = L"targetArchitecture";
const WCHAR targetFamilyName[] = L"targetFamily";
const WCHAR numberOfCoresName[] = L"numberOfCores";
const WCHAR enableSseContextName[] = L"enableSseContext";
const WCHAR multiCoreGdbServer[] = L"MultiCoreGdbServerSessions";
const WCHAR maximumGdbServerPacketLength[] = L"MaximumGdbServerPacketLength";
const WCHAR hostNameAndPort[] = L"HostNameAndPort";
const WCHAR maximumConnectAttempts[] = L"MaximumConnectAttempts";
const WCHAR sendPacketTimeout[] = L"SendPacketTimeout";
const WCHAR receivePacketTimeout[] = L"ReceivePacketTimeout";

//  General debugger information - handler map
const XML_ATTRNAME_HANDLER_STRUCT attrExdiServerHandlerMap[] = 
{
    {exdiGdbServerConfigData, gdbServerAgentNamePacket, XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, agentNamePacket), C_MAX_ATTR_LENGTH},
    {exdiGdbServerConfigData, gdbServerUuid,            XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, uuid), C_MAX_ATTR_LENGTH},
    {exdiGdbServerConfigData, displayCommPackets,       XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, fDisplayCommPackets), C_MAX_ATTR_LENGTH},
    {exdiGdbServerConfigData, debuggerSessionByCore,    XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, fDebuggerSessionByCore), C_MAX_ATTR_LENGTH},
    {exdiGdbServerConfigData, enableThrowExceptions,    XmlGetStringValue, FIELD_OFFSET(ConfigExdiDataEntry, fExceptionThrowEnabled), C_MAX_ATTR_LENGTH},
};

//  Attribute name - handler map for the GdbServer server tag info
const XML_ATTRNAME_HANDLER_STRUCT attrNameServerTarget[] = 
{
    {exdiGdbServerTargetData, targetArchitectureName, XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, targetArchitecture), C_MAX_ATTR_LENGTH},
    {exdiGdbServerTargetData, targetFamilyName,       XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, targetFamily), C_MAX_ATTR_LENGTH},
    {exdiGdbServerTargetData, numberOfCoresName,      XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, numberOfCores), C_MAX_ATTR_LENGTH},
    {exdiGdbServerTargetData, enableSseContextName,   XmlGetStringValue, FIELD_OFFSET(ConfigExdiTargetDataEntry, fEnabledIntelFpSseContext), C_MAX_ATTR_LENGTH},
};

//  General debugger information - handler map
const XML_ATTRNAME_HANDLER_STRUCT attrExdiServerConnection[] = 
{
    {gdbServerConnectionParameters, multiCoreGdbServer,           XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, fMultiCoreGdbServer), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionParameters, maximumGdbServerPacketLength, XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, maxServerPacketLength), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionParameters, maximumConnectAttempts,       XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, maxConnectAttempts), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionParameters, sendPacketTimeout,            XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, sendTimeout), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionParameters, receivePacketTimeout,         XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, receiveTimeout), C_MAX_ATTR_LENGTH},
    {gdbServerConnectionValue, hostNameAndPort,                   XmlGetStringValue, FIELD_OFFSET(ConfigGdbServerDataEntry, coreConnectionParameter), C_MAX_ATTR_LENGTH},
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

//=============================================================================
// Private function declarations
//=============================================================================
//  Find the error message
PCWSTR GetXmlErrorMsg(HRESULT hr)
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

void ReportXmlError(_In_ PCWSTR pMessage) 
{
    assert(pMessage != nullptr);

    MessageBox(0, reinterpret_cast<LPCTSTR>(pMessage), nullptr, MB_ICONERROR);
}

void ReportXmlExceptionCode(_In_ PCWSTR pMessage, _In_ DWORD exceptCode)
{
    WCHAR message[C_MAX_ATTR_LENGTH] = L"";
    swprintf_s(message, ARRAYSIZE(message), L"%s (exception Code: 0x%x)\n", pMessage, exceptCode);
    ReportXmlError(message);
}

void ReportExceptionError(_In_ PCSTR pMessage)
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

bool XmlGetStringValue(_In_z_ PCWSTR pAttrValue, _In_ size_t fieldOffset,
                       _In_ size_t totalSizeOfStruct, _In_ size_t numberOfElements,
                       _Out_writes_(totalSizeOfStruct) void * pOutData)
{
    bool isGet = false;

    try
    {
        assert(pAttrValue != nullptr && pOutData != nullptr);
        BYTE * pStart = (static_cast<BYTE *>(pOutData)) + fieldOffset;
        BYTE * pEnd = (static_cast<BYTE *>(pOutData)) + totalSizeOfStruct;
        PWSTR pString = reinterpret_cast<PWSTR>(pStart);
        size_t leftElements = (reinterpret_cast<PWSTR>(pEnd) - pString);
        if (fieldOffset > totalSizeOfStruct || numberOfElements > leftElements)
        {
            throw exception("The structure field offset does not belong to the current passed in structure");    
        }
        wcscpy_s(pString, numberOfElements, pAttrValue);
        isGet = true;
    }
	catch (exception & ex)
	{
		ReportExceptionError(ex.what());        
	}
    return isGet;
}

inline bool IsExdiGdbServerConfigDataTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;
    
    if (_wcsicmp(pTagName, exdiGdbServerConfigData) == 0)
    {
        isDone = true;
    }
    return isDone;        
}

inline bool IsExdiGdbServerTargetDataTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;
    
    if (_wcsicmp(pTagName, exdiGdbServerTargetData) == 0)
    {
        isDone = true;
    }
    return isDone;        
}

inline bool IsGdbServerConnectionParametersTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;
    
    if (_wcsicmp(pTagName, gdbServerConnectionParameters) == 0)
    {
        isDone = true;
    }
    return isDone;        
}

inline bool IsGdbServerValueTag(_In_ PCWSTR pTagName)
{
    assert(pTagName != nullptr);
    bool isDone = false;
    
    if (_wcsicmp(pTagName, gdbServerConnectionValue) == 0)
    {
        isDone = true;
    }
    return isDone;        
}

TargetArchitecture GetTargetGdbServerArchitecture(_In_ PCWSTR pDataString)
{
    assert(pDataString != nullptr);

    TargetArchitecture targetData = UNKNOWN_ARCH;
    if (_wcsicmp(pDataString, L"X86") == 0)
    {
        targetData = X86_ARCH;            
    }
    else if (_wcsicmp(pDataString, L"X64") == 0)
    {
        assert(false);
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
                   _T("EXDI-GdbServer Sample"),
                   MB_ICONERROR);
    }
    return targetData;
}

DWORD GetTargetGdbServerFamily(_In_ PCWSTR pDataString)
{
    assert(pDataString != nullptr);

    DWORD targetData = PROCESSOR_FAMILY_UNK;
    if (_wcsicmp(pDataString, L"ProcessorFamilyX86") == 0)
    {
        targetData = PROCESSOR_FAMILY_X86;            
    }
    else if (_wcsicmp(pDataString, L"ProcessorFamilyX64") == 0)
    {
        assert(false);
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
                   _T("EXDI-GdbServer Sample"),
                   MB_ICONERROR);
    }
    return targetData;
}


class ConfigExdiGdbServerHelper::ConfigExdiGdbServerHelperImpl
{
    public:
    ConfigExdiGdbServerHelperImpl::ConfigExdiGdbServerHelperImpl(): m_XmlLiteReader(nullptr), m_IStream(nullptr)
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
        assert(pXmlConfigFile != nullptr);
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
                ReportXmlError(GetXmlErrorMsg(hr));
            }
        }
	    __except (EXCEPTION_EXECUTE_HANDLER)
	    {
		    ReportXmlExceptionCode(L"A Windows Exception was catched in the function ConfigExdiGdbServerHelper::ReadConfigFile().\n",
                   			       GetExceptionCode());
            throw;
	    }
        return isReadDone;
    }

    inline void ConfigExdiGdbServerHelperImpl::GetExdiComponentAgentNamePacket(_Out_ wstring & packetName)
    {
        packetName = m_ExdiGdbServerData.component.agentNamePacket;
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

    inline bool ConfigExdiGdbServerHelperImpl::GetMultiCoreGdbServer()
    {
        return m_ExdiGdbServerData.gdbServer.fMultiCoreGdbServer;
    }

    inline size_t ConfigExdiGdbServerHelperImpl::GetMaxServerPacketLength()
    {
        return m_ExdiGdbServerData.gdbServer.maxServerPacketLength;
    }

    inline unsigned int ConfigExdiGdbServerHelperImpl::GetMaxConnectAttempts()
    {
        return m_ExdiGdbServerData.gdbServer.maxConnectAttempts;
    }

    inline unsigned int ConfigExdiGdbServerHelperImpl::GetSendPacketTimeout()
    {
        return m_ExdiGdbServerData.gdbServer.sendTimeout;
    }

    inline unsigned int ConfigExdiGdbServerHelperImpl::GetReceiveTimeout()
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
    
    private:
    CComPtr<IXmlReader> m_XmlLiteReader;
    CComPtr<IStream> m_IStream;
    ConfigExdiGdbSrvData m_ExdiGdbServerData;

    //  Xml helper related functionality
    inline bool IsMemoryXmlBuffer(_In_opt_ PCWSTR pXmlConfigFile) {return (pXmlConfigFile == nullptr);}

    //  ConfigExdiGdbServerHelper function
    HRESULT ConfigExdiGdbServerHelperImpl::InitHelper()
    {
        // Create IXmlReader object.
        HRESULT hr = CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(&m_XmlLiteReader), nullptr);
        if (SUCCEEDED(hr))
        {
            // Set IXmlReader properties.
            // Prohibit the Document Type Definitions (DTDs) in the document.
            hr = m_XmlLiteReader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
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
        return nullptr;
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

    //  Validates the XML tag-Attribute value and get the value from the xml file
    HRESULT ConfigExdiGdbServerHelperImpl::GetXmlTagAttributeValues(_In_ TAG_ATTR_LIST * const pTagAttrList, 
                                                                    _In_ const XML_ATTRNAME_HANDLER_STRUCT * pMap,
                                                                    _In_ size_t mapSize, size_t maxSizeOfOutStructData,
                                                                    _Out_writes_bytes_(maxSizeOfOutStructData) void * pOutData)
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
	    catch (exception & ex)
	    {
		    ReportExceptionError(ex.what());
            hr = E_FAIL;
	    }
        catch(...)
        {
            ReportXmlError(L"Unrecognized exception happened in ConfigExdiGdbServerHelperImpl::GetXmlTagAttributeValues()\n");
            hr = E_FAIL;
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

    HRESULT ConfigExdiGdbServerHelperImpl::HandleTagAttributeList(_In_ TAG_ATTR_LIST * const pTagAttrList)
    {
        assert(pTagAttrList != nullptr);
        HRESULT hr = E_FAIL;

        try
        {
            bool isSet = false;

            if (IsExdiGdbServerConfigDataTag(pTagAttrList->tagName))
            {
                ConfigExdiDataEntry exdiData = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrExdiServerHandlerMap, ARRAYSIZE(attrExdiServerHandlerMap), 
                                              sizeof(ConfigExdiDataEntry), static_cast<void *>(&exdiData));
                if (SUCCEEDED(hr))
                {
                    m_ExdiGdbServerData.component.agentNamePacket = exdiData.agentNamePacket;
                    m_ExdiGdbServerData.component.uuid = exdiData.uuid;    
                    m_ExdiGdbServerData.component.fDisplayCommPackets = (_wcsicmp(exdiData.fDisplayCommPackets, L"yes") == 0) ? true : false;
                    m_ExdiGdbServerData.component.fDebuggerSessionByCore = (_wcsicmp(exdiData.fDebuggerSessionByCore, L"yes") == 0) ? true : false;
                    m_ExdiGdbServerData.component.fExceptionThrowEnabled = (_wcsicmp(exdiData.fExceptionThrowEnabled, L"yes") == 0) ? true : false;
                    isSet = true;
                }
            }
            else if (IsExdiGdbServerTargetDataTag(pTagAttrList->tagName))
            {
                ConfigExdiTargetDataEntry targetData = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrNameServerTarget, ARRAYSIZE(attrNameServerTarget),
                                              sizeof(ConfigExdiTargetDataEntry), static_cast<void *>(&targetData));
                if (SUCCEEDED(hr))
                {
                    m_ExdiGdbServerData.target.targetArchitecture = GetTargetGdbServerArchitecture(targetData.targetArchitecture);
                    m_ExdiGdbServerData.target.targetFamily = GetTargetGdbServerFamily(targetData.targetFamily);
                    m_ExdiGdbServerData.target.numberOfCores = _wtoi(targetData.numberOfCores);
                    m_ExdiGdbServerData.target.fEnabledIntelFpSseContext = (_wcsicmp(targetData.fEnabledIntelFpSseContext, L"yes") == 0) ? true : false;
                    isSet = true;
                }
            }
            else if (IsGdbServerConnectionParametersTag(pTagAttrList->tagName))
            {
                ConfigGdbServerDataEntry gdbServer = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrExdiServerConnection, ARRAYSIZE(attrExdiServerConnection),
                                              sizeof(ConfigGdbServerDataEntry), static_cast<void *>(&gdbServer));
                if (SUCCEEDED(hr))
                {
                    m_ExdiGdbServerData.gdbServer.fMultiCoreGdbServer = (_wcsicmp(gdbServer.fMultiCoreGdbServer, L"yes") == 0) ? true : false;
                    m_ExdiGdbServerData.gdbServer.maxServerPacketLength = _wtoi(gdbServer.maxServerPacketLength);
                    m_ExdiGdbServerData.gdbServer.maxConnectAttempts = _wtoi(gdbServer.maxConnectAttempts);
                    m_ExdiGdbServerData.gdbServer.sendTimeout = _wtoi(gdbServer.sendTimeout);
                    m_ExdiGdbServerData.gdbServer.receiveTimeout = _wtoi(gdbServer.receiveTimeout);
                    isSet = true;
                }            
            }    
            else if (IsGdbServerValueTag(pTagAttrList->tagName))
            {
                assert(m_ExdiGdbServerData.gdbServer.coreConnectionParameters.size() <= m_ExdiGdbServerData.target.numberOfCores);
                ConfigGdbServerDataEntry gdbServer = {};
                hr = GetXmlTagAttributeValues(pTagAttrList, attrExdiServerConnection, ARRAYSIZE(attrExdiServerConnection),
                                              sizeof(ConfigGdbServerDataEntry), static_cast<void *>(&gdbServer));
                if (SUCCEEDED(hr))
                {
                    m_ExdiGdbServerData.gdbServer.coreConnectionParameters.push_back(gdbServer.coreConnectionParameter);
                    isSet = true;
                }
            
            }
            if (!isSet)
            {
                ReportXmlError(L"Failed setting a value in ConfigExdiGdbServerHelperImpl::HandleTagAttributeList()\n");
                hr = E_FAIL;
            }
        }
	    catch (exception & ex)
	    {
		    ReportExceptionError(ex.what());
	    }
        catch(...)
        {
            ReportXmlError(L"Unrecognized exception happened in ConfigExdiGdbServerHelperImpl::ProcessAttributeList()\n");
        }
        return hr;
    }

    HRESULT ConfigExdiGdbServerHelperImpl::ProcessAttributeList(_In_ TAG_ATTR_LIST * const pTagAttrList)
    {
        HRESULT hr = E_FAIL;
        __try
        {
            hr = HandleTagAttributeList(pTagAttrList);
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
		    ReportExceptionError(ex.what());
            throw;
	    }
        catch(...)
        {
            ReportXmlError(L"Unrecognized exception happened in ConfigExdiGdbServerHelperImpl::ParseAttributes()\n");
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

void ConfigExdiGdbServerHelper::GetExdiComponentAgentNamePacket(_Out_ wstring & packetName)
{
    assert(m_pConfigExdiGdbServerHelperImpl != nullptr);
    m_pConfigExdiGdbServerHelperImpl->GetExdiComponentAgentNamePacket(packetName);
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