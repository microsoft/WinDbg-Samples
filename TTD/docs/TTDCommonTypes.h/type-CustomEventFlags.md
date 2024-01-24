# `TTD::CustomEventFlags` type

32-bit flags type that modify aspects of a custom event.

It is implemented as a C++ `enum class` to make it a strong type.

## Flags

- `TTD::CustomEventFlags::Keyframe` (0x0000'0001) Denotes the custom event as a keyframe event.
  Keyframe events force the generation of a keyframe at the current position.
  This allows the custom event's position to be more acurately ordered with other common events,
  like observations of memory values.
  Note that generating keyframes may cost significant performance when recording, so use with care.

## Additional predefined values

|                               | Value        | Notes
|-                              |-             |-
| `TTD::CustomEventFlags::None` | 0            |
| `TTD::CustomEventFlags::All`  | `UINT32_MAX` |

## Supported operations

As a strongly typed flags value, `TTD::CustomEventFlags` supports defaulted comparisons and bit operations:

- Bit `&`, `|`, `^`, `~` with another `TTD::CustomEventFlags`.
- Comparison `==`, `!=`, `<`, `>`, `<=`, `>=` with another `TTD::CustomEventFlags`.

## Example of use

```C++
TTD::CustomEventFlags flags = TTD::CustomEventFlags::None;

if (makeItAKeyframe)
{
    flags |= TTD::CustomEventFlags::Keyframe;
}

pRecorder->AddCustomEvent(TTD::CustomEventType::Global, flags, nullptr, 0);
```
