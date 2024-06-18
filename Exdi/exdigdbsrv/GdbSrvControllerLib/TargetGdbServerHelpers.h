//----------------------------------------------------------------------------
//
// TargetGdbServerHelpers.h
//
// Helpers to handle specific Gdb server target comands
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
//  Gdb server related constans
//


// ************************************************************************************
//
#pragma region Target GDB server helpers

class Trace32GdbServerMemoryHelpers
{
public:

    static inline PCSTR GetGdbSrvReadMemoryCmd(
        _In_ memoryAccessType memType,
        _In_ bool is64BitArchitecture,
        _In_ TargetArchitecture targetArchitecture)
    {
        PCSTR pFormat = nullptr;

        if (memType.isPhysical)
        {
            pFormat = (is64BitArchitecture) ? "qtrace32.memory:a,%I64x,%x" : "qtrace32.memory:a,%x,%x";
        }
        else if (memType.isSupervisor)
        {
            pFormat = (is64BitArchitecture) ? "qtrace32.memory:s,%I64x,%x" : "qtrace32.memory:s,%x,%x";
        }
        else if (memType.isHypervisor)
        {
            pFormat = (is64BitArchitecture) ? "qtrace32.memory:h,%I64x,%x" : "qtrace32.memory:h,%x,%x";
        }
        else if (memType.isSpecialRegs)
        {
            if (targetArchitecture == ARM64_ARCH)
            {
                pFormat = "qtrace32.memory:SPR,%x,%x";
            }
            else if (targetArchitecture == ARM32_ARCH)
            {
                pFormat = "qtrace32.memory:C15,%x,%x";
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            pFormat = (is64BitArchitecture) ? "m%I64x,%x" : "m%x,%x";
        }
        return pFormat;
    }

    static inline PCSTR GetGdbSrvWriteMemoryCmd(
        _In_ memoryAccessType memType,
        _In_ bool is64BitArchitecture,
        _In_ TargetArchitecture targetArchitecture)
    {
        PCSTR pFormat = nullptr;

        if (memType.isPhysical)
        {
            pFormat = (is64BitArchitecture) ? "Qtrace32.memory:a,%I64x," : "Qtrace32.memory:a,%x,";
        }
        else if (memType.isSupervisor)
        {
            pFormat = (is64BitArchitecture) ? "Qtrace32.memory:s,%I64x," : "Qtrace32.memory:s,%x,";
        }
        else if (memType.isHypervisor)
        {
            pFormat = (is64BitArchitecture) ? "Qtrace32.memory:h,%I64x," : "Qtrace32.memory:h,%x,";
        }
        else if (memType.isSpecialRegs)
        {
            if (targetArchitecture == ARM64_ARCH)
            {
                pFormat = "Qtrace32.memory:SPR,%x,";
            }
            else if (targetArchitecture == ARM32_ARCH)
            {
                pFormat = "Qtrace32.memory:C15,%x,";
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            pFormat = (is64BitArchitecture) ? "M%I64x," : "M%x,";
        }

        return pFormat;
    }
};

class OpenOCDGdbServerMemoryHelpers
{
public:

    static inline PCSTR GetGdbSrvReadMemoryCmd(
        _In_ memoryAccessType memType,
        _In_ bool is64BitArchitecture)
    {

        PCSTR pFormat = nullptr;

        if (memType.isSpecialRegs)
        {
            //  Is it an OpenOCD GDB server
            pFormat = (is64BitArchitecture) ? "aarch64 mrs nsec %d %d %d %d %d" : "amd64 mrs nsec %d %d %d %d %d";
        }
/*
        @TODO: Modify the below code with the correct command, once OpenOCD
        provides a reliable command for reading physical memory.
        else if (memType.isPhysical)
        {
            pFormat = (is64BitArchitecture) ? "mdb phys %I64x %d" : "mdb phys %x %d";
        }
*/
        else
        {
            pFormat = is64BitArchitecture ? "m%I64x,%x" : "m%x,%x";
        }
        return pFormat;
    }

    static inline PCSTR GetGdbSrvWriteMemoryCmd(
        _In_ memoryAccessType memType,
        _In_ bool is64BitArchitecture)
    {
        PCSTR pFormat = nullptr;

        if (memType.isSpecialRegs)
        {
            pFormat = (is64BitArchitecture) ? "aarch64 mrs nsec %d %d %d %d %d %x" : "amd64 mrs nsec %d %d %d %d %d %x";
        }
/*      
        @TODO: Modify the below code with the correct command, once OpenOCD
        provides a reliable command for writing physical memory.
        else if (memType.isPhysical)
        {
            pFormat = (is64BitArchitecture) ? "mwb phys %I64x %d" : "mwb phys %x %d";
        }
*/
        else
        {
            pFormat = is64BitArchitecture ? "M%I64x," : "M%x,";
        }

        return pFormat;
    }
};

class BmcSmmDGdbServerMemoryHelpers
{
public:

    static inline PCSTR GetGdbSrvReadMemoryCmd(
        _In_ memoryAccessType memType,
        _In_ bool is64BitArchitecture)
    {

        return is64BitArchitecture ? "m%I64x,%x" : "m%x,%x";
    }

    static inline PCSTR GetGdbSrvWriteMemoryCmd(
        _In_ memoryAccessType memType,
        _In_ bool is64BitArchitecture)
    {

        return is64BitArchitecture ? "M%I64x," : "M%x,";
    }

    static inline PCSTR GetDynPAConfigModeCmd(_In_ bool mode)
    {
        return (mode) ? "Qqemu.PhyMemMode:1" : "Qqemu.PhyMemMode:0";
    }
};

class QEMUDGdbServerMemoryHelpers
{
public:

    static inline PCSTR GetGdbSrvReadMemoryCmd(
        _In_ memoryAccessType memType,
        _In_ bool is64BitArchitecture)
    {

        return is64BitArchitecture ? "m%I64x,%x" : "m%x,%x";
    }

    static inline PCSTR GetGdbSrvWriteMemoryCmd(
        _In_ memoryAccessType memType,
        _In_ bool is64BitArchitecture)
    {

        return is64BitArchitecture ? "M%I64x," : "M%x,";
    }

    static inline PCSTR GetDynPAConfigModeCmd(_In_ bool mode)
    {
        return (mode) ? "Qqemu.PhyMemMode:1" : "Qqemu.PhyMemMode:0";
    }
};

#pragma endregion
#pragma once
