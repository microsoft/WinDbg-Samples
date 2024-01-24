# `TTD::InstructionCount` type

64-bit unsigned integral type that represents a count of instructions executed in a program.

It is implemented as a C++ `enum class` to make it a strong type.

## Predefined values

|                                  | Value            | Notes
|-                                 |-                 |-
| `TTD::InstructionCount::Zero`    | 0                |
| `TTD::InstructionCount::Min`     | 0                |
| `TTD::InstructionCount::Max`     | `UINT64_MAX - 1` |
| `TTD::InstructionCount::Invalid` | `UINT64_MAX`     | Used as N/A or error marker

## Supported operations

As a strongly typed integral denoting a quantity, `TTD::InstructionCount` supports common operations:

- Arithmetic `+`, `-`, `*`, `/`, `%` with `uin64_t`.
- Arithmetic `+`, `-` with another `TTD::InstructionCount`.
- Comparison `==`, `!=`, `<`, `>`, `<=`, `>=` with `uin64_t`.
- Comparison `==`, `!=`, `<`, `>`, `<=`, `>=` with another `TTD::InstructionCount`.

## Example of use

```C++
static constexpr TTD::InstructionCount ThrottleAfterInstructions{ 100'000 };

pRecorder->StartRecordingCurrentThread(TTD::ActivityId{ 1 }, ThrottleAfterInstructions, nullptr, 0);

DoSomething();

TTD::InstructionCount instructionsRecorded = pRecorder->StopRecordingCurrentThread();
if (instructionsRecorded >= ThrottleAfterInstructions)
{
    // Apparently this island was ended by the throttle.
}
```
