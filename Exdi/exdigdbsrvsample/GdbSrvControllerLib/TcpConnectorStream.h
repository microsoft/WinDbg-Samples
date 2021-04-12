//----------------------------------------------------------------------------
//
//  TcpConnectorStream.h
//
//  This header file implements two clases:
//  1.  The TcpConnectorStream class encapsulates the socket mechanisms to 
//      actively connect to a server. This class will produce TcpConnectorStream 
//      objects when a client wants to establish a socket connection with a server.
//      Also, it encapsulates the WinSock initialization and cleanup mechanism.
//
//  2.  The class TcpIpStream provides TCP/IP network I/O mechanisms.
//      Basically, this class provides methods to send and receive data over 
//      a TCP/IP connection as well as methods to configure the socket connection.
//      Also, It stores the connected socket descriptor and information about the 
//      server peer (the IP address and TCP port number).
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------
#pragma once
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <string>
#include <vector>
#include <memory>
#include "ExceptionHelpers.h"
#include "TextHelpers.h"

#pragma comment(lib, "Ws2_32.lib")

namespace GdbSrvControllerLib
{
    //  Verifies if the error identifies a connection lost socket event.
    #define IS_CONNECTION_LOST(error)   ((error == WSAENETDOWN) || (error == WSAENOTCONN) || (error == WSAENETRESET) || \
                                         (error == WSAESHUTDOWN) || (error == WSAECONNABORTED) || (error == WSAETIMEDOUT) || \
                                         (error == WSAECONNRESET))

    //  The TcpIpStream class provides basic methods to configure, send, and receive data over a TCP/IP socket connection. 
    //  Each connection is completely encapsulated in each TcpIpStream object (the socket descriptor is kept private).
    //  The majority of the class methods are wrappers around the Bekerly Socket library functions.
    //  The class stores a pointer to the TextHandler object (it does not own this object) that allows tracing 
    //  all communication data sent and received over the socket connection.
    class TcpIpStream final
    {
       public:
        friend class TcpConnectorStream;

        ~TcpIpStream()
        {
            Close();
        }
     
        int Send(_In_ LPCSTR pBuffer, _In_ int length)
        {
            assert(pBuffer != nullptr);

            CallDisplayFunction(pBuffer, length, GdbSrvTextType::Command);
            
            int bytesDone = 0;
            PCHAR pTempBuffer = const_cast<PCHAR>(pBuffer);

            while (length > 0)
            {
                int sentBytes = send(m_socket, pTempBuffer, length, 0);
                if (sentBytes == SOCKET_ERROR || sentBytes == 0)
                {
                    break;
                }
                pTempBuffer += sentBytes;
                length -= sentBytes;
                bytesDone += sentBytes;
            }
            return bytesDone; 
        }

        int Receive(_Out_writes_bytes_(length) PCHAR pBuffer, _In_ int length)
        {
            assert(pBuffer != nullptr);

            int status = recv(m_socket, pBuffer, length, 0);

            if (status > 0)
            {
                CallDisplayFunction(pBuffer, status, GdbSrvTextType::CommandOutput);
            }

            return status;
        }

        int Peek(_Out_writes_bytes_(length) PCHAR pBuffer, _In_ int length, _In_ int flags) const
        {
            assert(pBuffer != nullptr);
            return recv(m_socket, pBuffer, length, flags);
        }

        int SetOptions(_In_ int level, _In_ int optionName, _In_reads_opt_(optionLength) const char * pOptionVal,
                       _In_ int optionLength) const
        {
            return setsockopt(m_socket, level, optionName, pOptionVal, optionLength); 
        }

        int GetOptions(_In_ int level, _In_ int optionName, _Out_writes_(*pOptionLength)char * pOptionVal,
                       _Inout_ int * pOptionLength) const
        {
            return getsockopt(m_socket, level, optionName, pOptionVal, pOptionLength); 
        }

        bool Connect()
        {
            bool connectDone = false;
            
            if (::connect(m_socket, reinterpret_cast<struct sockaddr *>(&m_address), sizeof(m_address)) != SOCKET_ERROR)
            {
                connectDone = true;
            }
            return connectDone;
        }

        bool Close() const
        {
            int result = closesocket(m_socket);    
            if (result == SOCKET_ERROR)
            {
                return false;
            }
            return true;
        }
        
        int Select(_Inout_opt_ fd_set * pReadfds, _Inout_opt_  fd_set * pWritefds,
                   _Inout_opt_  fd_set * pExceptfds, _In_opt_ const struct timeval * pTimeout) const
        {
            if (pReadfds != nullptr)
            {            
                FD_ZERO(pReadfds);
                FD_SET(m_socket, pReadfds);
            }
            if (pWritefds != nullptr)
            {            
                FD_ZERO(pWritefds);
                FD_SET(m_socket, pWritefds);
            }
            if (pExceptfds != nullptr)
            {            
                FD_ZERO(pExceptfds);
                FD_SET(m_socket, pExceptfds);
            }

            return select(0, pReadfds, pWritefds, pExceptfds, pTimeout);
        }

        inline int IsFDSet(_In_ fd_set * pFds) const
        {
            assert(pFds != nullptr);

            return FD_ISSET(m_socket, pFds);
        }

        inline int Ioctlsocket(_In_ long cmd, _Inout_ u_long * pArg) const
        {
            assert(pArg != nullptr);

            return ioctlsocket(m_socket, cmd, pArg);
        }

        inline void SetCallBackDisplayFunc(_In_ const pSetDisplayCommData function,
                                    _In_ IGdbSrvTextHandler * const pTextHandler)
        {
            m_pDisplayFunction = function;
            m_pTextHandler = pTextHandler;
        }

        inline void CallDisplayFunction(_In_ LPCSTR pBuffer, _In_ size_t len, _In_ GdbSrvTextType textType)
        {
            if (pBuffer != nullptr && m_pDisplayFunction != nullptr && m_pTextHandler != nullptr)
            {
                m_pDisplayFunction(pBuffer, len, textType, m_pTextHandler, m_channel);
            }
        }

        inline void CallDisplayFunction(_In_ LPCSTR pBuffer, _In_ GdbSrvTextType textType)
        {
            CallDisplayFunction(pBuffer, strlen(pBuffer), textType);
        }
     
        std::string getPeerIP() const {return m_peerIP;}
        USHORT getPeerPort() const {return m_peerPort;}
     
      private:
	    SOCKET               m_socket;
	    pSetDisplayCommData  m_pDisplayFunction;
        IGdbSrvTextHandler * m_pTextHandler;
	    std::string          m_peerIP;
	    USHORT               m_peerPort;
        struct sockaddr_in   m_address;
        unsigned             m_channel;

        TcpIpStream(_In_ SOCKET sd, _In_ struct sockaddr_in * pAddress, _In_ unsigned channel);
    };

    //  The TcpConnectorStream class provides the connection mechanism to actively establish a connection with a server. 
    //  Basically, it's a factory class for producing connector objects when a client wants to connect to the server.
    //  The sending data over the network behaviour is implemented in the TcpIpStream class.
    //  Also, it initializes the WinSock library by the process.
    class TcpConnectorStream final
    {
      public:
        TcpConnectorStream(_In_ const std::vector<std::wstring> &coreConnectionParameters) : m_isInitiated(false),
                                                                                             m_isConnected(false)
        {
            WSADATA wsaData = {0};
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0)
            {
                unsigned channel = 0;
                for (auto const& connectionStr: coreConnectionParameters)
                {
                    m_pTLinkLayerStreamClass.push_back(TcpInitialize(connectionStr, channel));
                    channel++;
                }
                if (m_pTLinkLayerStreamClass.size() == coreConnectionParameters.size())
                {
                    m_isInitiated = true;
                }
            } 
        }
        
        ~TcpConnectorStream()
        {
            Close();
            if (m_isInitiated)
            {
                WSACleanup();
            }
        }
        
        inline bool TcpOpenStreamCore(_In_ const std::wstring &connectionStr, _In_ unsigned core)
        {
            try
            {
                if (m_pTLinkLayerStreamClass[core] == nullptr)
                {
                    return false;
                }
                m_pTLinkLayerStreamClass[core] = TcpInitialize(connectionStr, core);
                return true;
            }
            CATCH_AND_RETURN_BOOLEAN
        }

        inline bool TcpConnectCore(_In_ unsigned maxAttempts, _In_ unsigned core)
        {
            return TcpConnectStream(m_pTLinkLayerStreamClass[core].get(), maxAttempts);
        }

        inline bool TcpCloseCore(_In_ unsigned core)
        {
            return TcpCloseStream(m_pTLinkLayerStreamClass[core].get());
        }


        bool Connect(unsigned int retries)
        {
            m_isConnected = TcpConnect(retries);
            return m_isConnected;
        }
        
        bool Close()
        {
            return TcpClose();
        }

        TcpIpStream * GetLinkLayerStream() const {return m_pTLinkLayerStreamClass[0].get();}
        TcpIpStream * GetLinkLayerStreamEntry(size_t coreNumber) const 
        {
            if (m_pTLinkLayerStreamClass.size() > 1)
            {
                return m_pTLinkLayerStreamClass[coreNumber].get();
            }
            return GetLinkLayerStream();
        }
        int GetLastError() const {return WSAGetLastError();}
        bool IsConnected() const {return m_isConnected;}
        bool IsConnectionLost(int error) const {return IS_CONNECTION_LOST(error);}
        size_t GetNumberOfConnections() const {return m_pTLinkLayerStreamClass.size();}
        
      private:
        std::vector<std::unique_ptr<TcpIpStream>> m_pTLinkLayerStreamClass;
        bool m_isInitiated;
        bool m_isConnected;

        std::unique_ptr<TcpIpStream> TcpInitialize(_In_ const std::wstring &connectionStr, _In_ unsigned channel);
        bool TcpConnect(_In_ unsigned int retries);
        bool TcpClose();
        bool ParseConnectString(_In_ LPCTSTR pConnect, _Out_writes_(hostNameLength) PSTR pHostName, _In_ ULONG hostNameLength, 
                                _Out_ USHORT * pPortNumber); 
        int ResolveHostName(_In_z_ const char * pHostname, _Inout_ struct in_addr * pAddr);
        bool TcpConnectStream(_In_ TcpIpStream * const pTcpStream, _In_ unsigned int maxAttempts);
        bool TcpCloseStream(_In_ TcpIpStream * const pTcpStream);
    };
}