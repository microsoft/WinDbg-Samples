//**************************************************************************
//
// FileParser.cpp
//
// A simple parser for the "text dump" files that we want the debugger to
// be able to open.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "TextDump.h"

using namespace Microsoft::WRL;

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

HRESULT TextDumpParser::Initialize()
{
    HRESULT hr = S_OK;

    //
    // We are going to ask for a memory mapping of the file.  For a file opened by the debugger regularly on
    // the file system, we will be able to get this.  There are instances where we *MAY NOT* be able to get
    // a file mapping.  This sample only deals with the memory mapping.  A more general plug-in might wish to
    // handle both.
    //
    // NOTE: all interfaces having to do with being the "debug source" (the thing you are debugging) are
    //       prefixed ISvcDebugSource...  The "ISvcDebugSourceFileMapping" interface should be read as
    //       'ISvc' 'DebugSource' 'FileMapping' and not be confused with a code source file (e.g.: some C/C++ source 
    //       code).  The interfaces having to do with code source files are ISvcSourceFile...
    //
    ComPtr<ISvcDebugSourceFileMapping> spFileMapping;
    IfFailedReturn(m_spFile.As(&spFileMapping));

    ULONG64 mappingSize;
    IfFailedReturn(spFileMapping->MapFile(reinterpret_cast<void **>(&m_pFileMapping), &mappingSize));

    // It would have failed to map if > MAX(size_t)
    m_mappingSize = static_cast<size_t>(mappingSize);

    //
    // As a simple sample, we'll handle UTF-8 and UTF-16LE files.  If there is no BOM, assume the file
    // is UTF-8.
    //
    if (m_mappingSize >= 3 && m_pFileMapping[0] == 0xef && m_pFileMapping[1] == 0xbb && m_pFileMapping[2] == 0xbf)
    {
        m_isUtf8 = true;
        m_pos = 3;
    }
    else if (m_mappingSize >= 2 && m_pFileMapping[0] == 0xff && m_pFileMapping[1] == 0xfe)
    {
        m_isUtf8 = false;
        m_pos = 2;
    }
    else
    {
        //
        // There are other "formats" -- we aren't handling them for the purposes of this sample.  If we didn't
        // recognize the BOM -- just start parsing UTF-8.  We'll fail to recognize our "header" in the file.
        //
        m_isUtf8 = true;
        m_pos = 0;
    }

    //
    // Check the header to make sure that the file format is what we "recognize"
    //
    std::wstring line;
    if (!ReadLine(&line))
    {
        return E_FAIL;
    }

    if (wcscmp(line.c_str(), L"*** TEXTUAL DEMONSTRATION FILE") == 0)
    {
        return S_OK;
    }

    // It's not OUR file format.
    return E_FAIL;
}

bool TextDumpParser::ReadLine(_Out_ std::wstring *pString)
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;
        *pString = std::wstring();

        if (m_pos >= m_mappingSize)
        {
            return E_BOUNDS;
        }

        size_t posEol;

        //
        // Scan for the next new line or EOF
        //
        if (m_isUtf8)
        {
            for (posEol = m_pos; posEol < m_mappingSize; ++posEol)
            {
                if (m_pFileMapping[posEol] == '\n' ||
                    (posEol + 1 < m_mappingSize && m_pFileMapping[posEol] == '\r' &&  
                                                   m_pFileMapping[posEol + 1] == '\n'))
                {
                    break;
                }
            }

            int sz = MultiByteToWideChar(CP_UTF8,
                                         MB_PRECOMPOSED,
                                         m_pFileMapping + m_pos,
                                         static_cast<int>(posEol - m_pos),
                                         nullptr,
                                         0);

            pString->resize(sz);

            int rsz = MultiByteToWideChar(CP_UTF8,
                                          MB_PRECOMPOSED,
                                          m_pFileMapping + m_pos,
                                          static_cast<int>(posEol - m_pos),
                                          const_cast<wchar_t *>(pString->data()),
                                          sz);

            if (sz != rsz)
            {
                return E_FAIL;
            }

            size_t remaining = m_mappingSize - posEol;
            if (remaining >= 2 && m_pFileMapping[posEol] == '\r' && m_pFileMapping[posEol + 1] == '\n')
            {
                posEol += 2;
            }
            else if (remaining >= 1 && m_pFileMapping[posEol] == '\n')
            {
                posEol += 1;
            }

            m_pos = posEol;
        }
        else
        {
            wchar_t const *pEol = reinterpret_cast<wchar_t const *>(m_pFileMapping + m_pos);
            for (posEol = m_pos; posEol < m_mappingSize; posEol += 2)
            {
                pEol = reinterpret_cast<wchar_t const *>(m_pFileMapping + posEol);
                if (*pEol == '\n' ||
                    (posEol + 2 < m_mappingSize && *pEol == L'\r' && *(pEol + 1) == L'\n'))
                {
                    break;
                }
            }

            *pString = std::wstring(reinterpret_cast<wchar_t const *>(m_pFileMapping + m_pos), posEol - m_pos);

            size_t remaining = m_mappingSize - posEol;
            if (remaining >= 4 && *pEol == L'\r' && *(pEol + 1) == L'\n')
            {
                posEol += 4;
            }
            else if (remaining >= 2 && *pEol == L'\n')
            {
                posEol += 2;
            }

            m_pos = posEol;
        }

        return hr;
    };

    return SUCCEEDED(ConvertException(fn));
}

bool TextDumpParser::ParseHex(_In_ wchar_t const *ps,
                              _Out_ ULONG64 *pValue,
                              _Out_ wchar_t const **ppc)
{
    ULONG64 val = 0;
    wchar_t const *pc = ps;
    size_t sepCount = 0;
    for(;;)
    {
        if (*pc >= L'0' && *pc <= '9')
        {
            val = (val << 4) | (*pc - L'0');
        }
        else if (*pc >= L'a' && *pc <= 'f')
        {
            val = (val << 4) | (*pc - L'a' + 10);
        }
        else if (*pc >= L'A' && *pc <= 'F')
        {
            val = (val << 4) | (*pc - L'A' + 10);
        }
        else if (*pc == '`')
        {
            ++sepCount;
        }
        else 
        {
            if (static_cast<size_t>(pc - ps) == sepCount)
            {
                return false;
            }

            *pValue = val;
            *ppc = pc;
            return true;
        }

        ++pc;
    }
}

bool TextDumpParser::IsEmptyLine(_In_ std::wstring const& str)
{
    if (str.empty()) { return true; }
    wchar_t const *pc = str.c_str();
    while (*pc && iswspace(*pc)) { ++pc; }
    return *pc == 0;
}

HRESULT TextDumpParser::ParseRegisters()
{
    HRESULT hr = S_OK;

    std::wstring line;

    //
    // The section ends with a blank line.
    //
    while(ReadLine(&line) && !IsEmptyLine(line))
    {
        wchar_t const *pc = line.c_str();

        // Example lines:
        //
        // rax=00000000000014c3 rbx=0000000000000001 rcx=0000000000000001
        // iopl=0         nv up ei pl zr na po nc
        //

        //
        // There can be multiple registers defined on a single line.
        //
        while(*pc)
        {
            wchar_t const *pn = pc;
            while (*pn && !iswspace(*pn) && *pn != L'=') { ++pn; }

            if (*pn != '=')
            {
                //
                // Skip...
                //
                pc = pn;
                while (*pc && iswspace(*pc)) { ++pc; }
                continue;
            }

            std::wstring registerName(pc, pn - pc);
            pc = pn + 1;

            ULONG64 registerValue;
            if (!ParseHex(pc, &registerValue, &pc))
            {
                return E_FAIL;
            }

            m_registerValues.push_back({ std::move(registerName), registerValue });
            
            while (*pc && iswspace(*pc)) { ++pc; }
        }
    }

    if (m_registerValues.size() == 0)
    {
        return E_FAIL;
    }

    return hr;
}

HRESULT TextDumpParser::ParseModuleInformation()
{
    HRESULT hr = S_OK;

    std::wstring line;

    //
    // The section ends with a blank line.
    //
    while(ReadLine(&line) && !IsEmptyLine(line))
    {
        wchar_t const *pc = line.c_str();

        // Example line:
        // 
        //     00007ff7`79e10000 00007ff7`79e67000 notepad "C:\Windows\System32\notepad.exe" F59533D5 00057000
        //

        ULONG64 startAddress;
        if (!ParseHex(pc, &startAddress, &pc))
        {
            return E_FAIL;
        }

        while (*pc && iswspace(*pc)) { ++pc; }

        ULONG64 endAddress;
        if (!ParseHex(pc, &endAddress, &pc))
        {
            return E_FAIL;
        }

        while (*pc && iswspace(*pc)) { ++pc; }

        wchar_t const *pn = pc;
        while (*pn && !iswspace(*pn)) { ++pn; }

        if (pn == pc)
        {
            return E_FAIL;
        }

        std::wstring moduleName(pc, pn - pc);

        pc = pn + 1;

        while (*pc && iswspace(*pc)) { ++pc; }

        if (*pc != L'"')
        {
            return E_FAIL;
        }
        pc++;
        pn = pc;

        while (*pn && *pn != L'"') { ++pn; }

        std::wstring imagePath(pc, pn - pc);

        if (*pn != L'"')
        {
            return E_FAIL;
        }
        pc = pn + 1;

        while (*pc && iswspace(*pc)) { ++pc; }

        ULONG64 timeStamp;
        if (!ParseHex(pc, &timeStamp, &pc)) 
        {
            return E_FAIL;
        }

        while (*pc && iswspace(*pc)) { ++pc; }

        ULONG64 imageSize;
        if (!ParseHex(pc, &imageSize, &pc))
        {
            return E_FAIL;
        }

        m_moduleInfos.push_back({ startAddress, endAddress, std::move(moduleName), std::move(imagePath),
                                  timeStamp, imageSize });
    }

    if (m_moduleInfos.size() == 0)
    {
        return E_FAIL;
    }

    return hr;
}

HRESULT TextDumpParser::ParseStackFrames()
{
    HRESULT hr = S_OK;

    ULONG64 curFrame = 0;
    std::wstring line;

    //
    // The section ends with a blank line
    //
    while(ReadLine(&line) && !IsEmptyLine(line))
    {
        wchar_t const *pc = line.c_str();

        // Example line:
        //
        //   00 00000072`a512ee18 00007fff`3322d1ee     win32u!NtUserMsgWaitForMultipleObjectsEx+0x14
        //

        ULONG64 frameNumber;
        if (!ParseHex(pc, &frameNumber, &pc) || frameNumber != curFrame)
        {
            return E_FAIL;
        }

        while (*pc && iswspace(*pc)) { ++pc; }

        ULONG64 childSp;
        ULONG64 retAddr;

        if (!ParseHex(pc, &childSp, &pc))
        {
            return E_FAIL;
        }

        while (*pc && iswspace(*pc)) { ++pc; }

        if (!ParseHex(pc, &retAddr, &pc))
        {
            return E_FAIL;
        }

        while (*pc && iswspace(*pc)) { ++pc; }

        wchar_t const *pn = pc;
        while (*pn && *pn != L'!') { ++pn; }

        if (*pn != L'!')
        {
            return E_FAIL;
        }

        std::wstring moduleName(pc, pn - pc);
        pc = pn + 1;
        wchar_t const *pdisp = nullptr;

        while (*pn)
        {
            if (*pn == L'+' && *(pn + 1) == L'0' && *(pn + 2) == L'x')
            {
                pdisp = pn;
                break;
            }
            ++pn;
        }

        std::wstring symbolName(pc, pn - pc);

        ULONG64 displacement = 0;
        if (pdisp != nullptr)
        {
            pc = pn + 3;
            if (!ParseHex(pc, &displacement, &pc))
            {
                return E_FAIL;
            }
        }

        m_stackFrames.push_back({ frameNumber, childSp, retAddr, 
                                  std::move(moduleName), std::move(symbolName), displacement });

        ++curFrame;
    }

    if (m_stackFrames.size() == 0)
    {
        return E_FAIL;
    }

    return hr;
}

HRESULT TextDumpParser::ParseMemoryRegions()
{
    HRESULT hr = S_OK;

    ULONG64 startAddress = 0;
    ULONG64 curAddress = 0;
    std::vector<unsigned char> data;

    bool firstLine = true;
    std::wstring line;

    //
    // The section ends with a blank line.
    //
    while(ReadLine(&line) && !IsEmptyLine(line))
    {
        wchar_t const *pc = line.c_str();

        // Example line:
        //
        //     00000072`a512ee18  ee d1 22 33 ff 7f 00 00-ff ff ff ff 00 00 00 00  .."3............
        //
        // We continue to parse lines and generate "memory regions".  Note that we do limited error checking
        // if there are multiple memory regions which are the same address.  The first one will "win."
        //

        ULONG64 lineAddr = 0;
        if (!ParseHex(pc, &lineAddr, &pc))
        {
            return E_FAIL;
        }

        if (firstLine)
        {
            startAddress = curAddress = lineAddr;
            firstLine = false;
        }
        else
        {
            //
            // Is this next line a new region or a continuation of the previous one.
            //
            if (lineAddr != curAddress)
            {
                m_memoryRegions.push_back({ startAddress, curAddress, std::move(data) });
                startAddress = curAddress = lineAddr;
                data = {};
            }
        }

        while (iswspace(*pc)) { ++pc; }

        for (;;)
        {
            ULONG64 byteData;
            wchar_t const *pn;
            if (ParseHex(pc, &byteData, &pn) &&  pn - pc == 2)
            {
                data.push_back(static_cast<unsigned char>(byteData));
                curAddress++;
                pc = pn;
            }
            else
            {
                //
                // It's not our expected format.
                //
                m_memoryRegions.clear();
                return E_FAIL;
            }

            if (iswspace(*pc) || *pc == L'-')
            {
                ++pc;
            }

            if (!*pc || isspace(*pc))
            {
                break;
            }
        }
    }

    if (startAddress != curAddress)
    {
        m_memoryRegions.push_back({ startAddress, curAddress, std::move(data) });
    }

    if (m_memoryRegions.size() == 0)
    {
        return E_FAIL;
    }

    return hr;
}

HRESULT TextDumpParser::Parse()
{
    auto fn = [&]()
    {
        HRESULT hr = S_OK;

        std::wstring line;
        while(ReadLine(&line))
        {
            wchar_t const *pc = line.c_str();
            if (*pc == L'#' || IsEmptyLine(line))
            {
                continue;
            }

            if (wcscmp(pc, L"*** MEMORY") == 0)
            {
                IfFailedReturn(ParseMemoryRegions());
            }
            else if (wcscmp(pc, L"*** STACK") == 0)
            {
                IfFailedReturn(ParseStackFrames());
            }
            else if (wcscmp(pc, L"*** MODULEINFO") == 0)
            {
                IfFailedReturn(ParseModuleInformation());
            }
            else if (wcscmp(pc, L"*** REGISTERS") == 0)
            {
                IfFailedReturn(ParseRegisters());
            }
        }

        return hr;
    };
    return ConvertException(fn);
}

} // TextDump
} // Services
} // TargetComposition
} // Debugger
