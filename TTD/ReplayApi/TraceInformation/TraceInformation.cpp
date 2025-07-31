#include <TTD/IReplayEngineStl.h>

#include <format>
#include <iostream>

using namespace TTD::Replay;

// Custom formatter for TTD::GuestAddress to work with std::format
template <>
struct std::formatter<TTD::GuestAddress, wchar_t> : std::formatter<uint64_t, wchar_t> {
    template <typename FormatContext>
    auto format(const TTD::GuestAddress& addr, FormatContext& ctx) const {
        return std::formatter<uint64_t, wchar_t>::format(static_cast<uint64_t>(addr), ctx);
    }
};

// Custom formatter for TTD::Replay::UniqueThreadId to work with std::format
template <>
struct std::formatter<UniqueThreadId, wchar_t> : std::formatter<uint32_t, wchar_t> {
    template <typename FormatContext>
    auto format(const UniqueThreadId& tid, FormatContext& ctx) const {
        return std::formatter<uint32_t, wchar_t>::format(static_cast<uint32_t>(tid), ctx);
    }
};

// Custom formatter for TTD::Replay::Position to work with std::format
template <>
struct std::formatter<TTD::Replay::Position, wchar_t> : std::formatter<uint64_t, wchar_t> {
    template <typename FormatContext>
    auto format(const TTD::Replay::Position& pos, FormatContext& ctx) const {
        return std::formatter<std::wstring, wchar_t>::format(
            std::format(L"{}:{}", pos.Sequence, pos.Steps), ctx);
    }
};

// Custom formatter for TTD::Replay::PositionRange to work with std::format
template <>
struct std::formatter<TTD::Replay::PositionRange, wchar_t> : std::formatter<std::wstring, wchar_t> {
    template <typename FormatContext>
    auto format(const TTD::Replay::PositionRange& range, FormatContext& ctx) const {
        return std::formatter<std::wstring, wchar_t>::format(
            std::format(L"{}-{}", range.Min, range.Max), ctx);
    }
};

void PrintException(ExceptionEvent const& exception)
{
    std::wcout << std::format(
        L"UTID: {:<6} Code: 0x{:08X} Flags: 0x{:04X} RecordAddress: 0x{:08X} PC: 0x{:08X} Parameters: ",
        exception.pThreadInfo->UniqueId,
        exception.Code,
        exception.Flags,
        exception.RecordAddress,
        exception.ProgramCounter
    );

    if (exception.ParameterCount == 0)
    {
        std::wcout << std::format(L"None\n");
    }
    else
    {
        for (size_t i = 0; i < exception.ParameterCount; ++i)
        {
            if (i == 0)
            {
                std::wcout << std::format(L"(0x{:X}", exception.Parameters[i]);
            }
            else
            {
                std::wcout << std::format(L", 0x{:X}", exception.Parameters[i]);
            }
        }
        std::wcout << std::format(L")\n");
    }
}


void ProcessTrace(IReplayEngine const& replayEngine, TraceInfo const& traceInfo) {
    std::wcout << std::format(L"Trace file          : {}\n", traceInfo.FileInfo.pFileName);

    SystemInfo const& systemInfo = replayEngine.GetSystemInfo();  

    std::wcout << std::format(L"Version             : 1.{:02}.{:02}\n", 
        systemInfo.MajorVersion, systemInfo.MinorVersion);
    
    std::wcout << std::format(L"Index               : {}\n", GetIndexStatusName(replayEngine.GetIndexStatus()));
    std::wcout << std::format(L"PID                 : 0x{:04X}\n", systemInfo.ProcessId);
    std::wcout << std::format(L"PEB                 : 0x{:X}\n", replayEngine.GetPebAddress());
    
    std::wcout << std::format(L"Lifetime            : {}\n", replayEngine.GetLifetime());
    
    std::wcout << std::format(L"Threads             : {:>11}\n", replayEngine.GetThreadCount());
    std::wcout << std::format(L"Modules             : {:>11}\n", replayEngine.GetModuleCount());
    std::wcout << std::format(L"ModuleInstances     : {:>11}\n", replayEngine.GetModuleInstanceCount());
    std::wcout << std::format(L"Exceptions          : {:>11}\n", replayEngine.GetExceptionEventCount());
    std::wcout << std::format(L"Keyframes           : {:>11}\n", replayEngine.GetKeyframeCount());
    
    std::wcout << std::format(L"System              :\n");
    
    std::wcout << std::format(L"  OS                : {}.{}.{}\n",
        systemInfo.System.MajorVersion, 
        systemInfo.System.MinorVersion,
        systemInfo.System.BuildNumber);
    
    std::wcout << std::format(L"ModuleInstances     : {:>11}\n", replayEngine.GetModuleInstanceCount());
    std::wcout << std::format(L"  Product Type      : {}\n", systemInfo.System.ProductType);
    std::wcout << std::format(L"  Suite Mask        : {}\n", systemInfo.System.SuiteMask);
    std::wcout << std::format(L"  Processors        : {}\n", systemInfo.System.NumberOfProcessors);
    std::wcout << std::format(L"  Platform ID       : {}\n", systemInfo.System.PlatformId);
    std::wcout << std::format(L"  Processsor Level  : {}\n", systemInfo.System.ProcessorLevel);
    std::wcout << std::format(L"  Processor Revision: {}\n", systemInfo.System.ProcessorRevision);

    if (replayEngine.GetRecordClientCount() > 0) {  
        std::wcout << std::format(L"Record clients      : {:>11}\n", replayEngine.GetRecordClientCount());
        std::wcout << std::format(L"Custom events       : {:>11}\n", replayEngine.GetCustomEventCount());
        std::wcout << std::format(L"Activities          : {:>11}\n", replayEngine.GetActivityCount());
        std::wcout << std::format(L"Islands             : {:>11}\n", replayEngine.GetIslandCount());
    }

    // Print the module list with position as the first column
    std::wcout << std::format(L"Modules:\n");
    for (size_t i = 0; i < replayEngine.GetModuleCount(); ++i) {
        Module const& moduleInfo = replayEngine.GetModuleList()[i];
        std::wcout << std::format(L"  0x{:016X} 0x{:08X} {}\n",
            moduleInfo.Address,
            moduleInfo.Size,
            std::wstring_view(moduleInfo.pName, moduleInfo.NameLength));
    }

    // Print the exception list with position as the first column
    std::wcout << std::format(L"Exceptions:\n");
    for (size_t i = 0; i < replayEngine.GetExceptionEventCount(); ++i) {
        ExceptionEvent const& exceptionInfo = replayEngine.GetExceptionEventList()[i];
        PrintException(exceptionInfo);
    }
}

void ProcessTraceList(ITraceList& traceList)
{
    if (traceList.GetTraceCount() == 0)
    {
        throw std::exception("no usable traces found");
    }

    for (size_t traceIndex = 0; traceIndex < traceList.GetTraceCount(); ++traceIndex)
    {
        TraceInfo const& traceInfo = traceList.GetTraceInfo(traceIndex);
        UniqueReplayEngine pReplayEngine{ traceList.OpenTrace(traceIndex) };
        if (pReplayEngine == nullptr)
        {
            throw std::exception("failed to open trace");
        }

        ProcessTrace(*pReplayEngine, traceInfo);
    }
}

int wmain(int argc, wchar_t *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: TraceInformation <trace file>\n";
        return 1;
    }

    std::wstring traceFile = argv[1];

    auto [pOwnedTraceList, createResult] = MakeTraceList();
    if (createResult != 0 || pOwnedTraceList == nullptr)
    {
        std::wcout << std::format(L"There was an issue creating a replay engine ({})\n", createResult);
        return -1;
    }

    // This allows the tool to get any messages from the initialization of the engine
    // pOwnedTraceList->RegisterDebugModeAndLogging(DebugModeType::None, &errorReporting);

    if (!pOwnedTraceList->LoadFile(traceFile.c_str(), nullptr))
    {
        std::wcout << std::format(L"Failed to initialize the engine\n");
        return -1;
    }

    try
    {
        ProcessTraceList(*pOwnedTraceList);
    }
    catch (std::exception const& e)
    {
        std::cout << std::format("Error: {}\n", e.what());
        return -1;
    }

    return 0;
}
