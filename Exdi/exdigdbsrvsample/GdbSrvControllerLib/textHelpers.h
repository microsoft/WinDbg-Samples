//----------------------------------------------------------------------------
//
// TextHelpers.h
//
// Wrapper classes for the system handles.
//
// Copyright (c) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#pragma once

namespace GdbSrvControllerLib
{
    enum class GdbSrvTextType
    {
        Command,
        CommandOutput,
        CommandError,
    };

    class IGdbSrvTextHandler
    {
    public:
        virtual void HandleText(_In_ GdbSrvTextType textType, _In_reads_bytes_(readSize) const char *pText,
                                _In_ size_t readSize) = 0;
        virtual ~IGdbSrvTextHandler(){}
    };
    //  This type is used to set the call back function for displaying send/recv data
    typedef void (*pSetDisplayCommData)(_In_reads_bytes_(readSize) const char * pData,  _In_ size_t readSize, _In_ GdbSrvTextType textType, IGdbSrvTextHandler * const pTextHandler, _In_ unsigned channel);
}