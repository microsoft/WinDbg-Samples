# TraceFallbacks

This sample demonstrates the Replay API FallbackCallback functionality, and acts as a tool for reporting potential performance issues to the TTD team.

Dependencies are managed using [vcpkg](https://github.com/microsoft/vcpkg).

---

## Understanding Fallbacks

### What Are Fallbacks?

Modern CPUs support over 1,000 unique instructions, with new ones added with each chip release. To avoid writing emulation code for every possible instruction, TTD uses a **fallback mechanism** for less common instructions.

**Fallbacks work by:**
1. Recording the input and output state of an instruction
2. Allowing that single instruction to execute natively
3. Capturing the results in the trace file

The fallback mechanism in TTD encompasses two distinct functions:

1. The ability of TTD's emulator to run an instruction natively, capturing all of its actions and outputs, and resuming emulation afterwards. The actions and outputs get recorded in the trace file.
2. Upon replay, the ability to skip the instruction completely, recreating its actions and outputs from the data recorded in the trace file.

The first function, to run an instruction natively and capture its effects, is routinely performed by the recorder as needed when it encounters an instruction it can't emulate. The second function is performed by TTD's replay engine automatically, whenever it encounters the recorded effects of an instruction in the trace file.

But TTD also applies some of this functionality for other purposes, which we call "synthetic" fallbacks:

1. If the recorder receives a fix to emulate an instruction that it previously couldn't emulate, it will record the effects in the trace file anyway, to allow older builds of the replay engine, which wouldn't know how to emulate the instruction, to replay the trace without errors or derailments.
2. This is also done sometimes when a bug in the emulator is fixed, and the fix changes the emulation behavior visibly.
3. If emulating an instruction requires a level of CPU support that falls outside of the requirements of the replay engine (like Intel/AMD's AES-NI, AVX2 or AVX512), TTD will also record the effects to allow the trace to be replayed in older computers.

In summary: Regular fallbacks are instructions that the recorder runs natively (with higher performance overhead) and whose effects are recorded in the trace file (costing space in the file). Synthetic fallbacks are regular instructions (with no additional performance overhead) whose effects are recorded in the trace file anyway, for a variety of reasons.


### Performance Impact

While fallbacks achieve the goal of reducing maintenance burden, they significantly impact recording performance:

- **Normal recording**: 10x-20x slower than native execution
- **Heavy fallback usage**: 100x-1000x slower than native execution
- **File size increase**: The amount varies based on the instruction, but it must record the side effects of the instruction (register output, flags, memory reads/writes, etc.) and the indirect effects of stopping and starting emulation. If fallbacks occur for a heavily used instruction, the file size can increase substantially.

**Why the impact?** Each fallback requires:
- Decoding and encoding the instruction
- Generating a code stub to handle the fallback
- Executing instruction natively
- Capturing CPU state after execution
- Writing additional data to the trace file
- Restarting execution in the TTD environment

### Identifying Fallback Impact

---

## 🚀 Build Instructions

### 📦 Prerequisites

- Windows 10/11
- Visual Studio 2022 (Community, Professional, or Enterprise)
- Git
- vcpkg (Uses the one included with Visual Studio)

---

### 1. Clone the Repository
```cmd
git clone https://github.com/microsoft/WinDbg-Samples.git
```

### 2. Get the TTD replay binaries
```cmd
cd WinDbg-Samples\TTD\ReplayApi\GetTtd
Get-TTD.cmd
```

### 3. Open the Solution
```cmd
cd ..\TraceFallbacks
start TraceFallbacks.sln
```

Build the solution in Visual Studio. Dependencies will be resolved automatically via vcpkg.

### Sharing fallback data with the TTD team

If you encounter performance issues due to fallbacks, consider sharing the output of TraceFallbacks with the TTD team. This helps improve fallback coverage and performance for future releases.

To share fallback data:
1. Run the TraceFallbacks sample to generate a fallback report:
   ```cmd
   TraceFallbacks.exe -o fallbacks.json <path-to-ttd-trace>
   ```
   This will create a `fallbacks.json` file in the current directory.
2. File an issue on the [Windbg-Feedback GitHub repository](https://github.com/microsoft/WinDbg-Feedback/issues).
3. Attach the `fallbacks.json` file to the issue.