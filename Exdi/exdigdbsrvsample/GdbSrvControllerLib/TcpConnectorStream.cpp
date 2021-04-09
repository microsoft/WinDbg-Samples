//----------------------------------------------------------------------------
//
// TcpConnectorStream.cpp
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------
#pragma once
#include "stdafx.h"
#include <algorithm>
#include <iterator>
#include "TcpConnectorStream.h"

namespace GdbSrvControllerLib
{
    using namespace std;

    unique_ptr<TcpIpStream> TcpConnectorStream::TcpInitialize(_In_ const wstring &connectionStr, _In_ unsigned channel)
    {
        USHORT portNumber;
        CHAR hostName[MAX_PATH + 1];
        if (ParseConnectString(static_cast<LPCTSTR>(connectionStr.c_str()), hostName, ARRAYSIZE(hostName), &portNumber))
        {
            struct sockaddr_in address;
            address.sin_family = AF_INET;
            address.sin_port = htons(portNumber);
            if (ResolveHostName(hostName, &address.sin_addr) != 0) 
            {
#pragma warning(suppress: 26000) // We decided using the ANSI version of the socket library in this sample code.
                inet_pton(PF_INET, hostName, &address.sin_addr);
            } 
            //  Create a IPv4 connection oriented socket.
            SOCKET sd = socket(AF_INET, SOCK_STREAM, 0);
            if (sd == INVALID_SOCKET)
            {
                if (WSAGetLastError() == WSAEAFNOSUPPORT)
                {
                    return nullptr;
                }
            }
            return unique_ptr<TcpIpStream>(new (nothrow) TcpIpStream(sd, &address, channel)); 
        }
        return nullptr;
    }

    bool TcpConnectorStream::TcpClose()
    {
        bool closeDone = true;

        for (const auto& pTcpStream : m_pTLinkLayerStreamClass)
        {
            assert(pTcpStream != nullptr);

            if (!TcpCloseStream(pTcpStream.get()))
            {
                closeDone = false;
                break;
            }
        }
        return closeDone;
    }

    bool TcpConnectorStream::TcpCloseStream(_In_ TcpIpStream * const pTcpStream)
    {
        assert(pTcpStream != nullptr);

        return pTcpStream->Close();
    }

    bool TcpConnectorStream::TcpConnect(_In_ unsigned int maxAttempts)
    {
        bool connectDone = false;

        for (const auto& pTcpStream : m_pTLinkLayerStreamClass)
        {
            assert(pTcpStream != nullptr);
            
            connectDone = TcpConnectStream(pTcpStream.get(), maxAttempts);
            if (!connectDone)
            {
                break;
            }
        }
        return connectDone;
    }

    bool TcpConnectorStream::TcpConnectStream(_In_ TcpIpStream * const pTcpStream, _In_ unsigned int maxAttempts)
    {
        assert(pTcpStream != nullptr);

        bool connectDone = false;
        unsigned int retries = 0;
        for (;;)
        {
            if (pTcpStream->Connect())
            {
                connectDone = true;
                break;
            }

            if (++retries > maxAttempts)
            {
                Close();
                break;
            }

            Sleep(100);
        }
        return connectDone;
    }

    int TcpConnectorStream::ResolveHostName(_In_z_ const char * pHostname, _Inout_ struct in_addr * pAddr) 
    {
        assert(pHostname != nullptr && pAddr != nullptr);
        struct addrinfo * pResult;
     
#pragma warning(suppress: 38026) // We decided using the ANSI version of the socket library in this sample code.
        int result = getaddrinfo(pHostname, nullptr, nullptr, &pResult);
        if (result == 0) 
        {
            //  Iterate over the returned addrinfo struct for finding our entry
        	for (addrinfo * pAddrInfo = pResult; pAddrInfo != nullptr; pAddrInfo = pAddrInfo->ai_next) 
            {
                if (pAddrInfo->ai_family == AF_INET)
                {
                    struct sockaddr_in * pSockaddrIpv4 = reinterpret_cast<struct sockaddr_in *>(pAddrInfo->ai_addr);
                    memcpy(pAddr, &pSockaddrIpv4->sin_addr, sizeof(struct in_addr));
                }
            }
            freeaddrinfo(pResult);
        }
        return result;
    }

    //  The connect string is expected in the format <hostName>:<TcP Port Number>
    bool TcpConnectorStream::ParseConnectString(_In_ LPCTSTR pConnect, _Out_writes_(hostNameLength) PSTR pHostName, 
                                                _In_ ULONG hostNameLength, _Out_ USHORT * pPortNumber) 
    {
        assert(pConnect != nullptr && pHostName != nullptr && pPortNumber != nullptr);
        bool isSet = false;

        //  Parse the passed in connect string
        wstring connectString = pConnect;
        wstring::size_type idx = connectString.find(L':');
        if (idx != string::npos)
        {
            wstring hostName = connectString.substr(0, idx);
            wstring portNumber = connectString.substr(idx + 1);
            if (!hostName.empty() && !portNumber.empty())
            {
                
                if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, hostName.c_str(), -1, pHostName, hostNameLength, nullptr, nullptr) != 0)
                {
                    *pPortNumber = static_cast<USHORT>(stoi(portNumber));
                    isSet = true;
                }
            }
        }
        return isSet;
    }

    TcpIpStream::TcpIpStream(_In_ SOCKET sd, _In_ struct sockaddr_in * pAddress, _In_ unsigned channel) : m_socket(sd),
                                                                                                          m_pDisplayFunction(nullptr),
                                                                                                          m_pTextHandler(nullptr),
                                                                                                          m_channel(channel)

    {
        assert(pAddress != nullptr);

        char ip[128 + 1] = {0};
        inet_ntop(PF_INET, reinterpret_cast<struct in_addr *>(&pAddress->sin_addr.s_addr), ip, sizeof(ip) - 1);
        m_peerIP = ip;
        m_peerPort = ntohs(pAddress->sin_port);
        m_address = *pAddress;
    }
}