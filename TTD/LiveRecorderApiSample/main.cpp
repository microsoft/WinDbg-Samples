#include <Windows.h>
#include <wrl.h>

#include <thread>
#include <future>
#include <filesystem>
#include <format>
#include <iostream>
#include <random>
#include <span>

#include <TTD/TTDLiveRecorder.h>

#pragma comment(lib, "TTDLiveRecorder")

// {6DA58208-3BF5-4B80-A711-781098BC4445}
static constexpr GUID ClientGuid = { 0x6da58208, 0x3bf5, 0x4b80, { 0xa7, 0x11, 0x78, 0x10, 0x98, 0xbc, 0x44, 0x45 } };

// We define what the activity IDs mean.
constexpr TTD::ActivityId StdSortActivity          { 1 };   // Call a std::sort function.
constexpr TTD::ActivityId MultithreadedSortActivity{ 2 };   // A multithreaded sort.

// We wish to report recording states by name.
char const* GetStateName(TTD::ThreadRecordingState state)
{
    switch (state)
    {
    case TTD::ThreadRecordingState::NotStarted: return "NotStarted";
    case TTD::ThreadRecordingState::Paused    : return "Paused";
    case TTD::ThreadRecordingState::Recording : return "Recording";
    case TTD::ThreadRecordingState::Throttled : return "Throttled";
    default                                   : return "<unknown state>";
    }
}

// Multithreaded sort function.
// This algorithm splits the sorted range in two and runs one of the two pieces asynchronously.
// It then merges the two pieces together, in-place.
// it does this recursively: each of the two pieces is in turn split,
// down to a reasonably (arbitrary) small size which is then sorted using std::sort.
void MultithreadedSort(TTD::ILiveRecorder* pRecorder, std::span<int> span)
{
    if (span.size() <= 100)
    {
        std::ranges::sort(span);
    }
    else
    {
        auto const midPoint = span.size() / 2;
        auto const loSpan = span.subspan(0, midPoint);
        auto const hiSpan = span.subspan(midPoint);

        auto const hiSpanFuture = std::async([pRecorder, hiSpan]()
            {
                // Asynchronous sorting of the hi span half.
                // This gets to run on a separate thread,
                // so in order to record we need to do it explicitly.
                pRecorder->StartRecordingCurrentThread(MultithreadedSortActivity, TTD::InstructionCount::Max);
                MultithreadedSort(pRecorder, hiSpan);
                pRecorder->StopRecordingCurrentThread();
            });

        // While the hi span half is being sorted asynchronously,
        // we will sort the low span half.
        MultithreadedSort(pRecorder, loSpan);

        // Before merging, we ensure the hi span half sort is complete.
        hiSpanFuture.wait();

        // Merge the two halves into a single sorted span.
        std::ranges::inplace_merge(span, span.begin() + midPoint);
    }
}

int wmain(int argc, wchar_t const* const* argv)
{
    std::default_random_engine rand{ std::random_device{}() };
    std::uniform_int randInt{};

    std::vector<int> randomSequence;
    randomSequence.reserve(10000);
    for (size_t i = 0; i < 10000; ++i)
    {
        randomSequence.push_back(randInt(rand));
    }

    std::filesystem::path binDir = std::filesystem::absolute(argv[0]).parent_path();

    // In order to record ourselves we will launch our own copy of TTD.exe.
    // We will then wait for it to signal that the process is ready to record.

    // We use a simple named event as the signal.
    std::wstring eventName = L"LiveRecorderApiSampleRecordingStarted";
    HANDLE recordingStartedEvent = CreateEventW(nullptr, FALSE, FALSE, eventName.c_str());
    if (recordingStartedEvent == nullptr)
    {
        std::wcerr << std::format(L"Couldn't create the '{}' named event. Error: {}\n", eventName.c_str(), GetLastError());
        return 1;
    }

    // We need to run TTD.exe using ShellExecuteW, so we can request elevation.
    auto const arguments = std::format(LR"(-out {} -attach {} -onInitCompleteEvent {} -recordMode manual)",
        binDir.native(),
        GetCurrentProcessId(),
        eventName
    );
    auto ttdResult = reinterpret_cast<INT_PTR>(ShellExecuteW(
        nullptr,
        L"runas",
        //L"TTD.exe",
        (binDir / L"TTD" / L"TTD.exe").c_str(),
        arguments.c_str(),
        nullptr,
        SW_SHOWNORMAL
    ));
    if (ttdResult <= 32)
    {
        DWORD extendedError = GetLastError();
        if (ttdResult == SE_ERR_ACCESSDENIED && extendedError == ERROR_CANCELLED)
        {
            std::cerr << "Elevation request was denied by the user so we can't proceed.\n";
        }
        else
        {
            std::cerr << std::format("Couldn't run TTD.exe. Error: {}, extended error: {}\n", ttdResult, extendedError);
        }
        return 1;
    }

    if (WaitForSingleObject(recordingStartedEvent, INFINITE) != WAIT_OBJECT_0)
    {
        std::cerr << "Error in the wait\n";
        return 1;
    }

    // TTD is now installed in the process and ready to record.
    Microsoft::WRL::ComPtr<TTD::ILiveRecorder> const pRecorder{ TTD::MakeLiveRecorder(ClientGuid, "Hello, there!") };
    if (pRecorder == nullptr)
    {
        std::cerr << "Failed to get the recorder interface!\n";
        return 2;
    }

    // We started TTD in manual mode, so nothing should be recording at this point.
    if (auto const state = pRecorder->GetState(); state != TTD::ThreadRecordingState::NotStarted)
    {
        std::cerr << std::format("main: The current thread should not be recording yet! State is {}\n", GetStateName(state));
        return 2;
    }

    // Record a simple single-threaded sort algorithm.
    // The sorting is recorded as a single island with its own activity ID.
    auto stdSortBuffer = randomSequence;
    pRecorder->StartRecordingCurrentThread(StdSortActivity, TTD::InstructionCount::Max);
    std::ranges::sort(stdSortBuffer);
    pRecorder->StopRecordingCurrentThread();

    // Verify that the sort worked as expected while being recorded.
    if (!std::ranges::is_sorted(stdSortBuffer))
    {
        std::cerr << "main: std::sort didn't work right.\n";
        return 2;
    }

    // Record all the pieces of a simple multithreaded sort algorithm.
    // The sorting of all the asynchronous pieces is recorded in the trace,
    // as islands belonging to the same activity.
    auto multithreadedSortBuffer = randomSequence;
    pRecorder->StartRecordingCurrentThread(MultithreadedSortActivity, TTD::InstructionCount::Max);
    MultithreadedSort(pRecorder.Get(), multithreadedSortBuffer);
    pRecorder->StopRecordingCurrentThread();

    // Verify that the multithreaded sort worked as expected while being recorded.
    if (!std::ranges::is_sorted(multithreadedSortBuffer))
    {
        std::cerr << "main: Multithreaded sort didn't work right.\n";
        return 2;
    }

    // Obtain and print the full path to the TTD trace file that is being recorded.
    wchar_t traceFilename[MAX_PATH];
    pRecorder->GetFileName(traceFilename, std::size(traceFilename));
    traceFilename[MAX_PATH - 1] = L'\0';
    std::wcout << std::format(L"Trace file: {}\n", traceFilename);

    // Close the recorder API, supplying a user data (in this case, just a short string).
    pRecorder->Close("Adios!");

    std::cout << "All done!\n";
}
