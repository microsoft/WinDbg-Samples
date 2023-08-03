//
//    Copyright (C) Microsoft.  All rights reserved.
//
//----------------------------------------------------------------------------
//
// Debug engine Ioctls for extending EXDI.
// Covers:
//   Read/write MSRs.
//   Multiprocessor description and control.
//   Determination of what breakpoint was hit for hrBp.
//
//----------------------------------------------------------------------------

#ifndef __DBGENG_EXDI_IO_H__
#define __DBGENG_EXDI_IO_H__

//
// Specific Ioctl operations.
// All Ioctl structures must have the Ioctl code as the first member.
//

typedef enum
{
    // Marker for the beginning of the enum.  Start at
    // a value other than zero to prevent obvious collisions
    // with other Ioctl codes.
    DBGENG_EXDI_IOC_BEFORE_FIRST = 0x8664,
    
    DBGENG_EXDI_IOC_IDENTIFY,
    DBGENG_EXDI_IOC_READ_MSR,
    DBGENG_EXDI_IOC_WRITE_MSR,
    DBGENG_EXDI_IOC_IDENTIFY_PROCESSORS,
    DBGENG_EXDI_IOC_GET_CURRENT_PROCESSOR,
    DBGENG_EXDI_IOC_SET_CURRENT_PROCESSOR,
    DBGENG_EXDI_IOC_GET_BREAKPOINT_HIT,
    DBGENG_EXDI_IOC_GET_KPCR,   //Gets the KPCR pointer for the current thread (e.g. from the TPIDRPRW register on ARM)

    // Marker for the end of the enum.
    DBGENG_EXDI_IOC_AFTER_LAST
} DBGENG_EXDI_IOCTL_CODE;

//  This is the next set of IOCTL codes used for exdiV3
typedef enum
{
    DBGENG_EXDI_IOCTL_V3_FIRST = DBGENG_EXDI_IOC_AFTER_LAST + 1,
    //  Get KPCR from the GDTR field for the current thread/processor.
    DBGENG_EXDI_IOCTL_V3_GET_KSPECIAL_REG_DESCRIPTOR,
    //  Store the KPCR value for later usage.
    DBGENG_EXDI_IOCTL_V3_STORE_KPCR_VALUE,
    //  Get the NT base address obtained by the COM server
    DBGENG_EXDI_IOCTL_V3_GET_NT_BASE_ADDRESS_VALUE,
    //  Get the Special registers memory content.
    DBGENG_EXDI_IOCTL_V3_GET_SPECIAL_REGISTER_VALUE,
    //  Get Supervisor/System mode memory content
    DBGENG_EXDI_IOCTL_V3_GET_SUPERVISOR_MODE_MEM_VALUE,
    //  Get Hypervisor mode memory content
    DBGENG_EXDI_IOCTL_V3_GET_HYPERVISOR_MODE_MEM_VALUE,
    //  Get additional GDB server info
    DBGENG_EXDI_IOCTL_V3_GET_ADDITIONAL_SERVER_INFO,

    DBGENG_EXDI_IOCTL_V3_LAST
} DBGENG_EXDI_IOCTL_CODE_V3_EX;

//  Store the kpcr offset.
typedef struct
{
    DBGENG_EXDI_IOCTL_CODE_V3_EX code;
    ULONG processorNumber;
    ULONG64 kpcrOffset;
} DBGENG_EXDI_IOCTL_STORE_KPCR_V3_EX_IN;

//  Special register content request
typedef struct
{
    DBGENG_EXDI_IOCTL_CODE_V3_EX code;
    ADDRESS_TYPE address; 
    ULONG bytesToRead;
} DBGENG_EXDI_IOCTL_READ_SPECIAL_MEMORY_EX_IN;

//  Additional info input structure
typedef struct
{
    DBGENG_EXDI_IOCTL_CODE_V3_EX code;
    struct
    {
        ULONG   HeuristicChunkSize:1;
        ULONG   RequireMemoryAccessByPA: 1;
        ULONG   Reserved:30;
    } request;
} DBGENG_EXDI_IOCTL_V3_GET_ADDITIONAL_SERVER_INFO_EX_IN;

//
// Basic Ioctl containing only a code for the Ioctl input.
//

typedef struct _DBGENG_EXDI_IOCTL_BASE_IN
{
    DBGENG_EXDI_IOCTL_CODE Code;
} DBGENG_EXDI_IOCTL_BASE_IN;

//
// IDENTIFY - Verify and describe Ioctl support.
//

#define DBGENG_EXDI_IOCTL_IDENTIFY_SIGNATURE '468E'

typedef struct _DBGENG_EXDI_IOCTL_IDENTIFY_OUT
{
    ULONG Signature;
    DBGENG_EXDI_IOCTL_CODE BeforeFirst;
    DBGENG_EXDI_IOCTL_CODE AfterLast;
} DBGENG_EXDI_IOCTL_IDENTIFY_OUT;

//
// {READ|WRITE}_MSR - Access processor MSRs.
//

// Input structure is used for both read and write.
typedef struct _DBGENG_EXDI_IOCTL_MSR_IN
{
    DBGENG_EXDI_IOCTL_CODE Code;
    ULONG Index;
    // Value is only used for write.
    ULONG64 Value;
} DBGENG_EXDI_IOCTL_MSR_IN;

typedef struct _DBGENG_EXDI_IOCTL_READ_MSR_OUT
{
    ULONG64 Value;
} DBGENG_EXDI_IOCTL_READ_MSR_OUT;

//
// Multiprocessor support.  Basic EXDI doesn't support
// multiprocessor machines so add Ioctls to query and
// control a "current" processor that the EXDI methods
// apply to.
//

//
// IDENTIFY_PROCESSORS - Used to query the processor configuration.
// Currently only the count is used.  Other fields are zeroed.
//

typedef struct _DBGENG_EXDI_IOCTL_IDENTIFY_PROCESSORS_OUT
{
    ULONG Flags;
    ULONG NumberProcessors;
    ULONG64 Reserved[7];
} DBGENG_EXDI_IOCTL_IDENTIFY_PROCESSORS_OUT;

//
// {GET|SET}_CURRENT_PROCESSOR - Current processor control.
//

typedef struct _DBGENG_EXDI_IOCTL_GET_CURRENT_PROCESSOR_OUT
{
    ULONG Processor;
} DBGENG_EXDI_IOCTL_GET_CURRENT_PROCESSOR_OUT;

typedef struct _DBGENG_EXDI_IOCTL_SET_CURRENT_PROCESSOR_IN
{
    DBGENG_EXDI_IOCTL_CODE Code;
    ULONG Processor;
} DBGENG_EXDI_IOCTL_SET_CURRENT_PROCESSOR_IN;

//
// GET_BREAKPOINT_HIT - Determine which breakpoint was hit
// after a breakpoint halt reason.
//

#define DBGENG_EXDI_IOCTL_BREAKPOINT_NONE 0
#define DBGENG_EXDI_IOCTL_BREAKPOINT_CODE 1
#define DBGENG_EXDI_IOCTL_BREAKPOINT_DATA 2

typedef struct _DBGENG_EXDI_IOCTL_GET_BREAKPOINT_HIT_OUT
{
    ADDRESS_TYPE Address;
    ULONG AccessWidth;
    DATA_ACCESS_TYPE AccessType;
    ULONG Type;
} DBGENG_EXDI_IOCTL_GET_BREAKPOINT_HIT_OUT, *PDBGENG_EXDI_IOCTL_GET_BREAKPOINT_HIT_OUT;

struct DBGENG_EXDI_IOC_GET_KPCR_IN
{
    DBGENG_EXDI_IOCTL_CODE Code;
    ULONG ProcessorNumber;
};

//Output for DBGENG_EXDI_IOC_GET_KPCR is PULONG64

#endif // #ifndef __DBGENG_EXDI_IO_H__
