# `TTD::CustomEventType` enumeration

8-bit enumeration type that indicates the type of custom event to record.

It is implemented as a C++ `enum class` to make it a strong type.

## Values

- `TTD::CustomEventType::ThreadLocal` (0) Denotes the custom event as belonging to the thread
  that is recording it. Note that if the calling thread is not currently recording,
  thread-local events are still recorded, but as global events.
- `TTD::CustomEventType::Global` (1) Denotes the custom event as global to the process and not belonging
  to the thread that is recording it.

## Supported operations

As a strongly typed enumeration value, `TTD::CustomEventType` supports defaulted comparisons:

- Comparison `==`, `!=`, `<`, `>`, `<=`, `>=` with another `TTD::CustomEventType`.

## Example of use

```C++
TTD::CustomEventType type = TTD::CustomEventType::ThreadLocal;

if (makeItGlobal)
{
    type = TTD::CustomEventType::Global;
}

pRecorder->AddCustomEvent(type, TTD::CustomEventFlags::None, nullptr, 0);
```
