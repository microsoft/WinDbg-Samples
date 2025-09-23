# Time Travel Debugging (TTD) SDK

Welcome to the Time Travel Debugging (TTD) SDK! This toolkit provides everything you need to leverage Microsoft's revolutionary Time Travel Debugging technology for advanced debugging, analysis, and security research.

## What is Time Travel Debugging?

Time Travel Debugging (TTD) is a powerful debugging technology that records the complete execution of a program, allowing you to replay and analyze it deterministically. TTD enables developers and researchers to:

- **Navigate backwards and forwards** through program execution with precision
- **Capture complete execution traces** including memory accesses, register states, and API calls
- **Debug intermittent and hard-to-reproduce issues** with perfect reproducibility
- **Perform post-mortem analysis** on crashes and complex failures
- **Analyze malware and security vulnerabilities** in a safe, controlled environment
- **Extract detailed insights** from program behavior without source code

## SDK Components

This SDK is organized into three main areas, each serving different aspects of TTD development:

### [ReplayApi/](ReplayApi/README.md)
**TTD Replay Engine APIs and Samples**

The core of the SDK, containing samples for programmatic analysis of TTD trace files. This includes:
- **Integration into interactive debuggers** and similar debugging tools.
- TTD **support for existing tools** via plugins.
- **Whole-trace analysis** tools to gather data like code coverage or memory map.
- **Directed analysis** tools like object lifetime trackers and data race detection.

Perfect for developers who want to build custom analysis tools, extract specific insights from traces, or integrate TTD capabilities into existing debugging pipelines. This is the same API the Windows debugger (WinDbg) uses for TTD support, now we are making it available for everyone.

### [LiveRecorderApiSample/](LiveRecorderApiSample/README.md)
**Live Recording API Sample**

Demonstrates how applications can use TTD's live recording capabilities to record portions of their own execution in real-time. This sample shows:
- **Self-recording applications** that capture their own execution
- **Selective recording** of critical code sections
- **Integration patterns** for embedding TTD recording in production code

Ideal for developers who want to add TTD recording capabilities directly into their applications for enhanced debugging and diagnostics.

### [docs/](docs/README.md)
**API Documentation and Programming Guides**

Documentation for all TTD programming interfaces, including:
- **API reference documentation** for all TTD interfaces and types
- **Programming concepts** and TTD-specific terminology
- **Best practices** for performance and reliability

Essential reading for anyone developing with TTD APIs, whether using the Replay Engine or Live Recorder interfaces.

## Getting Started

### 1. **Explore the Documentation**
Start with the [docs/](docs/README.md) directory to understand TTD concepts, terminology, and available APIs.

### 2. **Try the Samples**
- For **trace analysis**: Explore [ReplayApi/](ReplayApi/README.md) samples
- For **live recording**: Check out [LiveRecorderApiSample/](LiveRecorderApiSample/README.md)

### 3. **Build and Experiment**
All samples include Visual Studio project files and can be built using C++20 or later. Each directory contains detailed build instructions and usage examples.
The header files are also compatible with Clang for Windows.

## Prerequisites

### System Requirements
- **Windows 10** (version 1903 or later) or **Windows 11**
- **Visual Studio 2022** or later with C++ development tools
- **Windows SDK 10.0** or later
- **Administrative privileges** (required for TTD recording operations)

### Architecture Support
- **x64** (Intel/AMD 64-bit) - Able to record both x64 and x86 applications and replay any architecture (including ARM64)
- **x86** (32-bit) - Able to record and replay x86 applications only
- **ARM64** - Able to record and replay ARM64 applications only

### TTD Runtime
TTD requires the TTD runtime components. These can be obtained through:
- **Windows SDK** installation
- **WinDbg** (includes TTD runtime)
- **Automated download scripts** (included in samples - see [GetTtd/](ReplayApi/GetTtd) for details)

## API versioning

To set expectations, this is considered an experimental API (which is why its major version is 0). In particular:

- TTD's C++ APIs may be revised, enhanced or upgraded in future releases of TTD.
- These API updates may include binary incompatibilities with previous API releases, in which case the relevant versioning GUIDs will change.
- These APIs are not intended to be supported forever by TTD. The toolset is meant for developers, from developers. Any maintained tool that uses TTD APIs is expected to stay up to date with TTD revisions at their own discretion.
- Future releases of TTD will continue to support "old" API revisions for an unspecified amount of time, intended to allow users of the API to have ample freedom to choose when to update. The versioning GUIDs ensure that tools will receive the correct binary API implementation, and that they will receive null interfaces if the requested version is no longer supported.
- To ease the transition to a new API revision, such new revision will try to maintain source compatibility wherever possible with the previous revision. Wherever possible, any disappearing or legacy definitions will be provided as aliases and inline functions that mimic the old definitions, and marked "deprecated" to signal the need to update code to fit the new revision.

## Community and Support

### Resources
- **Official Documentation**: [Microsoft TTD Documentation](https://aka.ms/ttd)
- **WinDbg Download**: [WinDbg](https://aka.ms/windbg/download)
- **Sample Repository**: [WinDbg-Samples on GitHub](https://github.com/microsoft/WinDbg-Samples)
- **Debugging Documentation**: [Windows Debugging Tools](https://aka.ms/windbg)
- **Feedback and Issues**: Use the GitHub repository issues page for reporting bugs or requesting features.
## License

This project is licensed under the MIT License. See the LICENSE file in the repository root for complete details.
