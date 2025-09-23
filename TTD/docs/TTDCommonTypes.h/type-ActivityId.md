# `TTD::ActivityId` type

32-bit unsigned integral type that represents an opaque ID defined by some recording client.

It is implemented as a C++ `enum class` to make it a strong type.

## Predefined values

|                         | Value        | Notes
|-                        |-             |-
| `TTD::ActivityId::Zero` | 0            | Used as N/A or error marker
| `TTD::ActivityId::Min`  | 1            |
| `TTD::ActivityId::Max`  | `UINT32_MAX` |

## Supported operations

As a strongly typed opaque value, `TTD::ActivityId` only supports defaulted comparisons:

- Comparison `==`, `!=`, `<`, `>`, `<=`, `>=` with another `TTD::ActivityId`.

## Example of use

```C++
static constexpr TTD::ActivityId MeaningOfLifeActivity{ 42 };

pRecorder->StartRecordingCurrentThread(MeaningOfLifeActivity, TTD::InstructionCount::Invalid, nullptr, 0);
```
