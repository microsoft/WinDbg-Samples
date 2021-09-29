//----------------------------------------------------------------------------
//
// GdbSrvRspClient.cpp
//
// This class implements the RSP communication protocol used to communicate with the
// GdbServer stub. This class can be instantiated by using a TCPIP communication class
// or serial protocol class for connecting, sending and receiving data over the
// link layer (TCP/IP or RS-232).
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------
#include "stdafx.h"
#include <exception>
#include <assert.h>
#include <mstcpip.h>
#include "ExceptionHelpers.h"
#include "GdbSrvRspclient.h"
#include <regex>

using namespace GdbSrvControllerLib;

//=============================================================================
// Private defines and typedefs
//=============================================================================
//  Calculate the RSP packet length
#define CALC_RSP_PACKET_LENGTH(inputLenth)      (strlen("$") + inputLenth + strlen("#nn"))

//  Detect if the passed in character needs to be escaped
#define HANDLE_ESCAPE_SEQUENCE(ch)              ((ch == '$' || ch == '#' || ch == '{') ? true : false)

//  Return the status of the particular feature
#define IS_FEATURE_ENABLED(feature)             (GdbSrvRspClient<TConnectStream>::s_RspProtocolFeatures[feature].isEnabled)

#define GET_FEATURE_VALUE(feature)              (GdbSrvRspClient<TConnectStream>::s_RspProtocolFeatures[feature].featureDefaultValue)

#define GET_FEATURE_ENTRY(feature)              (GdbSrvRspClient<TConnectStream>::s_RspProtocolFeatures[feature])

#define GET_FEATURE_NAME(feature)               (GdbSrvRspClient<TConnectStream>::s_RspProtocolFeatures[feature].name)

#define SET_FEATURE_ENABLE(feature, enable)     (GdbSrvRspClient<TConnectStream>::s_RspProtocolFeatures[feature].isEnabled = enable)

//  Returns true if the interrupt event has been set
#define IS_INTERRUPT_EVENT_SET(interruptEvent)  (WaitForSingleObject(interruptEvent, 0) == WAIT_OBJECT_0)

//  Returns true if we need to continue resending the command request packet
#define IS_SEND_PACKET_DONE(ch, event)          ((ch != '+') && !IS_INTERRUPT_EVENT_SET(event))
//  Check if there is a NAK/Start data packet
#define IS_NAK_OR_START_PACKET(ch)              ((ch == '-') || (ch == '$'))

//  This type indicates the error structure used for displaying errors
typedef struct 
{
    int errorVal;
    LPCSTR description;
    LPCSTR actionHelper;
} ConnectStreamErrorStruct;


//=============================================================================
// Private data definitions
//=============================================================================    
//  Configuration default packet structure
template <class TConnectStream>
PacketConfig GdbSrvRspClient<TConnectStream>::s_RspProtocolFeatures[MAX_FEATURES] =
{
    {false, 0,      "VCont"},
    {false, 0,      "QStartNoAckMode"},
    {false, 2048,   "PacketSize"},
    {false, 0,      "qtrace32.memory"},
    {false, 0,      "Qtrace32.memory"},
    {false, 0,      "read.mrs"},
    {false, 0,      "write.mrs"},
    {false, 0,      "qXfer:features:read"},
};

//  List of command packets that do not require Acknowledgment packet
const char * listOfNotRequiredAckPackets[] =
{
    //  Set thread/processor packet
    "H"
};

//  Interrupt Packet
const char interruptPacket[] = {0x03};

//  Link Layer Configuration options
template <class TConnectStream>
RSP_CONFIG_COMM_SESSION GdbSrvRspClient<TConnectStream>::s_LinkLayerConfigOptions = {0};

//  List of socket stream connection errors
static const ConnectStreamErrorStruct tcpStreamErrors[] =
{
    {ERROR_HOST_DOWN,    
     "The remote system is not available or the system is not ready for accepting commands.\n"
     "The Target GdbServer is probably down or the target system is running.\n"},

    {WSAETIMEDOUT,    
     "The connection has been dropped because of a network failure or\n"
     "because the peer system failed to respond (The GdbServer did not response in time)\n",
	 "The GdbServer could not stop the target or we could lost the connection.\n"},

    {WSAEMSGSIZE,    
     "The message was too large to fit into the specified buffer and was truncated.",
     ""},

    {WSAESHUTDOWN,
     "A request to send or receive data was disallowed because the socket had already \n"
     "been shut down in that direction with a previous shutdown call.\n",
     ""},

    {WSAENETRESET,
     "the connection has been broken due to keep-alive activity that detected a failure \n"
     "while the operation was in progress.\n",
     ""},

    {WSAENOTCONN,
     "The socket is not connected.\n",
     ""},

    {WSAENOTSOCK,
     "The descriptor is not a socket.\n",
     ""},

    {WSANOTINITIALISED,
     "A successful WSAStartup call must occur before using this function.\n",
     ""},

    {WSAENETDOWN,
     "The network subsystem has failed.\n",
     ""},

    {WSAEINTR,
     "The socket was closed.\n",
     ""},

    {WSAEINPROGRESS,
     "A blocking Winsock call is in progress, or the service provider is still processing a callback function.\n",
     ""},

    {WSAEALREADY,
     "A nonblocking connect call is in progress on the specified socket.\n",
     ""},

    {WSAEADDRNOTAVAIL,
     "The remote address is not a valid address (such as ADDR_ANY).\n",
     ""},

    {WSAECONNABORTED,
     "The virtual circuit was terminated due to a time-out or other failure.\n",
     "The application should close the socket as it is no longer usable."},

    {WSAECONNRESET,
     "The virtual circuit was reset by the remote side executing a hard or abortive close.\n",
     "The application should close the socket as it is no longer usable."},

    {WSAEAFNOSUPPORT,
     "Addresses in the specified family cannot be used with this socket.\n",
     ""},

    {WSAECONNREFUSED,
     "The attempt to connect was forcefully rejected.\n",
     ""},

    {WSAEFAULT,
     "The name or namelen parameter is not a valid part of the user address space, the namelen parameter\n"
     "is too small, or the name parameter contains incorrect address format for the associated address family.\n",
     ""},

    {WSAEINVAL,
     "The parameter s is a listening socket.\n",
     ""},

    {WSAEISCONN,
     "The socket is already connected (connection-oriented sockets only).\n",
     ""},

    {WSAENETUNREACH,
     "The network cannot be reached from this host at this time.\n",
     ""},
};

//=============================================================================
// Private function definitions
//=============================================================================
//  FindErrorEntry  Finds an entry in the error table
//
//  Parameter:
//  errorCode       Error code to find
//
//  Return:
//  Pointer to the error code array entry.
//
const ConnectStreamErrorStruct * FindErrorEntry(_In_ int errorCode)
{
    const ConnectStreamErrorStruct * pEntry = nullptr;

    for (size_t index = 0; index < ARRAYSIZE(tcpStreamErrors); ++index) 
    {
        if (errorCode == tcpStreamErrors[index].errorVal) 
        {
            //  Display the error
            pEntry = &tcpStreamErrors[index];
            break;
        }
    }
    return pEntry;
}

//  GetErrorDescription Returnd the error description.
//
//  Parameter:
//  pEntry              Pointer to the entry.
//
//  Return:
//  Pointer to description string.
//
LPCSTR GetErrorDescription(_In_ const ConnectStreamErrorStruct * pEntry)
{
    assert(pEntry != nullptr);
    return pEntry->description;
}


//  EscapePacket    This function will escape the original string when the characters 
//                  '#' or '$' appear in the packet data. The escape character 
//                  is ASCII 0x7d ('}'), and is followed by the original character 
//                  XORed with 0x20. The character '}' itself must also be escaped. 
//
//  Parameter:
//  command         String command to check for any escape character.
//
//  Return:
//  The processed string for escape characters.
//
string EscapePacket(_In_ const string & command)
{
    string escapedOrigCommand(command);
    smatch characterMatch;
    bool found = regex_search(command, characterMatch, regex("[$#}]"));
    if (found)
    {
        for (size_t pos = 0; pos < characterMatch.size(); ++pos)
        {
            string escapedOriginalCh(1, (command[characterMatch.position(pos)] ^ 0x20));
            string temp = "{" + escapedOriginalCh;
            escapedOrigCommand.replace(characterMatch.position(pos), 1, temp);     
        }
    }
    return escapedOrigCommand;
}

//
//  MakeRunLengthEncoding   Implements the run-length encoding algorithm used by the RSP protocol.
//
//  Parameters:
//  pCommand                Pointer to the start of the string to check
//  remaining               Number of remaining characters in the string to check
//  pCheckSum               Pointer to the checksum variable
//  ppBuffer                Pointer to the pointer pointing to the processed character buffer
//
//  Return:
//  The number of characters that have been encoded.
//
//  Note.
//  The run-length encoding info can be found here:
//  http://www.embecosm.com/appnotes/ean4/embecosm-howto-rsp-server-ean4-issue-2.html
//
int MakeRunLengthEncoding(_In_reads_(remaining) const char * pCommand, _In_ int remaining,
                          _Inout_ unsigned int * pCheckSum, _Out_ char ** ppBuffer)
{
    assert(pCommand != nullptr && pCheckSum != nullptr && ppBuffer != nullptr);

    //  We cannot pass past '~' as this feature is suitable 
    //  for run-lengths of 4, 5 and 8-97
    int maxLen = (remaining > 97) ? 97 : remaining;

    int runLength;
    for (runLength = 1; runLength < maxLen; ++runLength)
    {
        if (pCommand[0] != pCommand[runLength])
        {
            break;
        }
    }
    //  Calculate the checksum and update the output buffer for the current character
    *pCheckSum += pCommand[0];
    *(*ppBuffer)++ = pCommand[0];
    if (--runLength >= 3)
    {
        //  yes, so there are characters that need to be encoded
        //  Skip the start & end data packets characters.  
        while (((runLength + 29) == '$') || ((runLength + 29) == '#'))
        {
            runLength--;
        }

        *pCheckSum += '*';
        *(*ppBuffer)++ = '*';
        *pCheckSum += static_cast<unsigned char>(runLength) + 29;
        *(*ppBuffer)++ = static_cast<char>(runLength) + 29;
        runLength++;
    }
    else
    {
        runLength = 1;
    }    
    return runLength;
}

//
//  ReceiveInternal     Reads a maximum packet length from the link layer and then
//                      returns each received character from the stored buffer.
//                      Indirectly returns the current character in the stored buffer.
//                      If the link layer function fails then it returns the
//                      error code rather than the number of remaining characters
//                      (this is similar to the Bekerly socket recv() function).
//
//  Parameters:
//  packetLength        Length of the expected packet.
//  pStream             Pointer to the TcpIpStream object.
//  resetBuffer         Flag indicating if we need to flush the current buffer.
//  pCurrentChar        Pointer to the current output character.
//
//  Return:
//  Number of remaining characters in the stored input buffer or the error 
//  returned from the stream receive function (SOCKET_ERROR).
//
//  Note.
//  This function will execute a read by requesting to receive a maximum number of characters
//  and then it'll dispatch the received characters until the buffer is empty or the 
//  caller requests reseting the input buffer.
//
int ReceiveInternal(_In_ int packetLength, _In_ TcpIpStream * const pStream,
                    _In_ bool resetBuffer, _Out_ char * pCurrentChar)
{
    assert(pStream != nullptr && pCurrentChar != nullptr);

    static unique_ptr<char> pReadInputStream = nullptr;
    static int readInputStreamCharCounter = 0;
    static char * pReadInputStreamBuffer = nullptr;
    static int maximumPacketLength = 0;

    if (resetBuffer || pReadInputStreamBuffer == nullptr)
    {
        maximumPacketLength = static_cast<int>(CALC_RSP_PACKET_LENGTH(packetLength));
        pReadInputStream = unique_ptr <char>(new (nothrow) char[maximumPacketLength]);
        assert(pReadInputStream != nullptr);
        readInputStreamCharCounter = 0;
    }

    if (readInputStreamCharCounter == 0)
    {
        pReadInputStreamBuffer = pReadInputStream.get();
        memset(pReadInputStreamBuffer, 0x00, maximumPacketLength);
        readInputStreamCharCounter = pStream->Receive(pReadInputStreamBuffer, maximumPacketLength);
        if (readInputStreamCharCounter == SOCKET_ERROR)
        {
            pReadInputStreamBuffer = nullptr;
        }
    }
    if (pReadInputStreamBuffer != nullptr)
    {
        readInputStreamCharCounter--;
        *pCurrentChar = *pReadInputStreamBuffer;
        pReadInputStreamBuffer++;
    }
    return readInputStreamCharCounter;
}

//
//  BuildRspPacket  Builds the RSP packet
//
//  Parameters:
//  pStream         Pointer to the TcpIpStream object.
//  outData         Reference to the built packet.
//  checkSum        Reference to the checksum of the built packet data
//
//  Return:
//  If there is no any error then it returns the packet length  
//  Otherwise the error.
//
int BuildRspPacket(_In_ TcpIpStream * const pStream, _Out_ string & outData, _Out_ unsigned int & checkSum)
{
    assert(pStream != nullptr);

    int readStatus;
    char currentChar;
    checkSum = 0;

    for(;;)
    {
        readStatus = ReceiveInternal(0, pStream, false, &currentChar);
        if (readStatus == SOCKET_ERROR)
        {
            break;
        }
        if (currentChar == '#')
        {
            checkSum %= 256;
            break;
        }
        outData += currentChar;
        checkSum += currentChar;
    }    
    return readStatus;
}

//
//  IsValidRspPacket    Validates the RSP packet checksum.
//                      It compares the received packet checksum fields with the calculated checksum.
//
//  Parameters:
//  pStream             Pointer to the TcpIpStream object.
//  outData             The calculated checksum of the received packet
//  isNoAckModeEnabled  Flag indicating if the ACK mode is not enabled
//  inputRspData        Reference to the received data packet without the checksum.
//  outRspData          If the checksum field is correct then it contains the outputed string.
//
//  Return:
//  true                If both checksums match.
//  false               Otherwise.
//
bool IsValidRspPacket(_In_ TcpIpStream * const pStream, _In_ unsigned int checkSum, _In_ bool isNoAckModeEnabled, 
                      _In_ const string & inputRspData, _Out_ string & outRspData)
{
    assert(pStream != nullptr);
    bool isDone = false;
    unsigned char checkSumL = 0;
    unsigned char checkSumR = 0;

    //  Verify the checksum
    int readStatus = ReceiveInternal(0, pStream, false, reinterpret_cast<char *>(&checkSumL));
    if (readStatus != SOCKET_ERROR)
    {
        readStatus = ReceiveInternal(0, pStream, false, reinterpret_cast<char *>(&checkSumR));
        if (readStatus != SOCKET_ERROR)
        {
            checkSumL = ((AciiHexToNumber(checkSumL) << 4) & 0xf0);
            unsigned int packetCheckSum = (AciiHexToNumber(checkSumR) & 0x0f) | checkSumL;
            if (checkSum == packetCheckSum)
            {
                //  Checksum matched, so try to send an ACK
                if (!isNoAckModeEnabled)
                {
                    pStream->Send("+", 1);
                }
                //  Build the output response
                outRspData.assign(inputRspData);
                isDone = true;

            }
            else
            {
                if (!isNoAckModeEnabled)
                {
                    //  Send the NAK 
                    pStream->Send("-", 1);
                }
            }
        }
    }
    return isDone;
}

#if 0
//  CreateSendRspPacketWithRunLengthEncoding    Createa a RSP send packet with run-length encoding.
//  Note.
//  This function has not been fully tested, so please ensure testing before using it.
//
string GdbSrvRspClient<TcpConnectorStream>::CreateSendRspPacketWithRunLengthEncoding(_In_ const string & command)
{
    //  Try to see if we need to escape the $/#/} characters in the request.
    //  As far as RSP states so far there is no a RSP command having $/#/} as part of the request command.
    string packetToSend = EscapePacket(command);

    //  Set the processing buffer
    int commandLength = static_cast<int>(packetToSend.length());
    unique_ptr <char> outPacketToSend(new char[commandLength + 1]);
    char * pOutPacketToSend = outPacketToSend.get();
    memset(pOutPacketToSend, 0x00, commandLength + 1);

    //  Try to add a run-length encoding to the packet data. I'm not sure if this is required
    //  for the RSP client, but it's one more issue that is not clear from the RSP doc.
    const char * pCommand = packetToSend.c_str();
    unsigned int checksum = 0;
    for (int index = 0, encodedLength = 0; index < commandLength; index += encodedLength)
    {
        //  If the encoding is required the add the encoding length to the current proccessed buffer character, 
        //  otherwise update the output buffer and calculate the checksum for the current processed character.
        encodedLength = MakeRunLengthEncoding(&pCommand[index], commandLength - index, &checksum, &pOutPacketToSend);
    }
    string outPacketData(outPacketToSend.get(), strlen(outPacketToSend.get()));

    //  Put the start marker of the data packet
    outPacketData.insert(0, "$");
    //  Put the end marker of the data packet
    outPacketData.insert(outPacketData.end(), 1, '#');

    //  Put the checksum as two ascii hex digits
    outPacketData.insert(outPacketData.end(), 1, NumberToAciiHex(((checksum >> 4) & 0xf)));
    outPacketData.insert(outPacketData.end(), 1, NumberToAciiHex((checksum & 0xf)));

    return outPacketData;
}
#endif

//
//  IsReceiveInterrupt  Check if we need to interrupt the ongoing receiving sequence.
//
//  Parameters:
//  readStatus          The status of the last read action.
//  isRspWaitNeeded     Flag is true when we are inside of the no receive timeout session.
//  interruptEvent      Handle of the interrupt event
//
//  Return:
//  true                If we need to interrupt the waiting sequence because the user cancel
//                      or a link layer error ocurred.
//  false               Otherwise.
//
inline bool IsReceiveInterrupt(_In_ int readStatus, _In_ bool isRspWaitNeeded, _In_ HANDLE interruptEvent)
{
    bool isInterrupted = false;
    
    if (readStatus == SOCKET_ERROR && isRspWaitNeeded)
    {
        isInterrupted = true;                
    }
    else if(IS_INTERRUPT_EVENT_SET(interruptEvent))
    {
        isInterrupted = true;                
    }
    return isInterrupted; 
}                                                               

//
//  WaitForRspPacketStart   Waits for the start packet character to arrive from the server.
//
//  Parameters:
//  maxPacketLength         Expected packet length
//  pStream                 Pointer to the TcpIpStream object.
//  isRspWaitNeeded         Flag true if we need to wait until the packet arrive (ignore timeout).
//  IsPollingChannelMode    Flag set if the current mode requires polling all channels.
//  fResetBuffer            Flag indicates if we need to reset any pending data in the local cached buffer.
//  
//  Return:
//  The number of received characters. 
//
int GdbSrvRspClient<TcpConnectorStream>::WaitForRspPacketStart(_In_ int maxPacketLength, _In_ TcpIpStream * const pStream, 
                                                               _In_ bool isRspWaitNeeded, _Inout_ bool & IsPollingChannelMode,
                                                               _In_ bool fResetBuffer)
{
    assert(pStream != nullptr && maxPacketLength != 0);
    char currentChar;
    int readStatus;
    bool reset = fResetBuffer;

    //  Wait for the packet start character to arrive.
    do
    {
        readStatus = ReceiveInternal(maxPacketLength, pStream, reset, &currentChar);
        //  Do we need to exit the receiving sequence?
        if (IsReceiveInterrupt(readStatus, isRspWaitNeeded, m_interruptEvent.Get()))
        {
            IsPollingChannelMode = false;
            break;
        }
        reset = false;            
    }
    while (currentChar != '$' && !IsPollingChannelMode);

    return readStatus;
}

//
//  CreateSendRspPacket     Creates a Rsp request packet.
//
//  Parameters:
//  command                 Reference to the request command data.
//
//  Return:
//  The Rsp formated string.
//
string GdbSrvRspClient<TcpConnectorStream>::CreateSendRspPacket(_In_ const string & command)
{
    //  Try to see if we need to escape the $/#/} characters in the request.
    //  As far as RSP states so far there is no request command having $/#/} characters.
    string packetToSend = EscapePacket(command);
    
    unsigned int checkSum = 0;
    for (auto pos = packetToSend.begin(); pos != packetToSend.end(); ++pos)
    {
        //  Calculate the checksum and update the output buffer for the current character
        checkSum += *pos;
    }
    checkSum %= 256;

    //  Put the start marker of the data packet
    packetToSend.insert(0, "$");
    //  Put the end marker of the data packet
    packetToSend.insert(packetToSend.end(), 1, '#');

    //  Put the checksum as two ascii hex digits
    packetToSend.insert(packetToSend.end(), 1, NumberToAciiHex(((checkSum >> 4) & 0xf)));
    packetToSend.insert(packetToSend.end(), 1, NumberToAciiHex((checkSum & 0xf)));

    return packetToSend;
}

//  SetProtocolFeatureValue     Set the Protocol feature value field
inline void GdbSrvRspClient<TcpConnectorStream>::SetProtocolFeatureValue(_In_ size_t index, _In_ int value)
{
    (GET_FEATURE_ENTRY(index)).featureDefaultValue = value;
}

//  SetProtocolFeatureFlag      Set the Protocol feature flag field
inline void GdbSrvRspClient<TcpConnectorStream>::SetProtocolFeatureFlag(_In_ size_t index, _In_ bool value)
{
    (GET_FEATURE_ENTRY(index)).isEnabled = value;
}

//
//  GetNoAckModeRequired        Check if the command needs ACK mode.
//
//  command                     command to check.
//
//  Returns
//  true                        The command does not need ack mode.
//  false                       Otherwise.
//
inline bool GdbSrvRspClient<TcpConnectorStream>::GetNoAckModeRequired(_In_ const string & command)
{
    bool isNoAckMode = false;

    //  Is ACK packet mode enabled?
    if (!IS_FEATURE_ENABLED(PACKET_QSTART_NO_ACKMODE))
    {
        size_t numberOfElements = ARRAYSIZE(listOfNotRequiredAckPackets);
        for (size_t index = 0; index < numberOfElements; ++index)
        {
            if (strcmp(listOfNotRequiredAckPackets[index], command.c_str()) == 0)
            {
                isNoAckMode = true;
                break;
            }
        }
    }
    else
    {
        isNoAckMode = true;        
    }
    return isNoAckMode;
}

//=============================================================================
// Public function definitions
//=============================================================================
//
//  SendRspPacket   Sends a RSP packet to the GdbServer.
//
//  Parameters:
//  command         Reference to a string containing the request data (command)
//
//  Return:
//  true            if succeeded sending the RSp packet.
//  false           Otherwise.
//  activeCore      Current active processor core.
//  
//  Note.
//  This function will create and send a packet with the following format:
//  $<data>#<2 bytes digits checksum>
//  
//  The GdbServer will ACK/NAK the packet, so If the checksum validation succeeded 
//  then the GdbServer will send an ACK packet ('+').
//  If GdbServer NAK the packet ('-') then we will resend the packet until we get 
//  the ACK or the user cancel the sending sequence.
//  
bool GdbSrvRspClient<TcpConnectorStream>::SendRspPacket(_In_ const string & command, _In_ unsigned activeCore)
{
    assert(m_pConnector != nullptr);

    try
    {
        scoped_lock packetGuard(m_gdbSrvRspLock);
        bool isDone = true;

        //  Create the packet to send
        string packetToSend = CreateSendRspPacket(command);

        //  Does we require ACK?
        bool isNoAckMode = GetNoAckModeRequired(command);

        TcpIpStream * pTcpStream = m_pConnector->GetLinkLayerStreamEntry(activeCore);
        assert(pTcpStream != nullptr);

        char ackCharacter[1] = {0};
        int sendResult = 0;
        int retryCounter = 0;
        bool isSendPacket = true;
        do
        {
            //  Send it over the Link layer until we receive an ACK from GdbServer
            if (isSendPacket)
            {
                sendResult = pTcpStream->Send(packetToSend.c_str(), static_cast<int>(packetToSend.length()));
                if (sendResult == SOCKET_ERROR) 
                {
                    isDone = false;
                    break;
                }
                isSendPacket = false;
            }
            //  Is no ACK packet mode enabled?
            if (isNoAckMode)
            {
                //  We do not need + packet, so exit here
                break;
            }
            //  Try to read the +/- (ACK/NAK) packet
            sendResult = pTcpStream->Receive(ackCharacter, sizeof(ackCharacter)); 
            if (sendResult == SOCKET_ERROR) 
            {
                //  Did we reach the maximum retry attempts?
                if (IS_MAX_ATTEMPTS(++retryCounter))
                {
                    isDone = false;
                    break;
                }
                else
                {                    
                    isSendPacket = true;
                }
            }
            //  Is it a start data packet?
            //  The start packet ($) checking is due to a GdbServer error, so force retrying again as the packet 
            //  has not been Acked yet.
            isSendPacket = IS_NAK_OR_START_PACKET(ackCharacter[0]);

            //  Should we check here for the '\03' interrupt request from the GdbServer?
            //  It's not clear if the GdbServer can send us break request provoked by the target.
            //  If yes then we should add the break checking (\x03) here and send/read then
            //  the stop reason package that will continue this break before exiting from this function.
        }
        while (IS_SEND_PACKET_DONE(ackCharacter[0], m_interruptEvent.Get()));
    
        return isDone;
    }
    CATCH_AND_RETURN_BOOLEAN
}

//
//  ReceiveRspPacketEx   Receives a RSP packet from the GdbServer.
//
//  Parameters:
//  isRspWaitNeeded      Flag indicates if we need to wait forever for the incoming Gdbserver packet.
//  response             Reference to the ouputed RSP packet string if the function succeed.
//  activeCore           Current active processor core.
//  IsPollingChannelMode Flag set if the current mode requires polling all channels
//  fResetBuffer         Flag indicates if we need to reset any pending data in the cached buffer.
//  
//  Return:
//  true                The received RSP packet is correct.
//  false               Otherwise.
//
//  Note.
//  The RSP packet is expected in the format:
//  $<data>#<2 bytes digits checksum>
//  The function validates the checksum and sends a CK/NAK (+/-) (if the ackmode is enabled).
//  If we receive a valid packet then it disables polling mode.
//  
bool GdbSrvRspClient<TcpConnectorStream>::ReceiveRspPacketEx(_Out_ string & response, _In_ unsigned activeCore, 
                                                             _In_ bool isRspWaitNeeded, _Inout_ bool & IsPollingChannelMode,
                                                             _In_ bool fResetBuffer)
{
    assert(m_pConnector != nullptr);
    try
    {
        bool isDone = false;

        scoped_lock packetGuard(m_gdbSrvRspLock);
        //  Verify if we have set the maximum response packet, if so then use
        //  this value as the maximum response
        int maxPacketLength = GET_FEATURE_VALUE(PACKET_SIZE);
        //  Get the current active core tcp stream object.
        TcpIpStream * pTcpStream = m_pConnector->GetLinkLayerStreamEntry(activeCore);
        assert(pTcpStream != nullptr);
        //  Wait for the first packet character '$' to arrive
        if (WaitForRspPacketStart(maxPacketLength, pTcpStream, isRspWaitNeeded, IsPollingChannelMode, fResetBuffer) != SOCKET_ERROR)
        {
            string replyPacket;
            replyPacket.reserve(response.length());
            unsigned int checkSum = 0;
            //  Build the data packet
            if (BuildRspPacket(pTcpStream, replyPacket, checkSum) != SOCKET_ERROR)   
            {
                //  Verify if the RSP checksum is valid, if so, then output the response string
                if (IsValidRspPacket(pTcpStream, checkSum, IS_FEATURE_ENABLED(PACKET_QSTART_NO_ACKMODE), 
                                     replyPacket, response))
                {
                    isDone = true;
                    //  Disable polling mode
                    IsPollingChannelMode = false;
                }
            }
        }
        return isDone;
    }
    CATCH_AND_RETURN_BOOLEAN
}

//
//  ConfigRspSession    Set the RSP session communication parameters.
//                      Also, it sets the TCP stream link layer options.
//
//  Parameters:
//  pConfigData         Pointer to the configuration session data.
//  core                if C_ALLCORES, then configure all cores, otherwise only specific core.
//
//  Return:
//  true                if we succeeded setting up the stream connection.
//  false               Otherwise.
//
bool GdbSrvRspClient<TcpConnectorStream>::ConfigRspSession(_In_ const RSP_CONFIG_COMM_SESSION * pConfigData,
                                                           _In_ unsigned core)
{
    assert(m_pConnector != nullptr && pConfigData != nullptr);
    scoped_lock packetGuard(m_gdbSrvRspLock);

    bool configDone = true;
    bool isAllCores = (core == C_ALLCORES) ? true : false;
    memcpy(&s_LinkLayerConfigOptions, pConfigData, sizeof(s_LinkLayerConfigOptions));
    size_t totalNumberOfProcessorCores = m_pConnector->GetNumberOfConnections();
    for (size_t coreNumber = 0; coreNumber < totalNumberOfProcessorCores; ++coreNumber)
    {
        if (isAllCores || core == coreNumber)
        {
            TcpIpStream * pStream = m_pConnector->GetLinkLayerStreamEntry(coreNumber);
            assert(pStream != nullptr);

            //  Set the call back function
            if (pConfigData->pDisplayCommDataFunc != nullptr && pConfigData->pTextHandler != nullptr)
            {
                pStream->SetCallBackDisplayFunc(pConfigData->pDisplayCommDataFunc, pConfigData->pTextHandler);
            }

            //  Disable Nagle algorithm
            bool isNagleAlgorithmDisabled = true;
            if (pStream->SetOptions(IPPROTO_TCP, TCP_NODELAY, 
                                    reinterpret_cast<const char *>(&isNagleAlgorithmDisabled),
                                    sizeof(isNagleAlgorithmDisabled)) == SOCKET_ERROR)
            {
                configDone = false;
                break;
            }

            unsigned char ackFrequency = 1;
            long unsigned int bytesReturned = 0;
            if (pStream->SetWSAIoctl(SIO_TCP_SET_ACK_FREQUENCY, &ackFrequency, 
                                     sizeof(ackFrequency), nullptr, 0, &bytesReturned) == SOCKET_ERROR)
            {
                configDone = false;
                break;
            }

            //  Enable TCP keep alive packets, so we can check if the GdbServer is alive
            DWORD isKeepAlive = 1;
            if (pStream->SetOptions(SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char *>(&isKeepAlive),
                                    sizeof(isKeepAlive)) == SOCKET_ERROR)
            {
                configDone = false;
                break;
            }

            int resultRecv = 0;
            if (pConfigData->recvTimeout != 0)
            {
                resultRecv = pStream->SetOptions(SOL_SOCKET, SO_RCVTIMEO, 
                                                 reinterpret_cast<const char *>(&pConfigData->recvTimeout),
                                                 sizeof(pConfigData->recvTimeout));
            }

            int resultSend = 0;
            if (pConfigData->sendTimeout != 0)
            {
                resultSend = pStream->SetOptions(SOL_SOCKET, SO_SNDTIMEO, 
                                                 reinterpret_cast<const char *>(&pConfigData->sendTimeout),
                                                 sizeof(pConfigData->sendTimeout));
            }
            if (resultRecv == SOCKET_ERROR || resultSend == SOCKET_ERROR)
            {
                configDone = false;
                break;
            }

        }
    }
    return configDone;
}

//
//  GetRspSessionStatus Get the connection status of the link layer session.  
//                      This can be used by the high layers for implementing keep alive mechanism.
//
//  Parameters:
//  error               A reference to the error code returned by the link layer.
//                      If this function detected that we lost connection then
//                      it contains an injected abort error code.
//  core                Processor core to check for RspConnection. If C_ALLCORES, then check all cores.
//
//  Return:
//  true                If this function succeeded retrieving the link layer status.
//
//  false               If this function failed retrieving the link layer status as 
//                      we do not have a physical connection yet.
//
//  Note.
//  If there is an error then it returns indirectly it.
//
bool GdbSrvRspClient<TcpConnectorStream>::GetRspSessionStatus(_Out_ HRESULT & error, _In_ unsigned core)
{
    assert(m_pConnector != nullptr);
    bool isDone = false;

    bool isAllCores = (core == C_ALLCORES) ? true : false;
    size_t totalNumberOfProcessorCores = m_pConnector->GetNumberOfConnections();
    for (size_t coreNumber = 0; coreNumber < totalNumberOfProcessorCores; ++coreNumber)
    {
        if (isAllCores || coreNumber == core)
        {
            TcpIpStream * pStream = m_pConnector->GetLinkLayerStreamEntry(coreNumber);
            assert(pStream != nullptr);

            if (m_pConnector->IsConnected())
            {
                fd_set fdSetRead;
                fd_set fdSetError;
                struct timeval selectTimeout;
                selectTimeout.tv_sec = 0;
                selectTimeout.tv_usec = 500;       

                int numberOfSockets = pStream->Select(&fdSetRead, nullptr, &fdSetError, &selectTimeout);
                if (numberOfSockets != SOCKET_ERROR && numberOfSockets != 0)
                {
                    if (pStream->IsFDSet(&fdSetRead) != 0)
                    {
                        //  Ensure that the connection has been closed/terminated by trying to see
                        //  if this is not a close/reset/terminate false positive event, so we will try peeking the data
                        //  If there is a pending data then this is not caused by a close/terminating event.
                        //  Change the socket mode to non-blocking.
                        u_long mode = 1;
                        int modeStatus = pStream->Ioctlsocket(FIONBIO, &mode);
                        if (modeStatus != SOCKET_ERROR)
                        {
                            //  Look for any pending data
                            char tempBuffer[1];
                            if (pStream->Peek(tempBuffer, sizeof(tempBuffer), MSG_PEEK) != NO_ERROR)
                            {
                                //  We got an error, so find out if this is a lost socket connection type of error.
                                int peekerror = m_pConnector->GetLastError();
                                if (m_pConnector->IsConnectionLost(peekerror))
                                {
                                    //  We lost connection, so abort the GDB RSP session.
                                    error = ERROR_OPERATION_ABORTED;
                                }
                            }
                            if (error != ERROR_OPERATION_ABORTED)
                            {
                                //  Change back to blocking mode
                                mode = 0;
                                if (pStream->Ioctlsocket(FIONBIO, &mode) == SOCKET_ERROR)
                                {
                                    // This should be a fatal error as we cannot set back the previous socket mode.
                                    error = ERROR_OPERATION_ABORTED;
                                }
                            }
                        }
                        else
                        {
                            error = HRESULT_FROM_WIN32(m_pConnector->GetLastError());    
                        }
                    }
                    //  Is there a socket error?
                    else if (pStream->IsFDSet(&fdSetError) != 0)
                    {
                        DWORD socketError = 0;
                        DWORD socketErrorLength = sizeof(socketError);
                        int errorStatus = pStream->GetOptions(SOL_SOCKET, SO_ERROR, 
                                                              reinterpret_cast<char *>(&socketError),
                                                              reinterpret_cast<int *>(&socketErrorLength));
                        if (errorStatus != SOCKET_ERROR)
                        {
                            error = HRESULT_FROM_WIN32(socketError);
                        }
                        else
                        {
                            error = HRESULT_FROM_WIN32(m_pConnector->GetLastError());    
                        }
                    }
                }
                else
                {
                    error = HRESULT_FROM_WIN32(m_pConnector->GetLastError());    
                }
                isDone = true;
            }
        }
    }
    return isDone;
}

//
//  UpdateRspPacketFeatures   Updates the local cache for the GdbServer supported features, so
//                            this function will parse the response and 
//                            stores the features in the local packetEnableFeatures array.
//  Parameters:
//  reply                     Reference to the string containing the feature reply (see below the format)
//
//  Return:
//  true                      always.
//
//  Note.
//  Reply format can be found here:
//  https://sourceware.org/gdb/onlinedocs/gdb/General-Query-Packets.html#qSupported
//
bool GdbSrvRspClient<TcpConnectorStream>::UpdateRspPacketFeatures(_In_ const string & reply)
{
    try
    {
        scoped_lock packetGuard(m_gdbSrvRspLock);
        if (!reply.empty())
        {
            for (int index = 0; index < MAX_FEATURES; ++index)
            {
                string::size_type pos = reply.find(GET_FEATURE_NAME(index));
                if (pos != string::npos)
                {
                    //  Looking for featureName=
                    pos += (GET_FEATURE_NAME(index)).length();
                    if (reply[pos] == '=')
                    {
                        //  This feature contains a value, so extract it
                        SetProtocolFeatureValue(index, stoi(reply.substr(pos + 1), nullptr, 16));
                    }
                    else if (reply[pos] == '+')
                    {
                        //  This feature contains an enabled flag, so set the flag.
                        SetProtocolFeatureFlag(index, true);
                    }
                    else if (reply[pos] == '-')
                    {
                        SetProtocolFeatureFlag(index, false);
                    }
                }
            }
        }
        return true;
    }
    CATCH_AND_RETURN_BOOLEAN
}

//
//  GetRspPacketFeatures    Retrieves the current stored features.
//
//  Parameters:
//  pConfig                 Pointer to the output buffer stored configuration.
//  index                   Feature index.
//
//  Returns:
//  Nothing.
//
void GdbSrvRspClient<TcpConnectorStream>::GetRspPacketFeatures(_Out_ PacketConfig * pConfig, _In_ RSP_FEATURES index)
{
    assert(pConfig != nullptr);
    scoped_lock packetGuard(m_gdbSrvRspLock);

    pConfig->isEnabled = s_RspProtocolFeatures[index].isEnabled;
    pConfig->featureDefaultValue = s_RspProtocolFeatures[index].featureDefaultValue;
    pConfig->name += s_RspProtocolFeatures[index].name;
}

//
//  ConnectRsp  Connects to the remote GdbServer
//
//  Returns:
//  true        if the connection succeeded.
//  false       Otherwise.
//
bool GdbSrvRspClient<TcpConnectorStream>::ConnectRsp()
{
    assert(m_pConnector != nullptr);
    scoped_lock packetGuard(m_gdbSrvRspLock);

    unsigned int retries = (s_LinkLayerConfigOptions.connectAttempts == 0) ? 1 :
                            s_LinkLayerConfigOptions.connectAttempts;
    return m_pConnector->Connect(retries);
}

//
//  AttachRspToCore  Create a new core TCP stream channel and connect to it.
//
//  Parameters:
//  connectionStr    Connection string.
//  core             Processor core to attach.
//
//  Returns:
//  true            if the attach operation succeeded (create a new channel and connect to it).
//  false           Otherwise.
//
bool GdbSrvRspClient<TcpConnectorStream>::AttachRspToCore(_In_ const wstring &connectionStr, 
                                                          _In_ unsigned core)
{
    assert(m_pConnector != nullptr);
    bool isAttached = false;

    scoped_lock packetGuard(m_gdbSrvRspLock);
    if (connectionStr.empty() || core == C_ALLCORES || core > m_pConnector->GetNumberOfConnections())
    {
        return isAttached;
    }

    isAttached = m_pConnector->TcpOpenStreamCore(connectionStr, core);
    if (isAttached)
    {
        unsigned int retries = (s_LinkLayerConfigOptions.connectAttempts == 0) ? 1 :
                                s_LinkLayerConfigOptions.connectAttempts;
        isAttached = m_pConnector->TcpConnectCore(retries, core);
    }
    return isAttached;
}

//
//  ConnectRspToCore  Connect to specific TCP stream channel
//
//  Parameters:
//  connectionStr    Connection string
//  core             Processor core to connect.
//
//  Returns:
//  true        if the TCT stream connect succeeded.
//  false       Otherwise.
//
bool GdbSrvRspClient<TcpConnectorStream>::ConnectRspToCore(_In_ const wstring &connectionStr, 
                                                           _In_ unsigned core)
{
    assert(m_pConnector != nullptr);

    scoped_lock packetGuard(m_gdbSrvRspLock);
    if (connectionStr.empty() || core == C_ALLCORES || core > m_pConnector->GetNumberOfConnections())
    {
        return false;
    }
    unsigned int retries = (s_LinkLayerConfigOptions.connectAttempts == 0) ? 1 :
                            s_LinkLayerConfigOptions.connectAttempts;
    return m_pConnector->TcpConnectCore(retries, core);
}

//
//  CloseRspCore    Close the RSP protocol for the passed in processor channel.
//
//  Parameters:
//  core            Processor core to close.
//
//  Returns:
//  true            if the close operation succeeded.
//  false           Otherwise.
//
bool GdbSrvRspClient<TcpConnectorStream>::CloseRspCore(_In_ const wstring &closeStr, 
                                                       _In_ unsigned core)
{
    UNREFERENCED_PARAMETER(closeStr);
    assert(m_pConnector != nullptr);

    scoped_lock packetGuard(m_gdbSrvRspLock);
    bool isClosed = false;
    if (core == C_ALLCORES || core > m_pConnector->GetNumberOfConnections())
    {
        return isClosed;
    }

    return m_pConnector->TcpCloseCore(core);
}

//
//  GetRspLastError Retrieves the last error from the link layer
//
//  Returns:
//  The stream class stored error.
//
int GdbSrvRspClient<TcpConnectorStream>::GetRspLastError() 
{
    assert(m_pConnector != nullptr);
    return m_pConnector->GetLastError();
}

//
//  ShutDownRsp     Shutdown the RSP protocol session by requesting closing the connection..
//
//  Returns:
//  true            if no error during closing the connection.
//  false           otherwise.
//
bool GdbSrvRspClient<TcpConnectorStream>::ShutDownRsp()
{
    assert(m_pConnector != nullptr);

    return m_pConnector->Close();
}

//
//  SendRspInterruptEx  Interrupt the target by sending a RSP interrup packet.
//                      This packet contains only one character 0x03 
//                      without a RSP protocol packet structure
//                      (it does not have any response).    
//
//  Parameters:
//  fResetAllCores      Flag if true, then we need to reset all cores
//                      if false, then the second parameter (activeCore) will
//                      have the core value that needs to be skipped.
//  activeCore          Contains the core that will be skipped from being interrupted.
//
//  Return:
//  true                if we succeeded sending the interrupt break sequence.
//  false               Otherwise.
//
//  Request:
//      0x03
//  Response: 
//      Nothing.
//  
//  Note.
//  !!! The interrupt command does not have a response, but we should not try
//      reading characters without using the critical section read internal buffer protection !!!
//  The interrupt command will generate sending a stop reason reply packet by the GdbServer and 
//  this packet will be received from the main working command thread.
//
bool GdbSrvRspClient<TcpConnectorStream>::SendRspInterruptEx(_In_ bool fResetAllCores, _In_ unsigned activeCore)
{
    assert(m_pConnector != nullptr);
    bool isDone = false;

    size_t totalNumberOfProcessorCores = m_pConnector->GetNumberOfConnections();
    for (size_t coreNumber = 0; coreNumber < totalNumberOfProcessorCores; ++coreNumber)
    {
        if (fResetAllCores || (coreNumber != activeCore))
        {
            TcpIpStream * pStream = m_pConnector->GetLinkLayerStreamEntry(coreNumber);
            assert(pStream != nullptr);

            int sendResult = pStream->Send(interruptPacket, static_cast<int>(strlen(interruptPacket)));
            if (sendResult != SOCKET_ERROR) 
            {
                //  Set the interrupt event 
                SetEvent(m_interruptEvent.Get());
                //  Wait a little bit for the GdbServer to process the break request packet.
                Sleep(200);
                isDone = true;
            }
        }
    }
    return isDone;
}

//
//  HandleRspErrors     Finds the RSP communication errors.    
//
//  Parameters:
//  testxType           Text type (command/output/error)
//
//  Return:
//  Nothing.
//
void GdbSrvRspClient<TcpConnectorStream>::HandleRspErrors(_In_ GdbSrvTextType textType)
{
    assert(m_pConnector != nullptr);
    int errorCode = m_pConnector->GetLastError();

    const ConnectStreamErrorStruct * pEntry = nullptr;
    size_t totalNumberOfProcessorCores = m_pConnector->GetNumberOfConnections();
    for (size_t coreNumber = 0; coreNumber < totalNumberOfProcessorCores; ++coreNumber)
    {
        TcpIpStream * pStream = m_pConnector->GetLinkLayerStreamEntry(coreNumber);
        assert(pStream != nullptr);

        pEntry = FindErrorEntry(errorCode);
        if (pEntry != nullptr)
        {
            pStream->CallDisplayFunction(pEntry->description, textType);
            break;
        }
    }
    if (pEntry == nullptr)
    {
        char errorString[128] = {0};
        _snprintf_s(errorString, _TRUNCATE, "The socket error 0x%x ocurred", errorCode);        
        TcpIpStream * pStream = m_pConnector->GetLinkLayerStream();
        assert(pStream != nullptr);
        pStream->CallDisplayFunction(errorString, textType);
    }
}

//
//  DiscardResponse    Discard any pending response.
//  
//  Parameters:
//  activeCore         The processor number that has to be skipped from discarding the response
//
//  Return:
//  Nothing.
//
void GdbSrvRspClient<TcpConnectorStream>::DiscardResponse(_In_ unsigned activeCore)
{
    assert(m_pConnector != nullptr);

    scoped_lock packetGuard(m_gdbSrvRspLock);
    bool IsPollingChannelMode = true;
    size_t totalNumberOfProcessorCores = m_pConnector->GetNumberOfConnections();
    for (unsigned coreNumber = 0; coreNumber < totalNumberOfProcessorCores; ++coreNumber)
    {
        if (coreNumber != activeCore)
        {
            TcpIpStream * pStream = m_pConnector->GetLinkLayerStreamEntry(coreNumber);
            assert(pStream != nullptr);

            string result;
            bool isRecvDone = ReceiveRspPacketEx(result, coreNumber, false, IsPollingChannelMode, true);
            if ((!isRecvDone && IsPollingChannelMode) || result.empty())
            {
                //  Try to interrupt
                pStream->Send(interruptPacket, static_cast<int>(strlen(interruptPacket)));
            }
            else
            {
                // Is the target running/power down packet?
                if (result.find("S00") != string::npos)
                {
                    pStream->CallDisplayFunction(GetErrorDescription(FindErrorEntry(ERROR_HOST_DOWN)), 
                                                 GdbSrvTextType::CommandError);                
                }
            }
        }
    }
}

//
//  IsFeatureEnabled    Check if the feature is enabled.
//  
//  Parameters:
//  feature             Feature to check
//
//  Return:
//  true                The feature is enabled
//  false               The feature is disabled
//
bool GdbSrvRspClient<TcpConnectorStream>::IsFeatureEnabled(_In_ unsigned feature)
{
    return IS_FEATURE_ENABLED(feature);
}

//
//  IsFeatureEnable     Set enable the feature.
//  
//  Parameters:
//  feature             Feature to check
//
//  Return:
//  Nothing.
//
void GdbSrvRspClient<TcpConnectorStream>::SetFeatureEnable(_In_ unsigned feature)
{
    SET_FEATURE_ENABLE(feature, true);
}


//
//  GetNumberOfStreamConnections    Get the number of ongoing connections
//  
//  Parameters:
//
//  Return:
//  Number of channel stream connections.
//
size_t GdbSrvRspClient<TcpConnectorStream>::GetNumberOfStreamConnections()
{
    assert(m_pConnector != nullptr);

    scoped_lock packetGuard(m_gdbSrvRspLock);
    return m_pConnector->GetNumberOfConnections();
}

GdbSrvRspClient<TcpConnectorStream>::GdbSrvRspClient(_In_ const vector<wstring> &coreConnectionParameters) :
                                     m_interruptEvent(CreateEvent(nullptr, FALSE, FALSE, nullptr)),
                             	     m_pConnector(unique_ptr<TConnectStream>(new (nothrow) TConnectStream(coreConnectionParameters)))
{
    InitializeCriticalSection(&m_gdbSrvRspLock);
}

GdbSrvRspClient<TcpConnectorStream>::~GdbSrvRspClient()
{
    ShutDownRsp();
    DeleteCriticalSection(&m_gdbSrvRspLock);
    m_interruptEvent.Close();
}
