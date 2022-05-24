//**************************************************************************
//
// FileParser.h
//
// A simple parser for the "text dump" files that we want the debugger to
// be able to open.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __FILEPARSER_H__
#define __FILEPARSER_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

struct StackFrame
{
    ULONG64 FrameNumber;
    ULONG64 ChildSp;
    ULONG64 RetAddr;
    std::wstring Module;
    std::wstring Symbol;
    ULONG64 Displacement;
};

struct MemoryRegion
{
    ULONG64 StartAddress;
    ULONG64 EndAddress;
    std::vector<unsigned char> Data;
};

struct ModuleInformation
{
    ULONG64 StartAddress;
    ULONG64 EndAddress;
    std::wstring ModuleName;
    std::wstring ModulePath;
    ULONG64 TimeStamp;
    ULONG64 ImageSize;
};

struct RegisterValue
{
    std::wstring Name;
    ULONG64 Value;
};

// TextDumpParser:
//
// A simple parser which parses our "text dump" file format.
//
class TextDumpParser
{
public:

    // TextDumpParser():
    //
    // Construct a new parser on a given file.
    //
    TextDumpParser(ISvcDebugSourceFile *pFile) :
        m_spFile(pFile),
        m_pFileMapping(nullptr),
        m_mappingSize(0),
        m_pos(0)
    {
    }

    // Initialize():
    //
    // Initializes the parser and performs a basic format check.  If this fails, the file is not
    // our text dump format.
    //
    HRESULT Initialize();

    // Parse():
    //
    // Parses the file and gathers all the information from each section of the text file.
    //
    HRESULT Parse();

    // Has*():
    //
    // Returns whether certain sections or section data exists in our "text dump"
    //
    bool HasStackFrames() const { return m_stackFrames.size() > 0; }
    bool HasMemoryRegions() const { return m_memoryRegions.size() > 0; }
    bool HasModuleInformations() const { return m_moduleInfos.size() > 0; }
    bool HasRegisters() const { return m_registerValues.size() > 0; }

    // Get*():
    //
    // Gets our view of certain section data from our "text dump"
    //
    std::vector<StackFrame> const &GetStackFrames() const { return m_stackFrames; }
    std::vector<MemoryRegion> const &GetMemoryRegions() const { return m_memoryRegions; }
    std::vector<ModuleInformation> const &GetModuleInformations() const { return m_moduleInfos; }
    std::vector<RegisterValue> const &GetRegisters() const { return m_registerValues; }

private:

    // ReadLine():
    //
    // Reads the next line from the text file and converts (if needed) to a standard Windows
    // UTF-16LE string.
    //
    bool ReadLine(_Out_ std::wstring *pLine);

    // IsEmptyLine():
    //
    // Checks whether the given string is an emtpy line (either "" or all whitespace of some form or another)
    //
    bool IsEmptyLine(_In_ std::wstring const& str);

    // ParseHex():
    //
    // Parses a hexidecimal value (no leading 0x, allows ` as a visual separator anywhere) at ps and returns
    // the value as a 64-bit unsigned value.  The character after the hex value is returned in ppc.  If there is
    // no parseable value at ps, false is returned.
    //
    bool ParseHex(_In_ wchar_t const *ps, _Out_ ULONG64 *pValue, _Out_ wchar_t const **ppc);

    // ParseStackFrames():
    //
    // Parses the stack frames section
    //
    HRESULT ParseStackFrames();

    // ParseMemoryRegions():
    //
    // Parses the memory regions.
    //
    HRESULT ParseMemoryRegions();

    // ParseModuleInformation():
    //
    // Parses the module information.
    //
    HRESULT ParseModuleInformation();

    // ParseRegisters():
    //
    // Parses the register information.
    //
    HRESULT ParseRegisters();

    std::vector<StackFrame> m_stackFrames;
    std::vector<MemoryRegion> m_memoryRegions;
    std::vector<ModuleInformation> m_moduleInfos;
    std::vector<RegisterValue> m_registerValues;

    Microsoft::WRL::ComPtr<ISvcDebugSourceFile> m_spFile;

    //*************************************************
    // Stream Reading:
    //

    bool m_isUtf8;
    char *m_pFileMapping;
    size_t m_mappingSize;
    size_t m_pos;

};

} // TextDump
} // Services
} // TargetComposition
} // Debugger

#endif // __FILEPARSER_H__
