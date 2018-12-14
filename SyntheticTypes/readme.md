# Synthetic Types
It is often the case that public symbols simply do not contain type information for some of the types you may want to look at in the debugger even though those types are defined in public SDK headers.  The same can be said if you have to, for some reason, debug some piece of code at the assembly level with no symbols.

The synthetic types extension comes to the rescue here!  This is a JavaScript extension which is intended to interpret memory in the debug target and present a façade over that memory to make it look as if you had type information.  It does this by reading type definitions from a **basic** C header file, dynamically producing JavaScript classes which create a façade for every type definition in the header, and subsequently giving you a way to “construct an instance” of any such class.  These will be fully usable in the `dx` command, the expression evaluator, as well as from other scripts.  As they are not real “symbols and types”, commands like `x` and `dt` will not operate on them.

## Usage
Run `.scriptload SynTypes.js` to load the script. This will extend the debugger data model to add a namespace **Debugger.Utility.Analysis.SyntheticTypes** that can be accessed by the `dx` command or JavaScript extensions.

## A First Example
Let’s take a look at a first example.  Consider the following header file:
```c
struct ExampleStruct
{
    char c;
    short s;
    int i;
    long l;
    int ar[5];
};
```
The synthetic types extension is invoked via a method, *ReadHeader*, within **Debugger.Utility.Analysis.SyntheticTypes**.   The *ReadHeader* method takes three arguments:
* **headerPath** – the path to a basic C header file to read from disk
* **module** – the module to reference for any symbols in the header.  If the header uses other types, this module’s PDB is what will be used.
* **[attributes]** – an optional argument which provides attributes to describe behaviors around reading the header.  We’ll talk about this more shortly.

Let’s take a look at this on our example header file:
```
0:000> dx Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\test.h", "ntdll")
Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\test.h", "ntdll")                 : ntdll.dll(test.h)
    Module           : ntdll.dll
    Header           : test.h
    Types         
```

If you click the DML link for types, you will see the types which were read from the header:
```
0:000> dx -r1 Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\test.h", "ntdll").Types
Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\test.h", "ntdll").Types                
    length           : 0x1
    [0x0]            : ExampleStruct
0:000> dx -r1 Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\test.h", "ntdll").Types[0]
Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\test.h", "ntdll").Types[0]                 : ExampleStruct
    IsUnion          : false
    Name             : ExampleStruct
    Make             [Make(address)- Constructs an instance of this type]
    Description     
0:000> dx -r1 Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\test.h", "ntdll").Types[0].Description
Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\test.h", "ntdll").Types[0].Description                
    [0x0]            : char c
    [0x1]            : short s
    [0x2]            : int i
    [0x3]            : long l
    [0x4]            : int[5] ar
```
Likewise, you can create an instance of the type via either the *Make* method on the specific type table or via the general *CreateInstance* method on **Debugger.Utility.Analysis.SyntheticTypes**.
```
0:007> db 007ff8`f03620d0 l20
00007ff8`f03620d0  08 4c 8b d2 41 80 e2 f0-4d 89 10 48 8d 98 00 00  .L..A...M..H....
00007ff8`f03620e0  01 00 66 42 8d 1c 08 f0-49 0f c7 0b 75 e3 49 8b  ..fB....I...u.I.

0:007> dx -r2 Debugger.Utility.Analysis.SyntheticTypes.CreateInstance("ExampleStruct", 0x007ff8`f03620d0)
Debugger.Utility.Analysis.SyntheticTypes.CreateInstance("ExampleStruct", 0x007ff8`f03620d0)                
    c                : 0x8
    s                : -11637
    i                : -253591487
    l                : 0x4810894d
    ar               [Type: int [5]]
        [0]              : 39053 [Type: int]
        [1]              : 1113980929 [Type: int]
        [2]              : -267903859 [Type: int]
        [3]              : 197594953 [Type: int]
        [4]              : -1958091915 [Type: int]

0:007> dx (short)-11637,x
(short)-11637,x  : 0xd28b [Type: short]
```
If you note from the memory dump above, the header reader performed natural alignment.  The *‘c’* field is presented from address 0d0.  The *‘s’* field came from the 0xd28b at 0d2 with one byte of padding in between.
# More Advanced Usages
It is important to note that the header parser in the extension is rather simplistic.  It cannot handle C++ headers.  It can handle simplistic #ifdefs and the like.  There may be cases with things like SDK headers where it is important to define a macro (e.g.: _WIN32_WINNT) in the context of what is being read.  This can be done with the optional *attributes* argument of *ReadHeader*.  

Take an example with the *dbghelp.h* header from the Windows SDK.  We want to set the _WIN64 macro for the header parsing, so we pass the following to *ReadHeader’s* attributes:
```c
new { Macros = { _WIN64 = “1” }}
```

This loads the header with the _WIN64 macro defined to “1”:
```
0:004> dx Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\dbghelp.h", "ntdll", new { Macros = new { _WIN64 = "1" }})
Debugger.Utility.Analysis.SyntheticTypes.ReadHeader("e:\\examples\\dbghelp.h", "ntdll", new { Macros = new { _WIN64 = "1" }})                 : ntdll.dll(dbghelp.h)
    Module           : ntdll.dll
    Header           : dbghelp.h
    Types        
```
Now let’s take a look at what a synthetic visualization of a _tagSTACKFRAME_EX looks like versus full private symbols.  Here’s the synthetic symbols we generated from the header read:
```
0:004> dx -r2 Debugger.Utility.Analysis.SyntheticTypes.CreateInstance("_tagSTACKFRAME_EX", 0xff4727dc10)
Debugger.Utility.Analysis.SyntheticTypes.CreateInstance("_tagSTACKFRAME_EX", 0xff4727dc10)                
    AddrPC          
        Offset           : 0x7ff8f039310c
        Segment          : 0x0
        Mode             : 0x3
    AddrReturn      
        Offset           : 0x7ff8f0396596
        Segment          : 0x0
        Mode             : 0x3
    AddrFrame       
        Offset           : 0x74aa17f480
        Segment          : 0x0
        Mode             : 0x3
    AddrStack       
        Offset           : 0x74aa17f450
        Segment          : 0x0
        Mode             : 0x3
    AddrBStore      
        Offset           : 0x0
        Segment          : 0x0
        Mode             : 0x0
    FuncTableEntry   : 0x0 [Type: void *]
    Params           [Type: unsigned __int64 [4]]
        [0]              : 0x0 [Type: unsigned __int64]
        [1]              : 0x7ff8f03ed190 [Type: unsigned __int64]
        [2]              : 0x7ff8f03ed190 [Type: unsigned __int64]
        [3]              : 0x7ff8f03ed190 [Type: unsigned __int64]
    Far              : 0x0
    Virtual          : 0x1
    Reserved         [Type: unsigned __int64 [3]]
        [0]              : 0x0 [Type: unsigned __int64]
        [1]              : 0x0 [Type: unsigned __int64]
        [2]              : 0x1 [Type: unsigned __int64]
    KdHelp          
        Thread           : 0x0
        ThCallbackStack  : 0x0
        ThCallbackBStore : 0x0
        NextCallback     : 0x0
        FramePointer     : 0x0
        KiCallUserMode   : 0x1
        KeUserCallbackDispatcher : 0x0
        SystemRangeStart : 0x0
        KiUserExceptionDispatcher : 0x0
        StackBase        : 0x74aa180000
        StackLimit       : 0x74aa16f000
        BuildVersion     : 0x4563
        Reserved0        : 0x0
        Reserved1        [Type: unsigned __int64 [4]]
    StackFrameSize   : 0x110
    InlineFrameContext : 0x944e0100
```
Here’s the full private symbols:
```
0:004> dx -r2 StackFrame
StackFrame                 : 0xff4727dc10 [Type: _tagSTACKFRAME_EX *]
    [+0x000] AddrPC           [Type: _tagADDRESS64]
        [+0x000] Offset           : 0x7ff8f039310c [Type: unsigned __int64]
        [+0x008] Segment          : 0x0 [Type: unsigned short]
        [+0x00c] Mode             : AddrModeFlat (3) [Type: ADDRESS_MODE]
    [+0x010] AddrReturn       [Type: _tagADDRESS64]
        [+0x000] Offset           : 0x7ff8f0396596 [Type: unsigned __int64]
        [+0x008] Segment          : 0x0 [Type: unsigned short]
        [+0x00c] Mode             : AddrModeFlat (3) [Type: ADDRESS_MODE]
    [+0x020] AddrFrame        [Type: _tagADDRESS64]
        [+0x000] Offset           : 0x74aa17f480 [Type: unsigned __int64]
        [+0x008] Segment          : 0x0 [Type: unsigned short]
        [+0x00c] Mode             : AddrModeFlat (3) [Type: ADDRESS_MODE]
    [+0x030] AddrStack        [Type: _tagADDRESS64]
        [+0x000] Offset           : 0x74aa17f450 [Type: unsigned __int64]
        [+0x008] Segment          : 0x0 [Type: unsigned short]
        [+0x00c] Mode             : AddrModeFlat (3) [Type: ADDRESS_MODE]
    [+0x040] AddrBStore       [Type: _tagADDRESS64]
        [+0x000] Offset           : 0x0 [Type: unsigned __int64]
        [+0x008] Segment          : 0x0 [Type: unsigned short]
        [+0x00c] Mode             : AddrMode1616 (0) [Type: ADDRESS_MODE]
    [+0x050] FuncTableEntry   : 0x0 [Type: void *]
    [+0x058] Params           [Type: unsigned __int64 [4]]
        [0]              : 0x0 [Type: unsigned __int64]
        [1]              : 0x7ff8f03ed190 [Type: unsigned __int64]
        [2]              : 0x7ff8f03ed190 [Type: unsigned __int64]
        [3]              : 0x7ff8f03ed190 [Type: unsigned __int64]
    [+0x078] Far              : 0 [Type: int]
    [+0x07c] Virtual          : 1 [Type: int]
    [+0x080] Reserved         [Type: unsigned __int64 [3]]
        [0]              : 0x0 [Type: unsigned __int64]
        [1]              : 0x0 [Type: unsigned __int64]
        [2]              : 0x1 [Type: unsigned __int64]
    [+0x098] KdHelp           [Type: _KDHELP64]
        [+0x000] Thread           : 0x0 [Type: unsigned __int64]
        [+0x008] ThCallbackStack  : 0x0 [Type: unsigned long]
        [+0x00c] ThCallbackBStore : 0x0 [Type: unsigned long]
        [+0x010] NextCallback     : 0x0 [Type: unsigned long]
        [+0x014] FramePointer     : 0x0 [Type: unsigned long]
        [+0x018] KiCallUserMode   : 0x1 [Type: unsigned __int64]
        [+0x020] KeUserCallbackDispatcher : 0x0 [Type: unsigned __int64]
        [+0x028] SystemRangeStart : 0x0 [Type: unsigned __int64]
        [+0x030] KiUserExceptionDispatcher : 0x0 [Type: unsigned __int64]
        [+0x038] StackBase        : 0x74aa180000 [Type: unsigned __int64]
        [+0x040] StackLimit       : 0x74aa16f000 [Type: unsigned __int64]
        [+0x048] BuildVersion     : 0x4563 [Type: unsigned long]
        [+0x04c] RetpolineStubFunctionTableSize : 0x0 [Type: unsigned long]
        [+0x050] RetpolineStubFunctionTable : 0x0 [Type: unsigned __int64]
        [+0x058] RetpolineStubOffset : 0x0 [Type: unsigned long]
        [+0x05c] RetpolineStubSize : 0x0 [Type: unsigned long]
        [+0x060] Reserved0        [Type: unsigned __int64 [2]]
    [+0x108] StackFrameSize   : 0x110 [Type: unsigned long]
    [+0x10c] InlineFrameContext : 0x944e0100 [Type: unsigned long]
```
While you do not get symbol offsets and all the “[Type: <native type>]” annotations, the values are identical – all from looking at the public SDK header!
