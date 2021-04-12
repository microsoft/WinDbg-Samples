//----------------------------------------------------------------------------
//
// GdbSrvRspClient.h
//
// This class implements the RSP communication protocol used to communicate with the
// GdbServer stub. This class can be instantiated by using a TCPIP communication class
// or serial protocol class for connecting, sending and receiving data over the
// link layer (TCP/IP or RS-232).
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------
#pragma once

#include "stdafx.h"
#include <assert.h>
#include <stdlib.h>
#include <string>
#include <memory>
#include <vector>
#include "TextHelpers.h"
#include "HandleHelpers.h"
#include "TcpConnectorStream.h"

namespace GdbSrvControllerLib
{
    using namespace std;

    //  Convert a number to an ASCII hex digit.
    #define NumberToAciiHex(number)             ((number < 10) ? ('0' + number) : ('a' + number - 10))
    //  Convert to hex number from ASCII hex
    #define AciiHexAFToNumber(number)           ((number >= 'a' && number <= 'f') ? (number - 'a' + 10) : 0)
    #define AciiHexToNumber(number)             ((number >= '0' && number <= '9') ? (number - '0') : AciiHexAFToNumber(number))

    //  This is a random number that indicates the maximum attempts for sending packets. 
    //  The RSP protocol states that we should continue resending the packet if the
    //  '+' ACK packet does not arrive, but if we do this then we could lock the 
    //  entire dbgeng.dll thread, so we will have this random number as the floor limit 
    //  for resending packets. Also, the user could cancel the resending sequence 
    //  by interrupting the session (CTRL-BREAK).
    const int MAX_PACKETS_ATTEMPTS = 3;

    //  This constant indicates all cores operation
    const unsigned C_ALLCORES = 0xffffffff;


    //  Check if we reach the maximum allowed retry attempts
    #define IS_MAX_ATTEMPTS(retry)    (retry > MAX_PACKETS_ATTEMPTS) 
    #define IS_RETRY_ALLOWED(retry)   (retry < MAX_PACKETS_ATTEMPTS) 

    //  This is the list of supported query feature packets
    typedef enum
    {
        PACKET_VCONT = 0,
        PACKET_QSTART_NO_ACKMODE,
        PACKET_SIZE,
        PACKET_READ_TRACE32_SPECIAL_MEM,
        PACKET_WRITE_TRACE32_SPECIAL_MEM,
        MAX_FEATURES
    } RSP_FEATURES;

    //  Short Packet type response
    typedef enum
    {
        RSP_OK,
        RSP_ERROR,
        RSP_STOP_REPLY,
        RSP_IGNORE_REPLY
    } RSP_Response_Packet;
    
    //  Identifies an unexpected packet response for the type of the command
    #define IS_BAD_REPLY(ch)        ((ch == RSP_STOP_REPLY) || (ch == RSP_IGNORE_REPLY))

    //  This structure describes the query feature packet local cache.
    //  This cache is used by the client to enable/disable features
    //  supported by the DbgServer implementation.
    typedef struct
    {
        bool   isEnabled;
        int    featureDefaultValue;
        string name;
    } PacketConfig;

    //  This structured is used to configure the RSP client to work with the specific link layer
    typedef struct
    {
        //  Maximum connection attempts
        unsigned int connectAttempts;
        //  Send timeout (milliseconds)
        unsigned int sendTimeout;
        //  Receive timeout (milliseconds)
        unsigned int recvTimeout;
        //  Pointer to the function to do a call back for displaying
        //  send and receive comm. data.
        pSetDisplayCommData pDisplayCommDataFunc;
        //  Pointer to the Text processing object.
        //  It's used for displaying comm. data.
        IGdbSrvTextHandler * pTextHandler;
    } RSP_CONFIG_COMM_SESSION;


    //  This class implement the client RSP protocol used to communicate
    //  with the GdbServer
    template <class TConnectStream> class GdbSrvRspClient final
    {
	    public:        
	    //  Construct the RSP client object for the passed in LinkLayer type
        //  The connection string format depends on the linklayer type.
	    GdbSrvRspClient(_In_ const vector<wstring> &coreConnectionParameters);

        ~GdbSrvRspClient();
                
        //  Set the configuration options used by the link layer session
        bool ConfigRspSession(_In_ const RSP_CONFIG_COMM_SESSION * pConfigData, _In_ unsigned core);

        //  Connect to the remote GdbServer
        bool ConnectRsp();

        //  Connect to specific core
        bool ConnectRspToCore(_In_ const wstring &connectionStr, _In_ unsigned core);

        //  Attach to a core by creating a new channel and connect to it.
        bool AttachRspToCore(_In_ const wstring &connectionStr, _In_ unsigned core);

        //  Sends RSP client packet to the GdbServer
        bool SendRspPacket(_In_ const string & command, _In_ unsigned activeCore);

        //  Receives a RSP packet from the GdbServer
	    bool ReceiveRspPacket(_Out_ string & response, _In_ unsigned activeCore, _In_ bool isWaitForever)
	    {
            bool IsPollingChannelMode = false;
            return ReceiveRspPacketEx(response, activeCore, isWaitForever, IsPollingChannelMode, true);
	    }

        bool ReceiveRspPacketEx(_Out_ string & response, _In_ unsigned activeCore, _In_ bool isWaitForever, _Inout_ bool & IsPollingChannelMode,
                                _In_ bool fReset);

        //  Send an interrupt message (CTRL-C)
        bool SendRspInterrupt()
        {
            return SendRspInterruptEx(true, 0);
        }

        //  Get the RSP connection status
        bool GetRspSessionStatus(_Out_ HRESULT & error, _In_ unsigned core);

        //  Retrieves the last error from the link layer
        int GetRspLastError(); 

        //  Shutdown the RSP protocol
        bool ShutDownRsp();

        //  Close specific core RSP channel
        bool CloseRspCore(_In_ const wstring &closeStr, _In_ unsigned core);

        //  Display RSP linklayer errors
        void HandleRspErrors(_In_ GdbSrvTextType textType);

        //  Updates the RSP query packet storage. 
        bool UpdateRspPacketFeatures(_In_ const string & features);
        
        //  Get a copy of the current stored features.
        void GetRspPacketFeatures(_Out_ PacketConfig * pConfig, _In_ RSP_FEATURES index);

        //  Get the number of connected link layers (it's used for multi-core GdbServer connections)
        size_t GetNumberOfStreamConnections();

        //  Send the interrup to specific processor cores.
        bool SendRspInterruptToProcessorCores(_In_ bool fResetAllCores, _In_ unsigned activeCore)
        {
            return SendRspInterruptEx(fResetAllCores, activeCore);
        }

        //  Discard any pending response
        void DiscardResponse(_In_ unsigned activeCore);

        //  Check if the GDB Server feature is enabled
        bool IsFeatureEnabled(_In_ unsigned feature);

	    private:
        ValidHandleWrapper m_interruptEvent;
        unique_ptr <TConnectStream> m_pConnector;
        static PacketConfig s_RspProtocolFeatures[MAX_FEATURES];
        static RSP_CONFIG_COMM_SESSION s_LinkLayerConfigOptions;
        CRITICAL_SECTION m_gdbSrvRspLock;
        int WaitForRspPacketStart(_In_ int maxPacketLength, _In_ TcpIpStream * pStream, _In_ bool isRspWaitNeeded, 
                                  _Inout_ bool & IsPollingChannelMode, _In_ bool fResetBuffer);
        string CreateSendRspPacket(_In_ const string & command);
        void SetProtocolFeatureValue(_In_ size_t index, _In_ int value);
        void SetProtocolFeatureFlag(_In_ size_t index, _In_ bool value);
        bool GetNoAckModeRequired(_In_ const string & command);
        bool SendRspInterruptEx(_In_ bool fResetAllCores, _In_ unsigned activeCore);
    }; 
}