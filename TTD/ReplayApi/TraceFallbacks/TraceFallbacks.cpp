// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// TraceFallbacks - A tool to identify instructions that Time Travel Debugging (TTD) used fallbacks for during recording.
//
// TTD has hand-coded emulation for many CPU instructions to ensure accurate recording and replay of program execution. However,
// there are some instructions that TTD cannot emulate directly, either due to their complexity or because
// they interact with hardware in ways that cannot be captured. In these cases, TTD uses "fallbacks" to handle these instructions.
// This tool analyzes a TTD trace file and reports on the use of fallbacks during the recording.

// The TTD Replay headers include a replaceable assert macro that can be used to check conditions during replay. If
// one is not defined, it defaults to no assertion checks. This sample will use the standard assert from the C runtime.
#include <assert.h>
#define DBG_ASSERT(cond) assert(cond)

// Include the necessary headers for the TTD Replay Engine. The Formatters.h header is not required for the replay engine itself,
// but it provides custom formatters for the TTD types to work with std::format, which is used in this sample.
#include <Formatters.h>
#include <ReplayHelpers.h>
#include <TTD/IReplayEngineStl.h>
#include <TTD/ErrorReporting.h>

#include "Fallbacks.h"
#include "FallbackFile.h"
#include "InstructionDecoder.h"

#include <zydis/Zydis.h> // for instruction decoding

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_map>

// Make the sample code easier to read by bringing the TTD namespaces into scope.
using namespace TTD;
using namespace TTD::Replay;

// In-memory structure to hold command line arguments.
struct CommandLine
{
    std::optional<std::filesystem::path>    TraceFile;
    std::optional<std::filesystem::path>    InputFile;
    std::optional<std::filesystem::path>    OutputFile;
    bool                                    NormalizeInstructions{ false };
    bool                                    ShowInstructionCount{ false };
    uint32_t                                TopN{ 20 };
};

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

// Count the total number of instructions executed in the trace by replaying each thread's active time range.
static InstructionCount CountInstructions(IReplayEngineView& replayEngineView)
{
    UniqueCursor pOwnedCursor{ replayEngineView.NewCursor() };
    if (!pOwnedCursor)
    {
        throw std::runtime_error("Failed to create a replay engine cursor");
    }

    // The public API only supports getting instruction count for the current thread, so
    // we need to replay each thread separately.
    pOwnedCursor->SetReplayFlags(ReplayFlags::ReplayOnlyCurrentThread);

    InstructionCount totalInstructions = InstructionCount::Zero;
    size_t const threadCount = replayEngineView.GetThreadCount();
    for (size_t i = 0; i < threadCount; ++i)
    {
        ThreadInfo const& thread = replayEngineView.GetThreadList()[i];
        PositionRange activeTime = thread.ActiveTime;
        activeTime.Max.Steps += 1; // make the range inclusive of the last step
        std::cout << std::format("Processing thread {}/{} from {} to {}: UTID {} ...", i + 1, threadCount, activeTime.Min, activeTime.Max, static_cast<uint32_t>(thread.UniqueId));

        pOwnedCursor->SetPosition(activeTime.Min);
        ICursorView::ReplayResult const replayResult = pOwnedCursor->ReplayForward(activeTime.Max);

        std::cout << std::format(" {} instructions\n", static_cast<uint64_t>(replayResult.InstructionsExecuted));

        totalInstructions += replayResult.InstructionsExecuted;
    }

    return totalInstructions;
}

// Print fallback info to the console.
template <std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, FallbackInfo>
static std::tuple<uint64_t, uint64_t> PrintFallbackInfo(
    std::string caption,
    R&&         fallbackInfo,
    bool        normalize,
    uint32_t    limit)
{
    // Check if empty first
    if (std::ranges::empty(fallbackInfo))
    {
        return std::make_tuple(0, 0);
    }

    // Calculate the maximum instruction size from the range
    auto const maxInstructionSize = std::ranges::max(
        fallbackInfo | std::views::transform([](auto const& info) { return info.Instruction.size; })
    );

    // Calculate the maximum length of the instruction text for formatting
    auto const maxInstructionTextSize = std::ranges::max(
        fallbackInfo | std::views::transform([normalize](auto const& info) -> size_t {
            return normalize ? info.NormalizedInstruction.size() : info.DecodedInstruction.size();
        })
    );

    // Create views for items to show and remaining items
    auto itemsToShow = fallbackInfo | std::views::take(limit);
    auto remainingItems = fallbackInfo | std::views::drop(limit);

    // Print caption
    std::cout << std::format("\n{}:\n", caption);

    // Print the items to show
    uint64_t count = 0;
    uint64_t sum = 0;
    for (auto const& info : itemsToShow)
    {
        std::string const& decodedInstruction = normalize ? info.NormalizedInstruction : info.DecodedInstruction;

        std::cout << std::format("{:>10} : {:{}s}  {:{}s}  ({})\n",
            info.Count,
            TTD::GetBytesString<_countof(info.Instruction.bytes)>(info.Instruction.size, info.Instruction.bytes),
            maxInstructionSize * 3,
            decodedInstruction,
            maxInstructionTextSize,
            info.Position
        );

        ++count;
        sum += info.Count;
    }

    // Calculate remaining items using views
    uint64_t remainingCount = 0;
    uint64_t remainingSum = 0;
    for (auto const& info : remainingItems)
    {
        ++remainingCount;
        remainingSum += info.Count;
    }

    // Print summary of remaining items if any
    if (remainingCount > 0)
    {
        std::cout << std::format("  ... {} more rows totaling {} additional fallbacks\n", remainingCount, remainingSum);
    }

    // Update totals
    count += remainingCount;
    sum += remainingSum;

    // Print totals
    std::cout << std::format("{} unique, {} total\n", count, sum);

    return std::make_tuple(count, sum);
}

// Print fallback stats to the console.
// Deliberately take fallbackInfo by value so this function can transform it as needed
// Note: This code deliberately takes fallbackInfo by value so that it can be sorted as needed.
static void PrintFallbackStatsToConsole(std::vector<FallbackInfo> fallbackInfo, CommandLine const& commandLine)
{
    // Print reporting settings
    std::cout << std::format("\nReporting Settings:\n");
    if (commandLine.NormalizeInstructions)
    {
        std::cout << std::format("- Normalized Instructions\n");
    }
    if (commandLine.TopN == std::numeric_limits<uint32_t>::max())
    {
        std::cout << std::format("- Showing All Fallbacks\n");
    }
    else
    {
        std::cout << std::format("- Top {}\n", commandLine.TopN);
    }

    // Group instructions by normalized instruction if requested
    if (commandLine.NormalizeInstructions)
    {
        fallbackInfo = NormalizeFallbackStats(fallbackInfo);
    }

    // Sort fallbacks by descending count
    std::ranges::sort(fallbackInfo, std::greater<>{}, &FallbackInfo::Count);

    // Print full fallbacks
    auto [fullFallbackCount, fullFallbackSum] = PrintFallbackInfo(
        "Full Fallbacks by Instruction",
        fallbackInfo | std::views::filter([](auto const& i) { return i.Type == FallbackType::FullFallback; }),
        commandLine.NormalizeInstructions,
        commandLine.TopN
    );

    // Print synthetic fallbacks
    auto [syntheticFallbackCount, syntheticFallbackSum] = PrintFallbackInfo(
        "Synthetic Fallbacks by Instruction",
        fallbackInfo | std::views::filter([](auto const& i) { return i.Type == FallbackType::SyntheticInstruction; }),
        commandLine.NormalizeInstructions,
        commandLine.TopN
    );

    // Print overall totals
    uint64_t const totalUniqueCount = fullFallbackCount + syntheticFallbackCount;
    uint64_t const totalFallbackSum = fullFallbackSum + syntheticFallbackSum;
    std::cout << std::format("\nOverall Fallbacks: {} unique, {} total\n", totalUniqueCount, totalFallbackSum);
}

// Print fallback stats to a JSON file.
// Note: This code deliberately takes fallbackInfo by value so that it can be sorted as needed.
static void PrintFallbackStatsToFile(std::vector<FallbackInfo> fallbackInfo, std::filesystem::path const& outputFile)
{
    // Sort fallbacks by descending count
    std::ranges::sort(fallbackInfo, std::greater<>{}, &FallbackInfo::Count);

    std::error_code ec = WriteFallbackStatsFile(fallbackInfo, outputFile);
    if (ec)
    {
        throw std::runtime_error(std::format("Failed to write fallback stats to {}: {}", outputFile.string(), ec.message()));
    }

    std::cout << std::format("Fallback stats written to {}\n", outputFile.string());
}

// Print fallback stats from raw stats.
static void PrintFallbackStats(IReplayEngineView& replayEngineView, FallbackStatsMap const& stats, CommandLine const& commandLine)
{
    std::vector<FallbackInfo> fallbackInfo = ProcessFallbackStats(replayEngineView, stats);

    PrintFallbackStatsToConsole(fallbackInfo, commandLine);

    if (commandLine.OutputFile.has_value())
    {
        PrintFallbackStatsToFile(fallbackInfo, commandLine.OutputFile.value());
    }
}

static void ProcessTrace(IReplayEngineView& replayEngineView, CommandLine const& commandLine)
{
    UniqueCursor pOwnedCursor{ replayEngineView.NewCursor() };
    if (!pOwnedCursor)
    {
        throw std::runtime_error("Failed to create a replay engine cursor");
    }

    // Show instruction count if requested
    if (commandLine.ShowInstructionCount)
    {
        std::cout << "Processing trace for instruction count...\n";
        InstructionCount const totalInstructions = CountInstructions(replayEngineView);
        std::cout << std::format("{} instructions processed.\n", static_cast<uint64_t>(totalInstructions));
        std::cout << "\n";
    }

    std::cout << "Processing trace for fallback instructions...\n";
    FallbackStatsMap const fallbackStats = GatherRawFallbackInfo(replayEngineView);

    PrintFallbackStats(replayEngineView, fallbackStats, commandLine);
}

std::optional<CommandLine> TryParseCommandLine(int argc, char* argv[])
{
    if (argc < 2)
    {
        return {};
    }

    CommandLine cmdLine;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc)
        {
            cmdLine.OutputFile = argv[++i];
        }
        else if (arg == "-i" && i + 1 < argc)
        {
            cmdLine.InputFile = argv[++i];
        }
        else if (arg == "-normalize")
        {
            cmdLine.NormalizeInstructions = true;
        }
        else if (arg == "-instructioncount")
        {
            cmdLine.ShowInstructionCount = true;
        }
        else if (arg == "-all")
        {
            cmdLine.TopN = std::numeric_limits<uint32_t>::max();
        }
        else if (arg == "-top" && i + 1 < argc)
        {
            try
            {
                cmdLine.TopN = std::stoul(argv[++i]);
                if (cmdLine.TopN == 0)
                {
                    std::cerr << "Top N value must be greater than 0\n";
                    return {};
                }
            }
            catch (...)
            {
                std::cerr << "Invalid value for -top option\n";
                return {};
            }
        }
        else if (!cmdLine.TraceFile.has_value())
        {
            cmdLine.TraceFile = argv[i];
        }
        else
        {
            std::cerr << std::format("Unknown command line argument: {}\n", arg);
            return {};
        }
    }
    
    // Validate mutual exclusivity of input file and trace file
    if (cmdLine.InputFile.has_value() && cmdLine.TraceFile.has_value())
    {
        std::cerr << "Error: Cannot specify both input file (-i) and trace file\n";
        return {};
    }
    
    // Ensure either input file or trace file is specified
    if (!cmdLine.InputFile.has_value() && !cmdLine.TraceFile.has_value())
    {
        std::cerr << "Error: Must specify either input file (-i) or trace file\n";
        return {};
    }
    
    // Validate that -instructioncount is only used with trace file
    if (cmdLine.ShowInstructionCount && cmdLine.InputFile.has_value())
    {
        std::cerr << "Error: -instructioncount option can only be used with trace files, not input files\n";
        return {};
    }
    
    return cmdLine;
}

int main(int argc, char *argv[])
{
    std::optional<CommandLine> commandLine = TryParseCommandLine(argc, argv);
    if (!commandLine.has_value())
    {
        std::cerr << "Usage: TraceFallbacks [reporting options] <trace file> [-o <json file>]\n";
        std::cerr << "       TraceFallbacks [reporting options] -i <json file>\n";
        std::cerr << "Options:\n";
        std::cerr << "       -i <input file>   - Read fallback data from json file instead of trace file\n";
        std::cerr << "       -o <output file>  - Write all fallback data to json\n";
        std::cerr << "Reporting Options:\n";
        std::cerr << "       -instructioncount - Show instruction counts for each thread and the entire trace\n";
        std::cerr << "       -normalize        - Normalize instructions\n";
        std::cerr << "       -top N            - Show top N fallbacks (default: 20)\n";
        std::cerr << "       -all              - Show all fallbacks (equivalent to -top max)\n";
        return 1;
    }

    // Check if we're in input file mode
    if (commandLine->InputFile.has_value())
    {
        // Read fallback stats from file and display
        std::vector<FallbackInfo> fallbackInfo;
        std::error_code ec = ReadFallbackStatsFile(commandLine->InputFile.value(), fallbackInfo);
        if (ec)
        {
            std::cout << std::format("Failed to read fallback stats from {}: {}\n", 
                commandLine->InputFile.value().string(), ec.message());
            return -1;
        }
        
        std::cout << std::format("Read {} fallback entries from {}\n\n", 
            fallbackInfo.size(), commandLine->InputFile.value().string());
        
        // Display the analysis
        PrintFallbackStatsToConsole(fallbackInfo, commandLine.value());
        
        // Optionally write to output file
        if (commandLine->OutputFile.has_value())
        {
            PrintFallbackStatsToFile(fallbackInfo, commandLine->OutputFile.value());
        }
        
        return 0;
    }

    // Original trace file processing mode
    std::filesystem::path traceFile = commandLine->TraceFile.value();

    // Start timing the operation
    auto const startTime = std::chrono::high_resolution_clock::now();
    
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
        ProcessTrace(*pOwnedReplayEngine, commandLine.value());
    }
    catch (std::exception const& e)
    {
        std::cout << std::format("Error: {}\n", e.what());
        return -1;
    }

    // Finish timing the operation and report
    auto const endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsedSeconds = endTime - startTime;
    std::cout << std::format("\nTotal execution time: {:.3f} seconds\n", elapsedSeconds.count());

    return 0;
}
