//----------------------------------------------------------------------------
//
// BufferedStreamReader.h
//
// A class used to read a stream line-by-line with an arbitrary delimiter.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <regex>
#include "BufferWrapper.h"

namespace KDControllerLib
{
    //This class is used to read a given stream (represented by a HANDLE) on a line-by-line basis, where
    //the 'line delimiter' is an arbitrary-length string.
    //It is used by KDController to read the entire response of kd.exe up until the '\r\nkd> ' sequence.
    //NOTE: The class does not own the handle.
    //NOTE: This class is optimized for simplicity, not performance. Do not use it in performance-critical code.
    class BufferedStreamReader final
    {
    public:
        typedef std::vector<std::string> MatchCollection;

        BufferedStreamReader(_In_ HANDLE stream);
        ~BufferedStreamReader();

        std::string Read(_In_ const std::regex &delimiter, _Out_opt_ MatchCollection *pRegexMatchesExcept0 = nullptr);

    private:
        static size_t const c_readChunkSize = 65536;

        HANDLE m_stream;
    
        //Contains the data already read from the stream but not returned to client yet.
        SimpleCharBuffer m_internalBuffer;
        size_t m_frontGapSize;

        //Returns false when no buffered data is available and a normal read should be performed
        bool TryReadBufferedData(_Inout_ std::string *pBuffer, 
                                 _In_ const std::regex &delimiter,
                                 _Out_opt_ MatchCollection *pRegexMatchesExcept0); 

        void RemoveFrontGapInBuffer();
    };
}