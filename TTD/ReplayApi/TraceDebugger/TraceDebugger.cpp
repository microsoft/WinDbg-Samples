// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// TraceDebugger - A simple mini debugger for analyzing TTD trace files.
//
// This sample demonstrates how to use the TTD Replay Engine to:
// * Inspect and display register values
// * Inspect and display memory contents
// * Single step forwards and backwards
// * Navigate to a specific position
// * Set a watchpoint on a memory range and move to the previous/next
//   position that triggers the watchpoint

// The TTD Replay headers include a replaceable assert macro that can be used to check conditions during replay. If
// one is not defined, it defaults to no assertion checks. This sample will use the standard assert from the C runtime.
#include <assert.h>
#define DBG_ASSERT(cond) assert(cond)

// Include the necessary headers for the TTD Replay Engine. The Formatters.h header is not required for the replay engine itself,
// but it provides custom formatters for the TTD types to work with std::format, which is used in this sample.
// To support examining and displaying register values, we include the IReplayEngineRegisters.h and RegisterNameMapping.h headers.
#include <Formatters.h>
#include <ReplayHelpers.h>
#include <RegisterNameMapping.h>
#include "Parsers.h"

#include <TTD/IReplayEngineStl.h>
#include <TTD/IReplayEngineRegisters.h>
#include <TTD/ErrorReporting.h>

// Standard C++ headers used by this sample
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <variant>

// Make the sample code easier to read by bringing the TTD namespaces into scope.
using namespace TTD;
using namespace Replay;

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

// Overload helper template for std::visit
template<class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template<class... Ts>
overload(Ts...) -> overload<Ts...>;

// UnifiedRegisterContext wraps a RegisterContext blob with architecture-specific pointers to the
// correct register context. Use this to access the registers in a cross-platform way.

using UnifiedRegisterContext = std::variant<
    AMD64_CONTEXT const*,
    X86_NT5_CONTEXT const*,
    ARM64_CONTEXT const*
>;

static UnifiedRegisterContext GetUnifiedRegisterContext(
    RegisterContext const& registersContext,
    ProcessorArchitecture architecture
)
{
    switch (architecture)
    {
        case ProcessorArchitecture::x86:
            return reinterpret_cast<X86_NT5_CONTEXT const*>(&registersContext);
        case ProcessorArchitecture::x64:
            return reinterpret_cast<AMD64_CONTEXT const*>(&registersContext);
        case ProcessorArchitecture::Arm64:
            return reinterpret_cast<ARM64_CONTEXT const*>(&registersContext);
        default:
            throw std::runtime_error("Unsupported architecture");
    }
}

// Command handler for 'r' command (display registers)
static bool DbgRegisters(ICursorView& cursor, std::string_view line = {})
{
    // Extract the register context from the cursor and establish the unified register context
    RegisterContext const registersBuffer = cursor.GetCrossPlatformContext();
    ProcessorArchitecture const architecture = GetGuestArchitecture(cursor);
    UnifiedRegisterContext const unifiedContext = GetUnifiedRegisterContext(registersBuffer, architecture);

    // If no specific register is requested, print general information about the current state
    auto const regString = ExtractFirstWord(line);
    if (regString.empty())
    {
        std::cout << std::format("Position = {}\n", cursor.GetPosition());
        std::cout << std::format("UTID = {}\n", cursor.GetThreadInfo().UniqueId);
        std::cout << std::format("RIP = 0x{:X}\n", cursor.GetProgramCounter());
        std::cout << std::format("RSP = 0x{:X}\n", cursor.GetStackPointer());

        // Print flags based on the architecture
        std::visit(overload{
            [](AMD64_CONTEXT const* registers) {
                std::cout << std::format("EFLAGS = 0x{:X}\n", registers->EFlags);
            },
            [](X86_NT5_CONTEXT const* registers) {
                std::cout << std::format("EFLAGS = 0x{:X}\n", registers->EFlags);
            },
            [](ARM64_CONTEXT const* registers) {
                std::cout << std::format("CPSR = 0x{:X}\n", registers->Cpsr);
            }
            }, unifiedContext);

        return true;
    }

    // Find the appropriate register name mapping based on the architecture.
    RegisterNameSpan registerNameMap = GetRegisterNameToContextMap(architecture);
    auto registerBegin = registerNameMap.begin();
    auto registerEnd = registerNameMap.end();

    // If a specific register is requested, find its position in the map.
    if (regString != "*")
    {
        std::wstring regWString(regString.begin(), regString.end());
        registerBegin = GetRegisterContextPosition(architecture, regWString);
        registerEnd = registerBegin;
        if (registerEnd != registerNameMap.end())
        {
            ++registerEnd;
        }
    }

    // If the register is not found, print an error message.
    if (registerBegin == registerEnd)
    {
        std::cout << std::format("Invalid register '{}'\n", std::string_view(regString.data(), regString.size()));
        return true;
    }

    // Print the register values.
    for (auto it = registerBegin; it != registerEnd; ++it)
    {
        // Do not print aliased registers if all registers are requested. An aliased register is a register
        // that is a subset of another register (e.g., 'al' is an alias for 'rax' in x64).
        if (regString == "*" && IsAliasedRegister(it->second))
        {
            continue;
        }

        // Locate the register data in the context structure. The largest register size is 512 bits (64 bytes),
        // so use GetDataString<64>() to format the data.
        auto const registerData = std::span{
            reinterpret_cast<uint8_t const*>(&registersBuffer) + it->second.Offset,
            static_cast<size_t>(it->second.Size)
        };
        std::wcout << std::format(L"{:>{}} = {}\n", it->first, static_cast<int>(it->first.size()), GetDataString<64>(registerData));
    }
    return true;
}

// Command handler for 't' command (step forward)
static bool DbgStepForward(ICursorView& cursor, std::string_view = {})
{
    cursor.ReplayForward(StepCount{ 1 });

    return DbgRegisters(cursor);
}

// Command handler for 't-' command (step backward)
static bool DbgStepBackward(ICursorView& cursor, std::string_view = {})
{
    cursor.ReplayBackward(StepCount{ 1 });

    return DbgRegisters(cursor);
}

// Command handler for 'tt' command (time travel)
static bool DbgTimeTravel(ICursorView& cursor, std::string_view line)
{
    Position pos = Position::Invalid;

    // If there is no sequence/step separator parse the number as a percentage
    if (line.find(':') == std::string::npos)
    {
        auto lineEnd = line.data() + line.size();
        float percent = 0.0;
        auto [ptr, ec] = std::from_chars(line.data(), lineEnd, percent);
        if (ec == std::errc{} && ptr == lineEnd)
        {
            PositionRange const lifetime = cursor.GetReplayEngine()->GetLifetime();
            pos = TryParsePositionFromPercentage(lifetime, percent);
        }
    }
    else
    {
        // Parse as sequence:step
        pos = TryParsePositionFromString(AsWString(line).c_str(), Position::Invalid);
    }

    if (pos == Position::Invalid)
    {
        std::cout << "Invalid position\n";
        return true;
    }

    std::cout << std::format("Traveling to position: {}\n", pos);
    cursor.SetPosition(pos);

    return DbgRegisters(cursor);
}

// Command handler for 'db/dw/etc' commands (display memory)
template < std::integral T >
static bool DbgMem(ICursorView& cursor, std::string_view line)
{
    auto const addressString = ExtractFirstWord(line);
    auto const addressOpt = TryParseUnsigned<uint64_t>(AsWString(addressString).c_str());
    if (!addressOpt)
    {
        std::cout << std::format("Invalid address\n");
        return true;
    }

    // In TTD, memory addresses are represented as GuestAddress. This strong type is used
    // to prevent accidental misuse of host addresses (i.e. addresses in the mini debugger process)
    // when interacting with the replay API.
    GuestAddress address{ *addressOpt };

    // Print 4 values of the specified type starting from the given address. QueryMemoryBuffer()
    // is used to read the memory at the specified address. It returns a MemoryBuffer which
    // indicates the size of the memory read and a pointer to the actual data. If the size returned
    // matches the size of value, we know value was successfully read.
    for (uint32_t i = 0; i < 4; ++i)
    {
        T value;
        MemoryBuffer const mem = cursor.QueryMemoryBuffer(address, BufferView{ &value, sizeof(value) });
        std::cout << std::format("0x{:016X} ", static_cast<uint64_t>(address));
        if (mem.Memory.Size == sizeof(value))
        {
            if constexpr (sizeof(value) == 1) std::cout << std::format("0x{:02X}\n", value);
            else if constexpr (sizeof(value) == 2) std::cout << std::format("0x{:04X}\n", value);
            else if constexpr (sizeof(value) == 4) std::cout << std::format("0x{:08X}\n", value);
            else if constexpr (sizeof(value) == 8) std::cout << std::format("0x{:016X}\n", value);
            else                                   std::cout << std::format("size?\n");
        }
        else
        {
            std::cout << std::format("????????\n");
        }
        address += sizeof(value);
    }

    return true;
}

// Common implementation for memory watchpoint commands
static bool DbgMemoryWatchpointImpl(ICursorView& cursor, std::string_view line, ReplayDirection direction)
{
    MemoryWatchpointData watchpoint{};

    // Parse the access mask, address, and size from the command line input.

    auto const accessString = ExtractFirstWord(line);
    if (accessString.empty())
    {
        std::cout << "usage: access address size\n";
        return true;
    }
    std::optional<DataAccessMask> const accessMask = ParseAccessMask(accessString);
    if (!accessMask)
    {
        std::cout << std::format("Invalid access mask '{}'\n", accessString);
        return true;
    }
    watchpoint.AccessMask = *accessMask;

    auto const addressString = ExtractFirstWord(line);
    if (addressString.empty())
    {
        std::cout << "usage: access address size\n";
        return true;
    }
    std::optional<uint64_t> const addressArgument = TryParseUnsigned<uint64_t>(addressString);
    if (addressArgument.value_or(0) == 0)
    {
        std::cout << std::format("Invalid address '{}'\n", addressString);
        return true;
    }
    watchpoint.Address = static_cast<GuestAddress>(addressArgument.value());


    auto const sizeString = ExtractFirstWord(line);
    if (sizeString.empty())
    {
        std::cout << "usage: access address size\n";
        return true;
    }
    std::optional<uint64_t> const sizeArgument = TryParseUnsigned<uint64_t>(sizeString);
    if (sizeArgument.value_or(0) == 0)
    {
        std::cout << std::format("Invalid size '{}'\n", sizeString);
        return true;
    }
    watchpoint.Size = sizeArgument.value();

    // Define a filter function to determine what to do when a memory watchpoint is hit.
    struct Filter
    {
        // For a memory watchpoint, the filter function is called with the watchpoint result and the thread view.
        // These parameters can be used to read memory / registers at the position the watchpoint was hit, if needed.
        bool operator()(Replay::ICursorView::MemoryWatchpointResult const&, Replay::IThreadView const*) const
        {
            // This filter function always returns true, meaning we want to stop replaying when the watchpoint is hit.
            return true;
        }

        // The progress function is called periodically during replay to indicate progress. It is not
        // guaranteed to be called, but if it is, it can be used to provide feedback to the user. If
        // it returns true, replay is interrupted. Note that this is an abstraction provided by FilteredWatchpointQuery():
        // the TTD Replay API only reports progress via the ICursorView::ReplayProgressCallback mechanism,
        // which only supplies the current position, not the percentage and does not support interruption.
        bool Progress(Position, double positionPercent) const
        {
            std::cout << std::format("Replaying ({:<6.2f}%)\r", positionPercent);

            // Do not stop replay
            return false;
        }
    } filter;

    // Determine the starting (Min) and ending (Max) positions for the replay range based on the current position
    // and the desired direction.
    PositionRange const replayRange = GetReplayRange(cursor, direction);

    // Create a new cursor to perform the query against and add the memory watchpoint to it.
    UniqueCursor filterCursor(cursor.GetReplayEngine()->NewCursor());
    filterCursor->AddMemoryWatchpoint(watchpoint);

    // Process the watchpoint query using the filter function and the specified replay range and direction.
    WatchpointQueryResult const result = FilteredWatchpointQuery(
        *filterCursor,
        replayRange,
        direction,
        filter);

    // Move to new line to skip over the replay progress output.
    std::cout << "\n";

    // Move the debugger cursor to the position where the watchpoint was hit.
    cursor.SetPosition(result.Position);

    return true;
}

// Command handler for 'ba' command (memory watchpoint forward)
static bool DbgMemoryWatchpointForward(ICursorView& cursor, std::string_view line)
{
    return DbgMemoryWatchpointImpl(cursor, line, ReplayDirection::Forward);
}

// Command handler for 'ba-' command (memory watchpoint backward)
static bool DbgMemoryWatchpointBackward(ICursorView& cursor, std::string_view line)
{
    return DbgMemoryWatchpointImpl(cursor, line, ReplayDirection::Backward);
}

// Command mapping for the debugger commands

using DbgCommandFunction = bool(ICursorView&, std::string_view);

struct DbgCommand
{
    DbgCommandFunction* Command;
    std::wstring        Usage;
};

static bool DbgUsage(ICursorView&, std::string_view);

std::pair<std::string, DbgCommand> const DbgCommandMap[]
{
    { "?"   , { &DbgUsage        ,                                    L"Show the list of commands available"                                      } },
    { "q"   , { [](ICursorView&, std::string_view) { return false; }, L"Quit"                                                                     } },
    { "r"   , { &DbgRegisters    ,                                    L"Registers"                                                                } },
    { "t"   , { &DbgStepForward  ,                                    L"Step forward"                                                             } },
    { "t-"  , { &DbgStepBackward ,                                    L"Step backward"                                                            } },
    { "tt"  , { &DbgTimeTravel   ,                                    L"Time travel to position / percentage in decimal format"                   } },
    { "db"  , { &DbgMem<uint8_t> ,                                    L"Show memory data as bytes"                                                } },
    { "dw"  , { &DbgMem<uint16_t>,                                    L"Show memory data as words"                                                } },
    { "dd"  , { &DbgMem<uint32_t>,                                    L"Show memory data as double words"                                         } },
    { "dq"  , { &DbgMem<uint64_t>,                                    L"Show memory data as quad qwords"                                          } },
    { "ba"  , { &DbgMemoryWatchpointForward ,                         L"Travel to next occurrence of watchpoint (accessmask address size)"        } },
    { "ba-" , { &DbgMemoryWatchpointBackward,                         L"Travel to previous occurrence of watchpoint (accessmask address size)"    } },
};

// Command handler for '?' command to display the list of available commands
static bool DbgUsage(ICursorView&, std::string_view)
{
    size_t const maxCommandLength = std::ranges::max_element(DbgCommandMap, {}, [](auto const& entry) { return entry.first.size(); })->first.size();
    for (auto const& [command, cmd] : DbgCommandMap)
    {
        std::wcout << std::format(L"{:>{}} - {}\n", AsWString(command), maxCommandLength, cmd.Usage);
    }
    std::cout << "\n";

    std::cout << "Valid accessmask characters:\n";
    std::cout << "  R - Read access\n";
    std::cout << "  O - Overwrite access - triggers before a write / mismatch, allowing the client to inspect the value before it is overwritten\n";
    std::cout << "  W - Write access\n";
    std::cout << "  E - Execute access\n";
    std::cout << "  C - CodeFetch access - aggregate code usage; the size and exact hits are implementation - dependent\n";
    std::cout << "  M - Data mismatch - the memory cache predicted the wrong value\n";
    std::cout << "  N - New data - First time seeing data at this address\n";
    std::cout << "  D - Redundant data - Data read from trace file matches memory cache\n";

    return true;
}

// This function presents information about the loaded trace file, to give a sense of
// how to use the replay API and the types of information contained in a trace file.
static void ProcessTrace(IReplayEngineView& replayEngineView) {
    // Establish a cursor for the mini debugger.
    UniqueCursor const pOwnedCursor{ replayEngineView.NewCursor() };
    if (pOwnedCursor == nullptr)
    {
        throw std::bad_alloc{};
    }

    // Set the initial position of the cursor to the beginning of the trace.
    pOwnedCursor->SetPosition(Position::Min);

    // Print general information about the start of the trace.
    DbgRegisters(*pOwnedCursor, "");

    std::map<std::string, DbgCommand> commandMap{
        std::begin(DbgCommandMap),
        std::end(DbgCommandMap),
    };

    // Remember the lifetime of the trace, which is used to determine the percentage into the trace.
    PositionRange const lifetime = replayEngineView.GetLifetime();

    // Debugger command loop.
    std::string lineString;
    for (;;)
    {
        // Print the debugger prompt
        double const progressPercent = GetProgressPercent(pOwnedCursor->GetPosition(), lifetime);
        std::cout << std::format("{} ({:>6.02f}%) - {}> ",
            pOwnedCursor->GetPosition(),
            progressPercent,
            pOwnedCursor->GetThreadInfo().UniqueId);

        // Read a line of input from the user and process it

        std::getline(std::cin, lineString);
        if (std::cin.eof())
        {
            break;
        }
        if (lineString.empty())
        {
            continue;
        }
        std::string_view line{ lineString };
        auto command = ExtractFirstWord(line);
        auto it = commandMap.find(std::string{ command });
        if (it != commandMap.end())
        {
            if (!it->second.Command(*pOwnedCursor, line))
            {
                break;
            }
        }
        else
        {
            std::cout << std::format("Unrecognized command '{}'\n", std::string_view(command.data(), command.size()));
        }
    }
}

int main(int argc, char* argv[])
{
    std::cout << "MiniDebugger - TTD Trace Analysis Tool\n";
    std::cout << "=====================================\n\n";

    if (argc < 2) {
        std::cerr << "Usage: MiniDebugger <trace file>\n";
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

    // This allows the tool to get any messages from the replay engine
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
