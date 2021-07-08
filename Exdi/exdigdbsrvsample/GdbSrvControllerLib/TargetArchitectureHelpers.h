//----------------------------------------------------------------------------
//
// TargetArchitectureHelpers.h
//
// Helpers to handle general utility functions for different targets 
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
#include "GdbSrvControllerLib.h"

using namespace GdbSrvControllerLib;
using namespace std;

//
//  Architectture related constans
//
#define TARGET_ARM_ARCH_PTR_SIZE(target)      ((target == ARM32_ARCH) ? sizeof(ULONG) : sizeof(ULONG64))
#define TARGET_INTEL_ARCH_PTR_SIZE(target)    ((target == X86_ARCH) ? sizeof(ULONG) : sizeof(ULONG64))
#define GET_PTR_SIZE_BY_ARCH(arch)            (((arch == X86_ARCH) || (arch == AMD64_ARCH)) ? \
                                               TARGET_INTEL_ARCH_PTR_SIZE(arch) : TARGET_ARM_ARCH_PTR_SIZE(arch))
//
// Define coprocessor access intrinsics.  Coprocessor 15 contains
// registers for the MMU, cache, TLB, feature bits, core
// identification and performance counters.
// op0=2/3 encodings, use with _Read/WriteStatusReg
//
#define ARM64_ENCODING_SYSREG(op0, op1, CRn, CRm, op2) \
                             ( (((op0 & 0xf) << 16) & 0xf0000) | \
                               (((op1 & 0xf) << 12) & 0x0f000) | \
                               (((CRn & 0xf) <<  8) & 0x00f00) | \
                               (((CRm & 0xf) <<  4) & 0x000f0) | \
                               (((op2 & 0xf) <<  0) & 0x0000f))

//  ARM64 Exception Level 1 cpsr register values (CPSRM_EL1h)
const DWORD C_EL1TCPSRREG = 4;
const DWORD C_EL1HCPSRREG = 5;
//  ARM64 Exception Level 2
const DWORD C_EL2TCPSRREG = 8;
const DWORD C_EL2HCPSRREG = 9;

// Invalid Address type value
const AddressType c_InvalidAddress = static_cast<AddressType>(-1);

//  This structure indicates the ARM64 System register acccode code encoding
typedef struct
{
    ULONG  Direction : 1;
    ULONG  CRm : 4;
    ULONG  Rt  : 5;
    ULONG  CRn : 4;
    ULONG  Op1 : 3;
    ULONG  Op2 : 3;
    ULONG  Op0 : 2;
    ULONG  Res0 : 3;
    ULONG  InstructionSize : 1;
    ULONG  ExceptionCode : 6;
} SystemRegister;

// ************************************************************************************
//
#pragma region Architecture helpers

class TargetArchitectureHelpers
{
public:

    static inline void DisplayTextData(_In_reads_bytes_(readSize) const char* pBuffer, _In_ size_t readSize,
        _In_ GdbSrvTextType textType, _In_ IGdbSrvTextHandler* const pTextHandler)
    {
        if (pTextHandler != nullptr)
        {
            pTextHandler->HandleText(textType, pBuffer, readSize);
        }
    }

    static void DisplayCommData(_In_reads_bytes_(readSize) const char* pBuffer, _In_ size_t readSize,
        _In_ GdbSrvTextType textType, _In_ IGdbSrvTextHandler* const pTextHandler, _In_ unsigned channel)
    {
        UNREFERENCED_PARAMETER(channel);
        DisplayTextData(pBuffer, readSize, textType, pTextHandler);
    }

    static void DisplayCommDataForChannel(_In_reads_bytes_(readSize) const char* pBuffer, _In_ size_t readSize,
        _In_ GdbSrvTextType textType, _In_ IGdbSrvTextHandler* const pTextHandler, _In_ unsigned channel)
    {
        UNREFERENCED_PARAMETER(readSize);

        if (pTextHandler != nullptr && *pBuffer != '\x0')
        {
            char channelText[128];
            sprintf_s(channelText, _countof(channelText), "Core:%u ", channel);
            std::string channelString(channelText);
            channelString += pBuffer;
            DisplayTextData(channelString.c_str(), strlen(channelString.c_str()), textType, pTextHandler);
        }
    }

    static AddressType EncodeAccessCode(
        _In_ TargetArchitecture arch,
        _In_ int op0,
        _In_ int op1,
        _In_ int crn,
        _In_ int crm,
        _In_ int op2)
    {
        if (arch == ARM64_ARCH)
        {
            return ARM64_ENCODING_SYSREG(op0, op1, crn, crm, op2);
        }
        else
        {
            MessageBox(0, _T("Target architecture is not supported"), _T("EXDI-GdbServer"), MB_ICONERROR);
        }
        return c_InvalidAddress;
    }

    static const char* GetProcessorStatusRegByArch(_In_ TargetArchitecture arch)
    {
        const char* pStatusRegister = nullptr;
        if (arch == ARM64_ARCH)
        {
            pStatusRegister = "cpsr";
        }
        return pStatusRegister;
    }

    static void GetMemoryPacketType(_In_ TargetArchitecture arch, _In_ DWORD64 cpsrRegValue, _Out_ memoryAccessType* pMemType)
    {
        assert(pMemType != nullptr);

        *pMemType = { 0 };
        if (arch == ARM64_ARCH)
        {
            //  Check the current target CPU mode stored in the CPSR register in order to set the correct memory type.
            if (cpsrRegValue != 0)
            {
                switch (cpsrRegValue & 0xf)
                {
                    //  NT space
                case C_EL1HCPSRREG:
                case C_EL1TCPSRREG:
                {
                    pMemType->isSupervisor = true;
                }
                break;

                //  Hypervisor space
                case C_EL2TCPSRREG:
                case C_EL2HCPSRREG:
                {
                    pMemType->isHypervisor = true;
                }
                break;

                default:
                {
                    // Force to use a supervisor mode packet as it should never fail the memory read,
                    // other than hypervisor or secure mode
                    pMemType->isSupervisor = true;
                }
                }
            }
        }
    }

    static HRESULT SetSystemRegister(_In_ TargetArchitecture arch, _In_ AddressType encodeRegIndex, 
        _Out_ SystemRegister * pSystemReg)
    {
        HRESULT hr = E_NOTIMPL;
        if (arch == ARM64_ARCH)
        {
            SystemRegister SysReg = { 0 };

            SysReg.Op0 = (encodeRegIndex >> 16) & 3;
            SysReg.Op1 = (encodeRegIndex >> 12) & 7;
            SysReg.CRn = (encodeRegIndex >> 8) & 15;
            SysReg.CRm = (encodeRegIndex >> 4) & 15;
            SysReg.Op2 = (encodeRegIndex >> 0) & 7;
            *pSystemReg = SysReg;
            hr = S_OK;
        }
        return hr;
    }

    static HRESULT SetSpecialMemoryPacketTypeARM64(_In_ ULONGLONG cpsrReg, _Out_ memoryAccessType* pMemType)
    {
        assert(pMemType != nullptr);

        HRESULT hr = S_OK;
        switch (cpsrReg & 0xf)
        {
            //  NT space
        case C_EL1HCPSRREG:
        case C_EL1TCPSRREG:
            //  Hypervisor space
        case C_EL2TCPSRREG:
        case C_EL2HCPSRREG:
        {
            pMemType->isSpecialRegs = 1;
        }
        break;

        default:
        {
            hr = E_FAIL;
            MessageBox(0, _T("Error: Invalid processor mode for getting ARM64 special registers."), _T("EXDI-GdbServer"), MB_ICONERROR);
        }
        }
        return hr;
    }

    static HRESULT SetSpecialMemoryPacketType(_In_ TargetArchitecture arch, _In_ ULONGLONG cpsrReg, _Out_ memoryAccessType* pMemType)
    {
        HRESULT hr = E_NOTIMPL;
        if (arch == ARM64_ARCH)
        {
            hr = TargetArchitectureHelpers::SetSpecialMemoryPacketTypeARM64(cpsrReg, pMemType);
        }
        return hr;
    }

    static std::wstring WMakeLowerCase(_In_ PCWSTR pIn)
    {
        std::wstring out = pIn;
        std::transform(out.cbegin(), out.cend(),  // Source
            out.begin(),               // destination
            [](wchar_t c) {
                return towlower(c);        // operation
            });
        return out;
    }

    static std::string MakeLowerCase(_In_ PCSTR pIn)
    {
        std::string out = pIn;
        std::transform(out.cbegin(), out.cend(),  // Source
            out.begin(),               // destination
            [](char c) {
                return tolower(c);        // operation
            });
        return out;
    }

    static void ReplaceString(_Inout_ std::wstring& str, _In_ const std::wstring& search, _In_ const std::wstring& replace)
    {
        std::size_t pos = 0;
        while ((pos = str.find(search, pos)) != std::string::npos)
        {
            str.replace(pos, search.length(), replace);
            pos += replace.length();
        }
    }

    //
    //  ReverseRegValue         Returns a string containing the passed in register string in reverse order.
    //
    //  Parameters:
    //  inputRegTargetOrder     Reference to the input string that contains hex-ascii characters in target order.
    //
    //  Return:
    //  The reversed string.
    //
    static std::string ReverseRegValue(_In_ const std::string& inputRegTargetOrder)
    {
        std::string outRegValue(inputRegTargetOrder);

        size_t outRegValueLength = outRegValue.length();
        for (size_t idx = 0; idx < outRegValueLength; idx += 2)
        {
            std::swap(outRegValue[idx], outRegValue[idx + 1]);
        }
        reverse(outRegValue.begin(), outRegValue.end());

        return outRegValue;
    }

    static void TokenizeThreadId(_In_ const std::string& value, _In_z_ const char* delimiters, _Out_ std::vector<std::string>* pTokens)
    {
        char* pData = const_cast<char*>(value.data());
        char* next_token = nullptr;
        char* token = strtok_s(pData, delimiters, &next_token);

        while (token != nullptr)
        {
            int tokenValue;
            if (sscanf_s(token, "%d", &tokenValue) != 1)
            {
                throw _com_error(E_FAIL);
            }
            char tokenStr[256] = { 0 };
            sprintf_s(tokenStr, _countof(tokenStr), "%d", tokenValue);
            pTokens->push_back(tokenStr);
            token = strtok_s(nullptr, delimiters, &next_token);
        }
    }

    static void TokenizeAccessCode(_In_ const std::wstring& value, 
        _In_z_ const wchar_t * delimiters, _Out_ std::vector<int>* pTokens)
    {
        wchar_t * pData = const_cast<wchar_t *>(value.data());
        wchar_t* next_token = nullptr;
        wchar_t* token = wcstok_s(pData, delimiters, &next_token);

        while (token != nullptr)
        {
            int tokenValue;
            if (swscanf_s(token, L"%d", &tokenValue) != 1)
            {
                throw _com_error(E_FAIL);
            }
            pTokens->push_back(tokenValue);
            token = wcstok_s(nullptr, delimiters, &next_token);
        }
    }

};

#pragma endregion
