# TTD/TTDCommonTypes.h

This header is included by other TTD headers to reference types and other definitions that are used by one or more interfaces.

It resides in the [Microsoft.TimeTravelDebugging.Apis](../Microsoft.TimeTravelDebugging.Apis.md) NuGet package.

## Type definitions

- [`TTD::ActivityId`](type-ActivityId.md)
- [`TTD::CustomEventFlags`](type-CustomEventFlags.md)
- [`TTD::CustomEventType`](enum-CustomEventType.md)
- [`TTD::InstructionCount`](type-InstructionCount.md)
- [`TTD::ThreadRecordingState`](enum-ThreadRecordingState.md)
- [`TTD::ThrottleState`](struct-ThrottleState.md)

## Constants

- `TTD::MaxUserDataSizeInBytes` (`size_t`) (16KB) This is the largest buffer acceptable as user data.
  This size represents a compromise between allowing enough information for most uses and keeping the data
  represented in the file in a simple manner.
