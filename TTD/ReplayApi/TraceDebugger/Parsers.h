// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// Parsers.h - A collection of utility functions and types for parsing and formatting data
//             in support of the mini debugger sample. There isn't anything particularly
//             noteworthy in here from a TTD perspective.

#pragma once

#include <assert.h>
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <limits>
#include <type_traits>
#include <algorithm>
#include <cctype>
#include <cerrno>

// Include TTD headers for DataAccessMask
#include <TTD/IReplayEngine.h>

inline std::wstring AsWString(std::string_view const str)
{
    // REVIEW: Open to different way.
    return std::wstring(str.begin(), str.end());
}

inline void SkipBlanks(std::string_view& line)
{
    auto const newPos = line.find_first_not_of(" \t");
    if (newPos == line.npos)
    {
        line = {};
    }
    else
    {
        line = line.substr(newPos);
    }
}

inline std::string_view ExtractFirstWord(std::string_view& line)
{
    SkipBlanks(line);
    std::string_view firstWord = line.substr(0, line.find_first_of(" \t"));
    line = line.substr(firstWord.size());
    SkipBlanks(line);
    return firstWord;
}

[[nodiscard]] inline std::optional<uint64_t> TryParseUnsigned64(
    _In_z_ wchar_t const* const pString,
    _In_   int            const radix
) noexcept
{
    errno = 0;

    wchar_t* pEnd = nullptr;
    uint64_t const result = wcstoull(pString, &pEnd, radix);

    if (errno != 0)
    {
        return std::nullopt;
    }

    if (*pEnd != '\0')
    {
        return std::nullopt;
    }

    return result;
}

template <typename TUnsigned>
[[nodiscard]] inline std::optional<TUnsigned> TryParseUnsigned(
    _In_z_ wchar_t const* const pString,
    int            const radix = 0
) noexcept
{
    static_assert(std::is_unsigned_v<TUnsigned>);

    auto const result = TryParseUnsigned64(pString, radix);

    if (!result || result > std::numeric_limits<TUnsigned>::max())
    {
        return std::nullopt;
    }

    return static_cast<TUnsigned>(*result);
}

template <typename TUnsigned>
[[nodiscard]] inline std::optional<TUnsigned> TryParseUnsigned(
    std::string_view const str,
    int            const radix = 0
) noexcept
{
    std::wstring tempStr(str.begin(), str.end());
    return TryParseUnsigned<TUnsigned>(tempStr.c_str(), radix);
}

template < size_t MaxBytes >
inline std::wstring GetDataString(std::span<uint8_t const> const input)
{
    size_t const dataSizeInBytes = std::min(input.size(), MaxBytes);
    bool const tooBig = dataSizeInBytes < input.size();

    wchar_t buffer[MaxBytes * 3 + 3 + 1];
    switch (dataSizeInBytes)
    {
    case 1: swprintf_s(buffer, L"0x%02X", *reinterpret_cast<uint8_t const*>(input.data())); return buffer;
    case 2: swprintf_s(buffer, L"0x%04X", *reinterpret_cast<uint16_t const*>(input.data())); return buffer;
    case 4: swprintf_s(buffer, L"0x%08X", *reinterpret_cast<uint32_t const*>(input.data())); return buffer;
    case 8: swprintf_s(buffer, L"0x%016llX", *reinterpret_cast<uint64_t const*>(input.data())); return buffer;
    }

    for (size_t i = 0; i < dataSizeInBytes; ++i)
    {
        swprintf_s(&buffer[i * 3], 4, L"%02X ", static_cast<uint8_t const*>(input.data())[i]);
    }
    if (tooBig)
    {
        DBG_ASSERT(dataSizeInBytes * 3 + 3 <= _countof(buffer));
        buffer[dataSizeInBytes * 3 - 1 + 0] = L'.';
        buffer[dataSizeInBytes * 3 - 1 + 1] = L'.';
        buffer[dataSizeInBytes * 3 - 1 + 2] = L'.';
        return { buffer, dataSizeInBytes * 3 - 1 + 3 };
    }
    return { buffer, dataSizeInBytes * 3 };
}

inline std::optional<TTD::Replay::DataAccessMask> ParseAccessMask(std::string_view const maskString)
{
    TTD::Replay::DataAccessMask accessMask = TTD::Replay::DataAccessMask::None;
    for (auto ch : maskString)
    {
        switch (std::toupper(ch))
        {
        case 'R':
            accessMask |= TTD::Replay::DataAccessMask::Read;
            break;
        case 'O':
            accessMask |= TTD::Replay::DataAccessMask::Overwrite;
            break;
        case 'W':
            accessMask |= TTD::Replay::DataAccessMask::Write;
            break;
        case 'E':
            accessMask |= TTD::Replay::DataAccessMask::Execute;
            break;
        case 'C':
            accessMask |= TTD::Replay::DataAccessMask::CodeFetch;
            break;
        case 'M':
            accessMask |= TTD::Replay::DataAccessMask::DataMismatch;
            break;
        case 'N':
            accessMask |= TTD::Replay::DataAccessMask::NewData;
            break;
        case 'D':
            accessMask |= TTD::Replay::DataAccessMask::RedundantData;
            break;
        default:
            return {};
        }
    }
    return accessMask;
}
