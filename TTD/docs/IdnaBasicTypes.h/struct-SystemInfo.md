# SystemInfo Structure

Comprehensive system information structure capturing operating system details, processor characteristics, timing information, and user context for TTD recordings.

## Definition

```cpp
struct SystemInfo
{
    uint32_t            MajorVersion;   // Log major version
    uint32_t            MinorVersion;   // Log minor version
    uint32_t            BuildNumber;    // Log build version
    uint32_t            ProcessId;      // System Process Id

    TimingInfo Time;

    struct
    {
        // ProcessorArchitecture, ProcessorLevel and ProcessorRevision are all
        // taken from the SYSTEM_INFO structure obtained by GetSystemInfo().
        uint16_t ProcessorArchitecture;
        uint16_t ProcessorLevel;
        uint16_t ProcessorRevision;
        uint8_t  NumberOfProcessors;
        uint8_t  ProductType;

        //
        // MajorVersion, MinorVersion, BuildNumber, PlatformId and
        // CSDVersion are all taken from the OSVERSIONINFO structure
        // returned by GetVersionEx().
        //

        uint32_t MajorVersion;
        uint32_t MinorVersion;
        uint32_t BuildNumber;
        uint32_t PlatformId;

        //
        // RVA to a CSDVersion string in the string table.
        //

        uint32_t CSDVersionRva;
        uint16_t SuiteMask;
        uint16_t Reserved2;

        union
        {
            // X86 platforms use CPUID function to obtain processor information.
            struct
            {
                // CPUID Subfunction 0, register EAX (VendorId[0]),
                // EBX (VendorId[1]) and ECX (VendorId[2]).
                uint32_t VendorId[3];
                // CPUID Subfunction 1, register EAX
                uint32_t VersionInformation;
                // CPUID Subfunction 1, register EDX
                uint32_t FeatureInformation;

                // CPUID, Subfunction 80000001, register EBX. This will only
                // be obtained if the vendor id is "AuthenticAMD".
                uint32_t AMDExtendedCpuFeatures;
            } X86CpuInfo;

            // Non-x86 platforms use processor feature flags.
            struct
            {
                uint64_t ProcessorFeatures[2];
            } OtherCpuInfo;
        } Cpu;
    } System;

    // Name of person that ran the guest process.
    char16_t UserName[64];
    char16_t SystemName[64];
};
```

## Fields

### Log Information
- `MajorVersion` - TTD log format major version number
- `MinorVersion` - TTD log format minor version number
- `BuildNumber` - TTD log format build number
- `ProcessId` - System process identifier for the recorded process

### Timing Information
- `Time` - [`TimingInfo`](struct-TimingInfo.md) structure containing process and system timing data

### System Information
- `System.ProcessorArchitecture` - Processor architecture (from `SYSTEM_INFO`)
- `System.ProcessorLevel` - Processor level/family (from `SYSTEM_INFO`)
- `System.ProcessorRevision` - Processor revision (from `SYSTEM_INFO`)
- `System.NumberOfProcessors` - Number of logical processors
- `System.ProductType` - Windows product type (workstation, server, etc.)

### Operating System Information
- `System.MajorVersion` - OS major version (from `OSVERSIONINFO`)
- `System.MinorVersion` - OS minor version (from `OSVERSIONINFO`)
- `System.BuildNumber` - OS build number (from `OSVERSIONINFO`)
- `System.PlatformId` - Platform identifier (from `OSVERSIONINFO`)
- `System.CSDVersionRva` - RVA to service pack string in string table
- `System.SuiteMask` - Suite mask indicating installed components

### Processor-Specific Information
- `System.Cpu.X86CpuInfo` - x86/x64-specific processor information from CPUID
  - `VendorId[3]` - CPU vendor identification string (12 bytes)
  - `VersionInformation` - CPU version information from CPUID.01h.EAX
  - `FeatureInformation` - CPU feature flags from CPUID.01h.EDX
  - `AMDExtendedCpuFeatures` - AMD-specific extended features (AMD only)
- `System.Cpu.OtherCpuInfo` - Non-x86 processor feature information
  - `ProcessorFeatures[2]` - Platform-specific processor features

### User Context
- `UserName[64]` - UTF-16 username who initiated the recording
- `SystemName[64]` - UTF-16 system/computer name where recording occurred

## Usage

### System Information Extraction
```cpp
static void ProcessTrace(IReplayEngineView const& replayEngineView) {
    SystemInfo const& sysInfo = replayEngineView.GetSystemInfo();

    // Log version information
    printf("TTD Log Version: %u.%u.%u\n",
           sysInfo.MajorVersion,
           sysInfo.MinorVersion,
           sysInfo.BuildNumber);

    printf("Process ID: %u\n", sysInfo.ProcessId);

    // Operating system information
    printf("OS Version: %u.%u.%u (Platform: %u)\n",
           sysInfo.System.MajorVersion,
           sysInfo.System.MinorVersion,
           sysInfo.System.BuildNumber,
           sysInfo.System.PlatformId);
}
```

## Important Notes

- System information is captured at the time of recording initiation
- Processor architecture affects which CPU information union member is valid
- Username and system name are stored as UTF-16 strings with 64-character limits
- Timing information uses Windows FILETIME format (100-nanosecond intervals)
- Service pack information is referenced via RVA to string table
- AMD-specific CPU features are only populated for AMD processors
- Non-x86 platforms use processor feature flags instead of CPUID information
- This information is crucial for replay compatibility and environment analysis

## See Also

- [`TimingInfo`](struct-TimingInfo.md) - Process and system timing information
