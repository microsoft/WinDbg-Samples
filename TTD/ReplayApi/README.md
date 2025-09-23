# TTD Replay API Samples and Tools

This directory contains samples and tools that demonstrate how to use the Microsoft Time Travel Debugging (TTD) Replay Engine APIs. These APIs enable programmatic analysis of TTD trace files for debugging, performance analysis, and security research.

## Overview

The TTD Replay Engine provides powerful APIs for:
- **Reading and analyzing trace files** programmatically
- **Navigating through execution history** with precise control
- **Extracting runtime information** such as memory accesses, register states, and API calls
- **Building custom analysis tools** tailored to specific debugging scenarios
- **Integrating TTD functionality** into existing debugging workflows

## Samples and Tools

There are several samples to get you started:

[TraceInformation/](TraceInformation/) - A command-line tool that reads TTD trace files and displays comprehensive information about the traced execution
[TraceAnalysis/](TraceAnalysis/) - A command-line tool that demonstrates data mining techniques from trace files, using code coverage collection as an example
[TraceDebugger/](TraceDebugger/) - A mini-debugger application that demonstrates interactive debugging capabilities for TTD trace files
[TtdExtension/](TtdExtension/) - A debugger extension that demonstrates how to access TTD Replay API interfaces from within WinDbg.

Most of these samples produce binaries that depend on the TTD runtime components (TTDReplay.dll and TTDReplayCPU.dll). To get a copy of those, go to [GetTtd/](GetTtd/)
and follow the instructions there.

### [inc/](inc/)
**Common Headers and Utilities**

Shared header files and utility functions used across multiple samples:
- **`Formatters.h`**: Custom formatters for TTD types with `std::format`
- **`RegisterNameMapping.h`**: CPU register name mapping utilities
- **`ReplayHelpers.h`**: Common helper functions for replay operations

## Building the Samples

### Prerequisites
- **Visual Studio 2022 or later** with C++ development tools
- **Windows SDK 10.0** or later
- **C++20 compiler support** (all samples use C++20 standard)
- **Run Get-Ttd.cmd or Get-Ttd.ps1 in GetTtd directory** to download the TTD runtime components

### Build Instructions
1. Open the solution file (`.sln`) for each sample in Visual Studio
2. Select your target architecture (x64, x86, or ARM64)
3. Build the solution (Ctrl+Shift+B)

### NuGet Dependencies
The samples automatically download the required TTD API packages via NuGet:
- `Microsoft.TimeTravelDebugging.Apis` - Core TTD Replay Engine APIs

## Using the Samples

### Command-Line Tools
Most samples are command-line applications that accept trace file paths as arguments:

```cmd
TraceInformation.exe path\to\trace.run
TraceAnalysis.exe path\to\trace.run
TraceDebugger.exe path\to\trace.run
```

### WinDbg Extension
The TtdExtension sample creates a DLL that can be loaded into WinDbg:

1. Build the TtdExtension project
2. Start WinDbg and open a trace file
3. Load the extension: `.load path\to\TtdExtension.dll`
4. Run commands: `!info`, `!help`

## Additional Resources

- [Microsoft Time Travel Debugging Documentation](https://aka.ms/ttd)
- [WinDbg Download](https://aka.ms/windbg/download)
- [Debugging Tools for Windows](https://aka.ms/windbg)

---

*These samples provide a foundation for building powerful trace analysis tools using the TTD Replay Engine APIs.*
