# TTD::ILiveRecorder::IsOpen method

Method of the [`TTD::ILiveRecorder`](interface-ILiveRecorder.md) interface.

Allows the caller to know whether the [`TTD::ILiveRecorder::Close`](ILiveRecorder_Close.md) method has already been called.

```C++
bool TTD::ILiveRecorder::IsOpen() const noexcept;
```

## Return value

Returns true if the recorder access object is active, hasn't yet been closed.

## Correct use

This method may be freely called from multiple threads simultaneously.

This method has no use restrictions.

## Example of use

```C++
if (!pRecorder->IsOpen())
{
    // The recorder object is closed, so we release it now.
    pRecorder->Release();
    pRecorder = nullptr;
}
```
