# Time Travel Debugging (TTD) programming interface

This documentation describes all the packages and APIs offered to interact with TTD's components.

## Links

- [Concepts](Concepts.md) — overview of TTD-specific terminology (positions, segments, guest addresses, etc.)
- [Microsoft.TimeTravelDebugging.Apis](Microsoft.TimeTravelDebugging.Apis.md) NuGet package
- [TTD programming samples and docs](https://github.com/microsoft/WinDbg-Samples/tree/HEAD/TTD) repository
- [TTD user documentation](https://aka.ms/ttd) including installation instructions, use cases and command-line options.

## Projects using TTD programming interface

- [WindbgTTD-TrackReg](https://github.com/CoolGuy-0000/WindbgTTD-TrackReg) is a debugger extension that can track a value backwards in time, to find the value's origin. For instance, to try and find the allocation of some object from the point where it's being deallocated:
```
Time Travel Position: 4B17B:1D4D
TTDReplay!std::_Deallocate<16>:
  232 00007ffc`ac6c6ac3 4883ec28        sub     rsp,28h
0:000> !timetrack rcx
[0] 4B17B:1D4D 		Tracking origin of rcx
  [1] TTDReplay!TTD::Replay::MemoryDb::Entry::~Entry+0x9 4B17B:1D46 		mov rcx, [rcx+0x20]
    [2] TTDReplay!std::vector<TTD::Replay::MemoryMap::Entry,std::allocator<TTD::Replay::MemoryMap::Entry> >::_Resize<std::_Value_init_tag>+0x111 1840:367 		mov [rsi], r14
      [3] TTDReplay!std::vector<TTD::Replay::MemoryMap::Entry,std::allocator<TTD::Replay::MemoryMap::Entry> >::_Resize<std::_Value_init_tag>+0xCE 1840:33E 		mov r14, rax
        [4] ntdll!RtlpAllocateNTHeapInternal+0x1C8 1840:320 		mov rax, rbp
          [5] ntdll!RtlpAllocateNTHeapInternal+0xE4 1840:309 		mov rbp, rax
            [6] ntdll!RtlpLowFragHeapAllocFromContext+0x782 1840:2FE 		lea rax, [rbx+0x10]
            		Origin: Address Calculation (LEA).
```
