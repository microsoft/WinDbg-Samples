// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// TraceInformation - A simple tool to print information about a Time Travel Debugging (TTD) trace file.
// 
// This tool uses the TTD Replay Engine to read a trace file and print information about the trace, such as the system
// information, thread information, module loading activity, and exceptions that occurred during the trace.

// The TTD Replay headers include a replaceable assert macro that can be used to check conditions during replay. If
// one is not defined, it defaults to no assertion checks. This sample will use the standard assert from the C runtime.
#include <assert.h>
#define DBG_ASSERT(cond) assert(cond)

// Include the necessary headers for the TTD Replay Engine. The Formatters.h header is not required for the replay engine itself,
// but it provides custom formatters for the TTD types to work with std::format, which is used in this sample.
#include <Formatters.h>
#include <TTD/IReplayEngineStl.h>
#include <TTD/ErrorReporting.h>

#include <filesystem>
#include <format>
#include <iostream>

// Make the sample code easier to read by bringing the TTD namespaces into scope.
using namespace TTD;
using namespace TTD::Replay;

// A simple error reporting class that prints errors to the console, as required
// by RegisterDebugModeAndLogging().
class BasicErrorReporting : public ErrorReporting
{
public:
    BasicErrorReporting() = default;

    void __fastcall VPrintError(_Printf_format_string_ char const* const pFmt, _In_ va_list argList) override
    {
        char buffer[2048];

        vsprintf_s(buffer, pFmt, argList);

        std::cout << std::format("Error: {}\n", buffer);
    }
};

// Print the exception information in a formatted manner.
static void PrintException(ExceptionEvent const& exception)
{
    // Most of this is standard Win32 exception information, but pThreadInfo
    // is a TTD-specific structure that contains the thread information. In this case, we will
    // print the UniqueId of the thread (unlike Win32 thread IDs, this value is guaranteed to
    // be unique in the trace).
    std::cout << std::format(
        "UTID: {:<6} Code: 0x{:08X} Flags: 0x{:04X} RecordAddress: 0x{:08X} PC: 0x{:08X} Parameters: ",
        exception.pThreadInfo->UniqueId,
        exception.Code,
        exception.Flags,
        exception.RecordAddress,
        exception.ProgramCounter
    );

    if (exception.ParameterCount == 0)
    {
        std::cout << std::format("None\n");
    }
    else
    {
        for (size_t i = 0; i < exception.ParameterCount; ++i)
        {
            if (i == 0)
            {
                std::cout << std::format("(0x{:X}", exception.Parameters[i]);
            }
            else
            {
                std::cout << std::format(", 0x{:X}", exception.Parameters[i]);
            }
        }
        std::cout << std::format(")\n");
    }
}

// This function presents information about the loaded trace file, to give a sense of
// how to use the replay API and the types of information contained in a trace file.
static void ProcessTrace(IReplayEngineView const& replayEngineView) {
    SystemInfo const& systemInfo = replayEngineView.GetSystemInfo();  

    std::cout << std::format("Version             : 1.{:02}.{:02}\n", 
        systemInfo.MajorVersion, systemInfo.MinorVersion);
    
    std::wcout << std::format(L"Index               : {}\n", GetIndexStatusName(replayEngineView.GetIndexStatus()));
    std::cout << std::format("PID                 : 0x{:04X}\n", systemInfo.ProcessId);
    std::cout << std::format("PEB                 : 0x{:X}\n", replayEngineView.GetPebAddress());
    
    std::cout << std::format("Lifetime            : {}\n", replayEngineView.GetLifetime());
    
    std::cout << std::format("Threads             : {:>11}\n", replayEngineView.GetThreadCount());
    std::cout << std::format("Modules             : {:>11}\n", replayEngineView.GetModuleCount());
    std::cout << std::format("ModuleInstances     : {:>11}\n", replayEngineView.GetModuleInstanceCount());
    std::cout << std::format("Exceptions          : {:>11}\n", replayEngineView.GetExceptionEventCount());
    std::cout << std::format("Keyframes           : {:>11}\n", replayEngineView.GetKeyframeCount());

    // Print the system information
    // (see https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info for more details)
    std::cout << std::format("System              :\n");
    
    std::cout << std::format("  OS                : {}.{}.{}\n",
        systemInfo.System.MajorVersion, 
        systemInfo.System.MinorVersion,
        systemInfo.System.BuildNumber);
    
    std::cout << std::format("  Product Type      : {}\n", systemInfo.System.ProductType);
    std::cout << std::format("  Suite Mask        : {}\n", systemInfo.System.SuiteMask);
    std::cout << std::format("  Processors        : {}\n", systemInfo.System.NumberOfProcessors);
    std::cout << std::format("  Platform ID       : {}\n", systemInfo.System.PlatformId);
    std::cout << std::format("  Processsor Level  : {}\n", systemInfo.System.ProcessorLevel);
    std::cout << std::format("  Processor Revision: {}\n", systemInfo.System.ProcessorRevision);

    // If the live recorder was used, print the recording information (see TTDLiveRecorder.h for more details)
    if (replayEngineView.GetRecordClientCount() > 0) {  
        std::cout << std::format("Record clients      : {:>11}\n", replayEngineView.GetRecordClientCount());
        std::cout << std::format("Custom events       : {:>11}\n", replayEngineView.GetCustomEventCount());
        std::cout << std::format("Activities          : {:>11}\n", replayEngineView.GetActivityCount());
        std::cout << std::format("Islands             : {:>11}\n", replayEngineView.GetIslandCount());
    }

    // Print the module list, showing when and where each module was loaded
    std::cout << std::format("Module loading activity:\n");
    for (ModuleInstance const& moduleInstance : ModuleInstances(&replayEngineView)) {
        std::wcout << std::format(L"[{}]  0x{:016X} 0x{:08X} {}\n",
            PositionRange(Position(moduleInstance.LoadTime), Position(moduleInstance.UnloadTime)),
            moduleInstance.pModule->Address,
            moduleInstance.pModule->Size,
            ModuleName(*moduleInstance.pModule));
    }

    // Print the exception list with position as the first column
    std::cout << std::format("Exceptions:\n");
    for (ExceptionEvent const& exceptionInfo : ExceptionEvents(&replayEngineView)) {
        PrintException(exceptionInfo);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: TraceInformation <trace file>\n";
        return 1;
    }

    std::filesystem::path traceFile = argv[1];

    // To get started, we need a replay engine to load the trace file
    auto [pOwnedReplayEngine, createResult] = MakeReplayEngine();
    if (createResult != 0 || pOwnedReplayEngine == nullptr)
    {
        std::cout << std::format("There was an issue creating a replay engine ({})\n", createResult);
        return -1;
    }

    // This allows the tool to get any messages from the initialization of the engine
     BasicErrorReporting errorReporting;
     pOwnedReplayEngine->RegisterDebugModeAndLogging(DebugModeType::None, &errorReporting);

    // Load the trace file into the replay engine
    if (!pOwnedReplayEngine->Initialize(traceFile.wstring().c_str()))
    {
        std::cout << std::format("Failed to initialize the engine\n");
        return -1;
    }

    // Process the trace
    try
    {
        ProcessTrace(*pOwnedReplayEngine);
    }
    catch (std::exception const& e)
    {
        std::cout << std::format("Error: {}\n", e.what());
        return -1;
    }

    return 0;
}
