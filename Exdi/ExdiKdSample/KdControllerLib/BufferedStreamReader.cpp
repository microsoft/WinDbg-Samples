//----------------------------------------------------------------------------
//
// BufferedStreamReader.cpp
//
// A class used to read a stream line-by-line with an arbitrary delimiter.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "BufferedStreamReader.h"
#include "ExceptionHelpers.h"
#include <algorithm>

using namespace KDControllerLib;

BufferedStreamReader::BufferedStreamReader(_In_ HANDLE stream)
    : m_stream(stream)
    , m_frontGapSize(0)
{
    assert(stream != INVALID_HANDLE_VALUE);
}

BufferedStreamReader::~BufferedStreamReader()
{
}

std::string BufferedStreamReader::Read(_In_ const std::regex &delimiter, _Out_opt_ MatchCollection *pRegexMatches)
{
    std::string result;

    for (;;)
    {
        if (TryReadBufferedData(&result,  delimiter, pRegexMatches))
        {
            return result;
        }
     
        RemoveFrontGapInBuffer();

        if (!m_internalBuffer.TryEnsureCapacity(m_internalBuffer.GetLength() + c_readChunkSize))
        {
            throw _com_error(E_OUTOFMEMORY);
        }

        size_t availableSize = m_internalBuffer.GetCapacity() - m_internalBuffer.GetLength();
        assert(availableSize >= c_readChunkSize);
        
        DWORD bytesRead;

        if (!ReadFile(m_stream, 
            m_internalBuffer.GetInternalBuffer() + m_internalBuffer.GetLength(),
            (availableSize > MAXDWORD) ? MAXDWORD : static_cast<DWORD>(availableSize),
            &bytesRead,
            nullptr))
        {
            throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
        }

        if (bytesRead == 0)
        {
            throw _com_error(E_FAIL);
        }

        m_internalBuffer.SetLength(m_internalBuffer.GetLength() + bytesRead);
    }
}

void BufferedStreamReader::RemoveFrontGapInBuffer()
{
    if (m_frontGapSize != 0)
    {
        assert(m_frontGapSize <= m_internalBuffer.GetLength());

        memmove(m_internalBuffer.GetInternalBuffer(), 
                m_internalBuffer.GetInternalBuffer() + m_frontGapSize, 
                m_internalBuffer.GetLength() - m_frontGapSize);

        m_internalBuffer.SetLength(m_internalBuffer.GetLength() - m_frontGapSize);
        m_frontGapSize = 0;
    }
}


bool BufferedStreamReader::TryReadBufferedData(_Inout_ std::string *pBuffer, 
                                               _In_ const std::regex &delimiter,
                                               _Out_opt_ MatchCollection *pRegexMatches)
{
    assert(pBuffer != nullptr);
    assert(m_frontGapSize <= m_internalBuffer.GetLength());
    pBuffer->clear();

    if (m_internalBuffer.GetLength() == m_frontGapSize)
    {
        return false;
    }

    char *pEndOfSearchedBuffer = m_internalBuffer.GetEndOfData();
    std::cmatch matches;

    if (!std::regex_search<const char *>(m_internalBuffer.GetInternalBuffer() + m_frontGapSize,
                                         pEndOfSearchedBuffer,
                                         matches,
                                         delimiter))
    {
        return false;
    }

    if (matches.empty())
    {
        assert(!"Unexpected zero-length matches returned by regex_search()");
        return false;
    }

    size_t delimiterOffset = matches[0].first - m_internalBuffer.GetInternalBuffer();
    assert(delimiterOffset < m_internalBuffer.GetLength());

    pBuffer->assign(m_internalBuffer.GetInternalBuffer() + m_frontGapSize, delimiterOffset - m_frontGapSize);

    m_frontGapSize = delimiterOffset + matches[0].length();
    assert(m_frontGapSize <= m_internalBuffer.GetLength());

    if (pRegexMatches != nullptr)
    {
        pRegexMatches->clear();
        for (size_t i = 1; i < matches.size(); i++)
        {
            pRegexMatches->push_back(matches[i].str());
        }
    }

    return true;
}
