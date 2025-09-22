# TimingInfo Structure

Process and system timing information captured during TTD recording sessions, providing temporal context for recorded execution traces.

## Definition

```cpp
struct TimingInfo
{
    // GetSystemTime() function timing information.
    uint64_t               SystemTime;
    // GetProcessTimes() function timing information.
    uint64_t               ProcessCreateTime;
    uint64_t               ProcessUserTime;
    uint64_t               ProcessKernelTime;
    // Currently undefined.
    uint64_t               SystemUpTime;
};
```

## Fields

- `SystemTime` - System time when recording started (from `GetSystemTime()`)
- `ProcessCreateTime` - Process creation time (from `GetProcessTimes()`)
- `ProcessUserTime` - Total user-mode execution time (from `GetProcessTimes()`)
- `ProcessKernelTime` - Total kernel-mode execution time (from `GetProcessTimes()`)
- `SystemUpTime` - System uptime at recording start (currently undefined)

## Time Format

All timing values are stored as Windows FILETIME format - 64-bit values representing the number of 100-nanosecond intervals since January 1, 1601 UTC.

## Important Notes

- All times are in Windows FILETIME format (100-nanosecond intervals since January 1, 1601 UTC)
- Process times represent cumulative CPU time, not wall-clock time
- CPU utilization calculation requires both execution time and process lifetime
- SystemUpTime field is currently undefined and may be zero
- Time comparisons should account for potential clock differences between systems
- Process creation time may precede the recording start time
- Kernel time includes time spent in system calls and kernel-mode operations
- User time represents application-level execution time

## See Also

- [`SystemInfo`](struct-SystemInfo.md) - Contains TimingInfo as part of comprehensive system information
- [`TBufferView`](template-TBufferView.md) - Buffer views that may contain timing data
- [`Position`](../IReplayEngine.h/struct-Position.md) - Timeline positions that relate to timing
- [`SequenceId`](enum-SequenceId.md) - Sequence identification in timeline context
