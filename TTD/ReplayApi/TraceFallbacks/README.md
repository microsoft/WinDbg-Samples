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

TODO: Insert discussion of full and synthetic callbacks.

### Performance Impact

While fallbacks achieve the goal of reducing maintenance burden, they significantly impact recording performance:

- **Normal recording**: 10x-20x slower than native execution
- **Heavy fallback usage**: 100x-1000x slower than native execution
- **File size increase**: The amount varies based on the instruction, but it must record the impact of the instruction (register output + flags) and the indirect effects of stopping and starting emulation. If fallbacks occur for a heavily used instruction, the file size can increase substantially.

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

## 📦 Prerequisites

- Windows 10/11
- Visual Studio 2022 (Community, Professional, or Enterprise)
- Git
- vcpkg (Uses the one included with Visual Studio)

---

### 1. Clone the Repository
```cmd
git clone https://github.com/<your-org>/<your-repo>.git
cd <your-repo>/TTD/ReplayApi/TraceFallbacks
```

### 2. Open the Solution
```cmd
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