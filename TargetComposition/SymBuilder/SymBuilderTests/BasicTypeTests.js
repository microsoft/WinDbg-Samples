"use strict";
//
// [Harness: run notepad.exe]
//

//**************************************************************************
// BasicTypeTests.js
//
// Unit tests for basic types within the type builder. 
//
// NOTE: The test harness will read the script before opening the engine and having it execute it.
//       During that process, it will read any comment lines at the beginning of the file (after any
//       optional "use strict";) and will interpret // [Harness: <something>] as a command to the
//       test harness.  Here, this script will be executed against "notepad.exe" because of the
//       harness command.
//
// Any method named Test_* is a test case.  The "TestSuite" global array is what the test runner will
// pick up on.  It has the format of { Name: <test name>, Code: <test function> }.  All tests will be
// executed in the order specified in that array.
//
// The reason tests here are "Test_*" is that it makes the script easy to pick up in a regular debugger
// install and understand what is going wrong.  You can step through the script:
//
//     .scriptload BasicTypeTests.js
//     .scriptdebug BasicTypeTests.js
//         <In the script debugger, set your breakpoints / q>
//     dx @$t = @$scriptContents.initializeTests()
//     dx @$scriptContents.Test_X();
//         <When broken into the script debugger, do what is needed>
//
// You can also attach an outer NATIVE debugger to the debugger and set corresponding breakpoints in
// SymbolBuilderComposition.dll.
//   

var __symbolBuilderSymbols = null;
var __ctl = null;
var __symBuilder = null;

var __uniqueId = 0;

//**************************************************************************
// Utility:
//
// Right now, these helpers are included in each script to make writing tests easier.  It would be nice
// if the infrastructure could inject some of these into the test script context instead of duplicating
// them into each test script.
//

// __Errorinfo:
//
// Parses the stack from an error to yield file names, line numbers, etc...
// Note that this is ChakraCore specific and may need to change if the engine underneath
// JsProvider ever changes.
//
class __Errorinfo
{
    constructor(err)
    {
        this.__err = err;
        this.__errstack = err.stack;
        this.__errlines = this.__errstack.split("\n");
    }

    *[Symbol.iterator]()
    {
        for (var line of this.__errlines)
        {
            var re = /at (\S+) \(([^:]*):(\d+):(\d+)\)/;
            var result = re.exec(line);
            if (result)
            {
                yield { FunctionName: result[1],
                        SourceFile: result[2],
                        SourceLine: result[3],
                        SourceColumn: result[4] };
            }
        }
    }
}

// __formerror:
//
// Takes an existing error object for a verification failure (and its stack) and reforms it into a new
// one with a slightly different message.
//
function __formerror(e, str)
{
    var info = new __Errorinfo(e);
    var callerInfo = null;
    var idx = 0;
    for (var frame of info)
    {
        if (++idx == 2)
        {
            callerInfo = frame;
            break;
        }
    }

    var msg = "Verification FAILED";
    if (callerInfo)
    {
        msg += " @ ";
        msg += callerInfo.FunctionName;
        msg += ":";
        msg += callerInfo.SourceLine;
        msg += ":";
        msg += callerInfo.SourceColumn;
    }

    if (str !== undefined)
    {
        msg += " (";
        msg += str;
        msg += ")";
    }

    return new Error(msg);
}

// __VERIFY:
//
// Helper to verify a boolean condition and throw an error (with optional message) if the verification
// fails.
//
function __VERIFY(val, excStr)
{
    if (!val)
    {
        throw __formerror(new Error("VERIFICATION FAILED"), excStr);
    }
}

function __COUNTOF(data)
{
    var count = 0;
    for (var datum of data)
    {
        ++count;
    }
    return count;
}

function __getUniqueName(baseName)
{
    var id = ++__uniqueId;
    return (baseName + "__" + id.toString());
}

//**************************************************************************
// Test Cases
//

// Test_VerifyBuilderSymbols:
//
// Verify that after having symbols created for notepad.exe, we have the .SymbolBuilderSymbols
// property on its module object and that it's the same as what was returned from the original creation.
//
function Test_VerifyBuilderSymbols()
{
    var notepadModule = host.currentProcess.Modules.getValueAt("notepad.exe");
    var builderSymbols = notepadModule.SymbolBuilderSymbols;

    __VERIFY(builderSymbols !== undefined && builderSymbols !== null && !!builderSymbols, 
             "Unable to find .SymbolBuilderSymbols property on expected module");

    return true;
}

// Test_CreateAndDestroyEmptyUdt:
//
// Verifies that we can create and verify an empty UDT (getting back at it with standard type system APIs in
// JavaScript) and then successfully destroy it.
//
function Test_CreateAndDestroyEmptyUdt()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    //
    // Verify the type has what we expect from the symbol provider's API:
    //
    __VERIFY(foo.Name == name, "Unexpected type name");
    __VERIFY(foo.QualifiedName == name, "Unexpected qualified type name");
    __VERIFY(foo.Size == 0, "Unexpected size for newly created type");
    __VERIFY(foo.Alignment == 1, "Unexpected alignment for newly created type");
    __VERIFY(__COUNTOF(foo.BaseClasses) == 0, "Unexpected number of base classes for newly created type");
    __VERIFY(__COUNTOF(foo.Fields) == 0, "Unexpected number of fields for newly created type");

    //
    // Verify we can get back at the type from standard JavaScript means for the underlying type system.
    //
    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.name == name, "Unexpected type system name");
    __VERIFY(fooTy.size == 0, "Unexpected type system size");
    __VERIFY(fooTy.typeKind == "udt", "Unexpected type system type kind");
    __VERIFY(__COUNTOF(Object.getOwnPropertyNames(fooTy.fields)) == 0, "Unexpected type system number of fields");
    __VERIFY(__COUNTOF(fooTy.baseClasses) == 0, "Unexpected type system number of base classes");

    foo.Delete();

    var fooTy2 = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy2 == null, "Unexpected ability to find UDT after deletion");

    return true;
}

// Test_UdtWithBasicFields:
//
// Verifies that we can create a UDT and add basic fields to the UDT and it will lay out as expected.  This is
// *NOT* testing packing and alignment.
//
// effectively:
//
//     struct foo {
//         int x;
//         int y;
//     };
//
function Test_UdtWithBasicFields()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    //
    // Add "x" and verify certain things about the returned field and subsequently, the type system that is
    // exposed.
    //
    var fldX = foo.Fields.Add("x", "int");
    __VERIFY(fldX.Name == "x", "Unexpected name for field");
    __VERIFY(fldX.QualifiedName == "x", "Unexpected qualified name for field");
    __VERIFY(fldX.IsAutomaticLayout, "Unexpected value for .IsAutomaticLayout");
    __VERIFY(fldX.Offset == 0, "Unexpected layout of first int field");
    __VERIFY(fldX.Type.Name == "int", "Unexpected type name of field");
    __VERIFY(fldX.Parent.Name == name, "Unexpected parent name of field");

    __VERIFY(foo.Size == 4, "Unexpected size of struct after adding a single int");
    __VERIFY(foo.Alignment == 4, "Unexpected alignment of struct after adding a single int");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.size == 4 , "Unexpected underlying type system size after adding a single int");
    var fooTyFldX = fooTy.fields.x;
    __VERIFY(fooTyFldX !== undefined, "Unable to find newly added field from type system");
    __VERIFY(fooTyFldX.offset == 0, "Unexpected offset of field in underlying type system");
    __VERIFY(fooTyFldX.type.name == "int", "Unexpected type of field in underlying type system");

    __VERIFY(host.evaluateExpression("sizeof(" + name + ")") == 4, "Unexpected sizeof() in EE");

    //
    // Add "y" and verify certain things about the type have changed as expected!
    //
    var fldY = foo.Fields.Add("y", "int");
    __VERIFY(fldY.Offset == 4, "Unexpected layout of second int field");

    __VERIFY(foo.Size == 8, "Unexpected size of struct after adding second int");
    __VERIFY(foo.Alignment == 4, "Unexpected alignment of struct after adding second int");

    //
    // Get rid of 'fooTy' as it's an *OLD* cached copy of the type!
    //
    fooTy = host.getModuleType("notepad.exe", name);

    __VERIFY(fooTy.size == 8, "Unepxected underlying type system size after adding second int");
    var fooTyFldY = fooTy.fields.y;
    __VERIFY(fooTyFldY !== undefined, "Unable to find second added field from type system");
    __VERIFY(fooTyFldY.offset == 4, "Unexpected offset of second field in underlying type system");

    __VERIFY(host.evaluateExpression("sizeof(" + name + ")") == 8, "Unexpected sizeof() in EE after second field");

    foo.Delete();
    return true;
}

// Test_AutoLayoutAlignment:
//
// Verifies that we can create a UDT and place increasingly sized (and aligned) fields and that the alignment
// padding is properly inserted by automatic layout.
//
function Test_AutoLayoutAlignment()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);
    var fldA = foo.Fields.Add("a", "char");                 // [0, 1)
    var fldB = foo.Fields.Add("b", "short");                // [2, 4)
    var fldC = foo.Fields.Add("c", "int");                  // [4, 8)
    var fldD = foo.Fields.Add("d", "char");                 // [8, 9)
    var fldE = foo.Fields.Add("e", "__int64");              // [16, 24)
    var fldF = foo.Fields.Add("f", "char");                 // [24, 25)

    __VERIFY(fldA.Offset == 0, "unexpected offset of 'a'");
    __VERIFY(fldB.Offset == 2, "unexpected offset of 'b'");
    __VERIFY(fldC.Offset == 4, "unexpected offset of 'c'");
    __VERIFY(fldD.Offset == 8, "unexpected offset of 'd'");
    __VERIFY(fldE.Offset == 16, "unexpected offset of 'e'");
    __VERIFY(fldF.Offset == 24, "unexpected offset of 'f'");

    //
    // It should be padded out to its own natural alignment.
    //
    __VERIFY(foo.Size == 32, "unexpected overall size of type");

    foo.Delete();
    return true;
}

// Test_NestedStructsWithAutoAlignment:
//
// Verifies that we can create two UDTs and place one within the other getting proper alignment padding
// in auto layout mode.
//
function Test_NestedStructsWithAutoAlignment()
{
    var fooName = __getUniqueName("foo");
    var barName = __getUniqueName("bar");

    var foo = __symbolBuilderSymbols.Types.Create(fooName);
    var fooFldA = foo.Fields.Add("a", "char");          // [0, 1)

    var bar = __symbolBuilderSymbols.Types.Create(barName);
    var barFldJ = bar.Fields.Add("j", "int");           // [0, 4)
    var barFldK = bar.Fields.Add("k", "char");          // [4, 5) <-- +pad to 8

    //
    // Add by name for the "first" attempt and add by type object for the "second" attempt
    //
    var fooFldB = foo.Fields.Add("b", barName);         // [4, 12)
    var fooFldC = foo.Fields.Add("c", bar);             // [12, 20)

    //
    // Now go and verify things through the symbol builder's API:
    //
    __VERIFY(barFldJ.Offset == 0 && barFldK.Offset == 4 && bar.Size == 8 && bar.Alignment == 4,
             "unexpected layout of 'bar' type");

    __VERIFY(fooFldA.Offset == 0, "unexpected offset of 'a'");
    __VERIFY(fooFldB.Offset == 4, "unexpected offset of 'b'");
    __VERIFY(fooFldC.Offset == 12, "unexpected offset of 'c'");
    __VERIFY(foo.Size == 20, "unexpected size of 'foo'");

    //
    // Do some basic sanity checking against the underlying type system.
    //
    var fooTy = host.getModuleType("notepad.exe", fooName);
    var fooTyFldB = fooTy.fields.b;
    __VERIFY(fooTyFldB !== undefined, "cannot find 'b'");
    __VERIFY(fooTyFldB.offset == 4, "unexpected type system offset of 'b'");
    __VERIFY(fooTyFldB.type.name == barName, "unexpected type system type of 'b'");
    __VERIFY(fooTyFldB.type.size == 8, "unexpected type system size of 'bar' via 'b'");

    foo.Delete();
    bar.Delete();
    return true;
}

// Test_StructManualLayout:
//
// Verifies that we can manually layout a struct.
//
function Test_StructManualLayout()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    var fooFldX = foo.Fields.Add("x", "int", 0);            // [0, 4)
    var fooFldY = foo.Fields.Add("y", "int", 0);            // [0, 4)
    var fooFldZ = foo.Fields.Add("z", "int", 1);            // [1, 5) <-- **EXPLICITLY UNALIGNED** <-- pad to 8

    __VERIFY(fooFldX.Offset == 0, "unexpected offset of 'x'");
    __VERIFY(!fooFldX.IsAutomaticLayout, "unexpected .IsAutomaticLayout for manual field");
    __VERIFY(fooFldY.Offset == 0, "unexpected offset of 'y'");
    __VERIFY(fooFldZ.Offset == 1, "unexpected offset of 'z'");
    __VERIFY(foo.Size == 8, "unexpected overall size of manual layout type");
    __VERIFY(foo.Alignment == 4, "unexpected alignment of manual layout type");

    //
    // Do some basic sanity checking against the underlying type system.
    //
    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.x.offset == 0, "unexpected underlying type system offset of 'x'");
    __VERIFY(fooTy.fields.y.offset == 0, "unexpected underlying type system offset of 'y'");
    __VERIFY(fooTy.fields.z.offset == 1, "unexpected underlying type system offset of 'z'");
    __VERIFY(fooTy.size == 8, "unexpected underlying type system size of manual layout type");

    foo.Delete();
    return true;
}

// Test_StructMixedManualAutoLayout:
//
// Verifies that we can mix automatic and manual layout fields in a struct and get what we expect.
//
function Test_StructMixedManualAutoLayout()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    var fooFldA = foo.Fields.Add("a", "int");               // [0, 4)
    var fooFldB = foo.Fields.Add("b", "int");               // [4, 8)
    var fooFldC = foo.Fields.Add("c", "int", 20);           // [20, 24)
    var fooFldD = foo.Fields.Add("d", "int");               // [24, 28)
    var fooFldE = foo.Fields.Add("e", "char", 61);          // [61, 62)
    var fooFldF = foo.Fields.Add("f", "__int64");           // [64, 72)

    __VERIFY(fooFldD.Offset == 24, "unexpected offset of 'd'");
    __VERIFY(fooFldF.Offset == 64, "unexpected offset of 'f'");
    __VERIFY(foo.Size == 72, "unexpected size of type");
    __VERIFY(foo.Alignment == 8, "unexpected alignment of type");

    //
    // Do some basic sanity checking against the underlying type system.
    //
    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.d.offset == 24, "unexpected underlying type system offset of 'd'");
    __VERIFY(fooTy.fields.f.offset == 64, "unexpected underlying type system offset of 'd'");
    __VERIFY(fooTy.size == 72, "unexpected underlying type system size");

    foo.Delete();
    return true;
}

// Test_StructDeleteFields:
//
// Verifies that we can delete fields of a struct and that layout will reflow around the deleted
// field.
//
function Test_StructDeleteFields()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    var fooFldA = foo.Fields.Add("a", "__int64");           // [0, 8)
    var fooFldB = foo.Fields.Add("b", "char");              // [8, 9)
    var fooFldC = foo.Fields.Add("c", "int");               // [12, 16)

    __VERIFY(foo.Size == 16, "unexpected size of type");
    __VERIFY(fooFldC.Offset == 12, "unexpected offset of 'c'");

    //
    // Do some *PRE DELETE* sanity checks against the underlying type system.
    //
    var fooTy = host.getModuleType("notepad.exe", name);

    __VERIFY(fooTy.fields.b.offset == 8, "unexpected underlying type system offset of 'b'");
    __VERIFY(fooTy.fields.c.offset == 12, "unexpected underlying type system offset of 'c'");

    fooFldB.Delete();

    __VERIFY(foo.Size == 16, "unexpected size of type after field delete");
    __VERIFY(fooFldC.Offset == 8, "unexpected offset of 'c' after 'b' delete");

    fooTy = host.getModuleType("notepad.exe", name);

    __VERIFY(fooTy.size == 16, "unexpected underlying type system size after field delete");
    __VERIFY(fooTy.fields.b === undefined, "unexpected ability to find 'b' after field delete");

    foo.Delete();
    return true;
}

// Test_StructChangeFieldType
//
// Verifies that we can change the type of a field and that layout will reflow around the changed
// type.
//
function Test_StructChangeFieldType()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    var fooFldA = foo.Fields.Add("a", "__int64");           // [0, 8)
    var fooFldB = foo.Fields.Add("b", "int");               // [8, 12)
    var fooFldC = foo.Fields.Add("c", "short");             // [12, 14)
    var fooFldD = foo.Fields.Add("d", "char");              // [14, 15)

    //
    // Do some *PRE DELETE* sanity checks against the underlying type system.
    //
    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.c.offset == 12, "unexpected underlying type system offset of 'c' pre-retype");
    __VERIFY(fooTy.fields.d.offset == 14, "unexpected underlying type system offset of 'd' pre-retype");
    __VERIFY(fooTy.size == 16, "unexpected underlying type system size pre-retype");

    //
    // To:
    //
    // a: __int64 [0, 8)
    // b: char [8, 9)
    // c: short [10, 12)
    // d: char [12, 13)    <-- still aligns to 16 overall size. but c and d moved.
    //

    fooFldB.Type = "char";

    __VERIFY(fooFldB.Offset == 8, "unexpected offset of 'b' post retype");
    __VERIFY(fooFldB.Type.Name == "char", "unexpected type of 'b' post retype");
    __VERIFY(fooFldC.Offset == 10, "unexpected offset of 'c' post retype");
    __VERIFY(fooFldD.Offset == 12, "unexpected offset of 'd' post retype");

    fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.c.offset == 10, "unexpected underlying type system offset of 'c' post-retype");
    __VERIFY(fooTy.fields.d.offset == 12, "unexpected underlying type system offset of 'd' post-retype");
    __VERIFY(fooTy.size == 16, "unexpected underlying type system size post-retype");

    foo.Delete();
    return true;
}

// Test_StructMoveField:
//
// Verifies that we can move a field of a type in automatic layout mode and that things reflow and the
// structure looks as expected.
//
function Test_StructMoveField()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    var fooFldA = foo.Fields.Add("a", "__int64");       // [0, 8)
    var fooFldB = foo.Fields.Add("b", "int");           // [8, 12)
    var fooFldC = foo.Fields.Add("c", "short");         // [12, 14)
    var fooFldD = foo.Fields.Add("d", "char");          // [15, 16)

    //
    // Do some *PRE DELETE* sanity checks against the underlying type system.
    //
    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.size == 16, "unexpected underlying type system size pre-move");
    __VERIFY(fooTy.fields.a.offset == 0, "unexpected underlying type system offset of 'a' pre-move");

    //
    // Rearrange the type to:
    //
    // d:char    [0, 1)
    // a:__int64 [8, 16)
    // b: int    [16, 20)
    // c: short  [20, 22)   <-- size is now 24
    //
    fooFldD.MoveBefore(fooFldA);

    __VERIFY(fooFldA.Offset == 8, "unexpected offset of 'a' after move");
    __VERIFY(foo.Size == 24, "unexpected size of type after move");

    fooTy = host.getModuleType("notepad.exe", name);

    __VERIFY(fooTy.size == 24, "unexpected underlying type system size post-move");
    __VERIFY(fooTy.fields.a.offset == 8, "unexpected underlying type system offset of 'a' post-move");

    //
    // Move it back and ensure that things work as expected and we can push something to the back of
    // the field list.
    //
    fooFldD.MoveBefore(99);

    __VERIFY(fooFldA.Offset == 0, "unexpected offset of 'a' after restore");
    __VERIFY(foo.Size == 16, "unexpected size of type after restore");

    fooTy = host.getModuleType("notepad.exe", name);

    __VERIFY(fooTy.size == 16, "unexpected underlying type system size post-restore");
    __VERIFY(fooTy.fields.a.offset == 0, "unexpected underlying type system offset of 'a' post-restore");

    var barName = __getUniqueName("bar");
    var bar = __symbolBuilderSymbols.Types.Create(barName);

    var barFldX = bar.Fields.Add("x", "int");
    var barFldY = bar.Fields.Add("y", "int");
    var barFldZ = bar.Fields.Add("z", "int");

    //
    // Verify that trying to do a .MoveBefore() a field of *ANOTHER* type fails.
    //
    var caught = false;
    try
    {
        fooFldA.MoveBefore(barFldZ);
    }
    catch(exc)
    {
        caught = true;
    }

    __VERIFY(caught, "unexpected success of moving a 'foo' field to before a 'bar' field!");

    foo.Delete();
    bar.Delete();

    return true;
}

// Test_ArrayCreation:
//
// Verify that we can create arrays and get back expected results.
//
function Test_ArrayCreation()
{
    var arOfInt1 = __symbolBuilderSymbols.Types.CreateArray("int", 32);
    __VERIFY(arOfInt1.Size == 32 * 4, "unexpected size of array");
    __VERIFY(arOfInt1.BaseType.Name == "int", "unexpected base type of array");
    __VERIFY(arOfInt1.ArraySize == 32, "unexpected array dimension");

    var name =  __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    //
    // Ensure that we can add fields of this type and get back at such via the type system.
    //
    var fooFldA = foo.Fields.Add("a", arOfInt1);
    var fooFldB = foo.Fields.Add("b", "int [8]");
    var fooFldC = foo.Fields.Add("c", "char[4]");

    __VERIFY(fooFldA.Type.BaseType.Name == "int", "unexpected field type of 'a'");
    __VERIFY(fooFldA.Type.ArraySize == 32, "unexpected array dimension of 'a'");
    __VERIFY(fooFldA.Offset == 0, "unexpected offset of 'a'");
    __VERIFY(fooFldB.Type.BaseType.Name == "int", "unexpected field type of 'b'");
    __VERIFY(fooFldB.Type.ArraySize == 8, "unexpected array dimension of 'b'");
    __VERIFY(fooFldB.Offset == 32 * 4, "unexpected offset of 'b'");
    __VERIFY(fooFldC.Type.BaseType.Name == "char", "unexpected field type of 'c'");
    __VERIFY(fooFldC.Type.ArraySize == 4, "unexpected array dimension of 'c'");
    __VERIFY(fooFldC.Offset == 32 * 4 + 8 * 4, "unexpected offset of 'c'");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.a.type.typeKind == "array", "unexpected type system type kind of field 'a'");
    __VERIFY(fooTy.fields.b.type.typeKind == "array", "unexpected type system type kind of field 'b'");
    __VERIFY(fooTy.fields.c.type.typeKind == "array", "unexpected type system type kind of field 'c'");
    __VERIFY(fooTy.fields.a.type.baseType.name == "int", "unexpected type system array-of type of field 'a'");
    __VERIFY(fooTy.fields.b.type.baseType.name == "int", "unexpected type system array-of type of field 'b'");
    __VERIFY(fooTy.fields.c.type.baseType.name == "char", "unexpected type system array-of type of field 'c'");

    foo.Delete();
    return true;
}

// Test_PointerCreation:
//
// Verify that we can create poiners and get back expected results.
//
function Test_PointerCreation()
{
    //
    // Find the size of a pointer in a module which is *NOT* on symbol builder symbols.  Note that these types
    // are natively understood by the debugger/EE and they will work even if the module has no symbols whatsoever.
    //
    var genPtr = host.getModuleType("ntdll", "int *");
    var ptrSize = genPtr.size;

    var ptrToInt1 = __symbolBuilderSymbols.Types.CreatePointer("int");
    __VERIFY(ptrToInt1.Size == ptrSize, "unexpected pointer size");
    __VERIFY(ptrToInt1.BaseType.Name == "int", "unexpected base type of pointer");

    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    //
    // Ensure that we can add fields of this type and get back at such via the type system.
    //
    var fooFldA = foo.Fields.Add("a", ptrToInt1);
    var fooFldB = foo.Fields.Add("b", "int *");
    var fooFldC = foo.Fields.Add("c", "char *");
    var fooFldD = foo.Fields.Add("d", name + " *");
    var fooFldE = foo.Fields.Add("e", "int **");
    var fooFldF = foo.Fields.Add("f", name + " * *");

    __VERIFY(fooFldA.Type.BaseType.Name == "int", "unexpected field type of 'a'");
    __VERIFY(fooFldA.Offset == 0, "unexpected offset of 'a'");
    __VERIFY(fooFldB.Type.BaseType.Name == "int", "unexpected field type of 'b'");
    __VERIFY(fooFldB.Offset == ptrSize * 1, "unexpected offset of 'b'");
    __VERIFY(fooFldC.Type.BaseType.Name == "char", "unexpected field type of 'c'");
    __VERIFY(fooFldC.Offset == ptrSize * 2, "unexpected offset of 'c'");
    __VERIFY(fooFldD.Type.BaseType.Name == name, "unexpected field type of 'd'");
    __VERIFY(fooFldD.Offset == ptrSize * 3, "unexpected offset of 'd'");
    __VERIFY(fooFldE.Type.BaseType.BaseType.Name == "int", "unexpected field type of 'e'");
    __VERIFY(fooFldE.Offset == ptrSize * 4, "unexpected offset of 'e'");
    __VERIFY(fooFldF.Type.BaseType.BaseType.Name == name, "unexpected field type of 'f'");
    __VERIFY(fooFldF.Offset == ptrSize * 5, "unexpected offset of 'f'");


    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.a.type.typeKind == "pointer", "unexpected type system kind of field 'a'");
    __VERIFY(fooTy.fields.a.type.pointerKind == "standard", "unexpected type system pointer kind of field 'a'");
    __VERIFY(fooTy.fields.a.type.baseType.name == "int", "unexpected type system base type name of field 'a'");
    __VERIFY(fooTy.fields.b.type.typeKind == "pointer", "unexpected type system kind of field 'b'");
    __VERIFY(fooTy.fields.b.type.pointerKind == "standard", "unexpected type system pointer kind of field 'b'");
    __VERIFY(fooTy.fields.b.type.baseType.name == "int", "unexpected type system base type name of field 'a'");
    __VERIFY(fooTy.fields.c.type.typeKind == "pointer", "unexpected type system kind of field 'c'");
    __VERIFY(fooTy.fields.c.type.pointerKind == "standard", "unexpected type system pointer kind of field 'c'");
    __VERIFY(fooTy.fields.c.type.baseType.name == "char", "unexpected type system base type name of field 'c'");
    __VERIFY(fooTy.fields.d.type.typeKind == "pointer", "unexpected type system kind of field 'd'");
    __VERIFY(fooTy.fields.d.type.pointerKind == "standard", "unexpected type system pointer kind of field 'd'");
    __VERIFY(fooTy.fields.d.type.baseType.name == name, "unexpected type system base type name of field 'd'");
    __VERIFY(fooTy.fields.e.type.typeKind == "pointer", "unexpected type system kind of field 'e'");
    __VERIFY(fooTy.fields.e.type.pointerKind == "standard", "unexpected type system pointer kind of field 'e'");
    __VERIFY(fooTy.fields.e.type.baseType.baseType.name == "int", "unexpected type system base type name of field 'e'");
    __VERIFY(fooTy.fields.f.type.typeKind == "pointer", "unexpected type system kind of field 'f'");
    __VERIFY(fooTy.fields.f.type.pointerKind == "standard", "unexpected type system pointer kind of field 'f'");
    __VERIFY(fooTy.fields.f.type.baseType.baseType.name == name, "unexpected type system base type name of field 'f'");

    foo.Delete();
    return true;
}

// Test_EnumWithAutoLayout:
//
// Verify that we can create enums with automatic layout of enumerants and get back expected results.
//
function Test_EnumWithAutoLayout()
{
    var name = __getUniqueName("fruit");
    var enumType = __symbolBuilderSymbols.Types.CreateEnum(name);

    var enumerantA = enumType.Enumerants.Add("apple");
    var enumerantB = enumType.Enumerants.Add("orange");
    var enumerantC = enumType.Enumerants.Add("grapefruit");
    var enumerantD = enumType.Enumerants.Add("lychee");

    __VERIFY(enumType.BaseType.Name == "int", "unexpected base type of enum");
    __VERIFY(enumType.Size == 4, "unexpected size of enum");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.apple.value == 0, "unexpected type system value of enumerant 'apple'");
    __VERIFY(fooTy.fields.orange.value == 1, "unexpected type system value of enumerant 'orange'");
    __VERIFY(fooTy.fields.grapefruit.value == 2, "unexpected type system value of enumerant 'grapefruit'");
    __VERIFY(fooTy.fields.lychee.value == 3, "unexpected type system value of enumerant 'lychee'");

    enumType.Delete();
    return true;
}

// Test_EnumWithMixedLayout:
//
// Verify that we can create enums with mixed layout (some auto, some manual) of enumerants and get
// back expected results.
//
function Test_EnumWithMixedLayout()
{
    var name = __getUniqueName("fruit");
    var enumType = __symbolBuilderSymbols.Types.CreateEnum(name);

    var enumerantA = enumType.Enumerants.Add("apple");
    var enumerantB = enumType.Enumerants.Add("orange");
    var enumerantC = enumType.Enumerants.Add("grapefruit", 42);
    var enumerantD = enumType.Enumerants.Add("lychee");

    __VERIFY(enumType.BaseType.Name == "int", "unexpected base type of enum");
    __VERIFY(enumType.Size == 4, "unexpected size of enum");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.apple.value == 0, "unexpected type system value of enumerant 'apple'");
    __VERIFY(fooTy.fields.orange.value == 1, "unexpected type system value of enumerant 'orange'");
    __VERIFY(fooTy.fields.grapefruit.value == 42, "unexpected type system value of enumerant 'grapefruit'");
    __VERIFY(fooTy.fields.lychee.value == 43, "unexpected type system value of enumerant 'lychee'");

    enumType.Delete();
    return true;
}

// Test_EnumDeleteEnumerants:
//
// Verifies that we can delete an enumerant and get automatic layout reflow around the deleted value.
//
function Test_EnumDeleteEnumerants()
{
    var name = __getUniqueName("foo");
    var enumType = __symbolBuilderSymbols.Types.CreateEnum(name);

    var enumerantA = enumType.Enumerants.Add("apple");
    var enumerantB = enumType.Enumerants.Add("orange");
    var enumerantC = enumType.Enumerants.Add("grapefruit");
    var enumerantD = enumType.Enumerants.Add("lychee");

    __VERIFY(enumType.BaseType.Name == "int", "unexpected base type of enum");
    __VERIFY(enumType.Size == 4, "unexpected size of enum");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.apple.value == 0, "unexpected type system value of enumerant 'apple'");
    __VERIFY(fooTy.fields.orange.value == 1, "unexpected type system value of enumerant 'orange'");
    __VERIFY(fooTy.fields.grapefruit.value == 2, "unexpected type system value of enumerant 'grapefruit'");
    __VERIFY(fooTy.fields.lychee.value == 3, "unexpected type system value of enumerant 'lychee'");

    enumerantC.Delete();

    fooTy == host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.apple.value == 0, "unexpected type system value of enumerant 'apple' post delete");
    __VERIFY(fooTy.fields.orange.value == 1, "unexpected type system value of enumerant 'orange' post delete");
    __VERIFY(fooTy.fields.lychee.value == 2, "unexpected type system value of enumerant 'lychee' post delete");

    __VERIFY(fooTy.fields.grapefruit === undefined, "unexpected abillity to see deleted enumerant at type system");

    enumType.Delete();
    return true;
}

// Test_EnumWithNonDefaultBaseType:
//
// Verify that we can create enums with a non default (e.g.: not int) base type and get automatic layout of 
// enumerants and get back expected results.
//
function Test_EnumWithNonDefaultBaseType()
{
    var name = __getUniqueName("fruit");
    var enumType = __symbolBuilderSymbols.Types.CreateEnum(name, "char");

    var enumerantA = enumType.Enumerants.Add("apple");
    var enumerantB = enumType.Enumerants.Add("orange");
    var enumerantC = enumType.Enumerants.Add("grapefruit");
    var enumerantD = enumType.Enumerants.Add("lychee");

    __VERIFY(enumType.BaseType.Name == "char", "unexpected base type of enum");
    __VERIFY(enumType.Size == 1, "unexpected size of enum");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.apple.value == 0, "unexpected type system value of enumerant 'apple'");
    __VERIFY(fooTy.fields.orange.value == 1, "unexpected type system value of enumerant 'orange'");
    __VERIFY(fooTy.fields.grapefruit.value == 2, "unexpected type system value of enumerant 'grapefruit'");
    __VERIFY(fooTy.fields.lychee.value == 3, "unexpected type system value of enumerant 'lychee'");

    enumType.Delete();
    return true;
}

// Test_EnumMoveEnumerant:
//
// Verify that we can create enums with mixed layout (some auto, some manual) of enumerants, move enumerants
// around, and get expected results.
//
function Test_EnumMoveEnumerant()
{
    var name = __getUniqueName("fruit");
    var enumType = __symbolBuilderSymbols.Types.CreateEnum(name);

    var enumerantA = enumType.Enumerants.Add("apple");
    var enumerantB = enumType.Enumerants.Add("orange");
    var enumerantC = enumType.Enumerants.Add("grapefruit", 42);
    var enumerantD = enumType.Enumerants.Add("lychee");

    __VERIFY(enumType.BaseType.Name == "int", "unexpected base type of enum");
    __VERIFY(enumType.Size == 4, "unexpected size of enum");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.apple.value == 0, "unexpected type system value of enumerant 'apple'");
    __VERIFY(fooTy.fields.orange.value == 1, "unexpected type system value of enumerant 'orange'");
    __VERIFY(fooTy.fields.grapefruit.value == 42, "unexpected type system value of enumerant 'grapefruit'");
    __VERIFY(fooTy.fields.lychee.value == 43, "unexpected type system value of enumerant 'lychee'");

    enumerantA.MoveBefore(99);
    fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.orange.value == 0, "unexpected type system value of enumerant 'orange' post move 1");
    __VERIFY(fooTy.fields.grapefruit.value == 42, "unexpected type system value of enumerant 'grapefruit' post move 1");
    __VERIFY(fooTy.fields.lychee.value == 43, "unexpected type system value of enumerant 'lychee' post move 1");
    __VERIFY(fooTy.fields.apple.value == 44, "unexpected type system value of enumerant 'apple' post move 1");

    enumerantC.MoveBefore(enumerantB);
    fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.grapefruit.value == 42, "unexpected type system value of enumerant 'grapefruit' post move 2");
    __VERIFY(fooTy.fields.orange.value == 43, "unexpected type system value of enumerant 'orange' post move 2");
    __VERIFY(fooTy.fields.lychee.value == 44, "unexpected type system value of enumerant 'lychee' post move 2");
    __VERIFY(fooTy.fields.apple.value == 45, "unexpected type system value of enumerant 'apple' post move 2");

    enumerantC.MoveBefore(99);
    __VERIFY(fooTy.fields.orange.value == 0, "unexpected type system value of enumerant 'orange' post move 3");
    __VERIFY(fooTy.fields.lychee.value == 1, "unexpected type system value of enumerant 'lychee' post move 3");
    __VERIFY(fooTy.fields.apple.value == 2, "unexpected type system value of enumerant 'apple' post move 3");
    __VERIFY(fooTy.fields.grapefruit.value == 42, "unexpected type system value of enumerant 'grapefruit' post move 3");

    enumType.Delete();
    return true;
}

// Test_BitfieldWithAutoLayout:
//
// Verify that we can create UDTs with bitfields with auto layout and get expected results.
//
function Test_BitfieldWithAutoLayout()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    var fldA = foo.Fields.Add("a", "char: 1");              // [0, 1)   0:0
    var fldB = foo.Fields.Add("b", "char: 3");              // [0, 1)   1:3
    var fldC = foo.Fields.Add("c", "char: 2");              // [0, 1)   4:5
    var fldD = foo.Fields.Add("d", "char: 4");              // [1, 2)   0:3
    var fldE = foo.Fields.Add("e", "int: 30");              // [4, 8)   0:29
    var fldF = foo.Fields.Add("f", "int");                  // [8, 12)  ---

    __VERIFY(fldA.Offset == 0, "unexpected offset of 'a'");
    __VERIFY(fldA.BitFieldPosition == 0, "unexpected bitfield position of 'a'");
    __VERIFY(fldA.BitFieldLength == 1, "unexpected bitfield length of 'a'");
    __VERIFY(fldB.Offset == 0, "unexpected offset of 'b'");
    __VERIFY(fldB.BitFieldPosition == 1, "unexpected bitfield position of 'b'");
    __VERIFY(fldB.BitFieldLength == 3, "unexpected bitfield length of 'b'");
    __VERIFY(fldC.Offset == 0, "unexpected offset of 'c'");
    __VERIFY(fldC.BitFieldPosition == 4, "unexpected bitfield position of 'c'");
    __VERIFY(fldC.BitFieldLength == 2, "unexpected bitfield length of 'c'");
    __VERIFY(fldD.Offset == 1, "unexpected offset of 'd'");
    __VERIFY(fldD.BitFieldPosition == 0, "unexpected bitfield position of 'd'");
    __VERIFY(fldD.BitFieldLength == 4, "unexpected bitfield length of 'd'");
    __VERIFY(fldE.Offset == 4, "unexpected offset of 'e'");
    __VERIFY(fldE.BitFieldPosition == 0, "unexpected bitfield position of 'e'");
    __VERIFY(fldE.BitFieldLength == 30, "unexpected bitfield length of 'e'");
    __VERIFY(fldF.Offset == 8, "unexpected offset of 'f'");
    __VERIFY(!fldF.BitFieldPosition, "unexpected bitfield property of non bitfield 'f'");
    __VERIFY(!fldF.BitFieldLength, "unexpected bitfield property of non bitfield 'f'");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.a.offset == 0, "unexpected type system offset for 'a'");
    __VERIFY(fooTy.fields.a.type.isBitField == true, "bitfield does not show up in type system for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.length == 1, "unexpected type system bitfield length for 'a'");
    __VERIFY(fooTy.fields.b.offset == 0, "unexpected type system offset for 'b'");
    __VERIFY(fooTy.fields.b.type.isBitField == true, "bitfield does not show up in type system for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.lsb == 1, "unexpected type system bitfield position for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.length == 3, "unexpected type system bitfield length for 'b'");
    __VERIFY(fooTy.fields.c.offset == 0, "unexpected type system offset for 'c'");
    __VERIFY(fooTy.fields.c.type.isBitField == true, "bitfield does not show up in type system for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.lsb == 4, "unexpected type system bitfield position for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.length == 2, "unexpected type system bitfield length for 'c'");
    __VERIFY(fooTy.fields.d.offset == 1, "unexpected type system offset for 'd'");
    __VERIFY(fooTy.fields.d.type.isBitField == true, "bitfield does not show up in type system for 'd'");
    __VERIFY(fooTy.fields.d.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'd'");
    __VERIFY(fooTy.fields.d.type.bitFieldPositions.length == 4, "unexpected type system bitfield length for 'd'");
    __VERIFY(fooTy.fields.e.offset == 4, "unexpected type system offset for 'e'");
    __VERIFY(fooTy.fields.e.type.isBitField == true, "bitfield does not show up in type system for 'e'");
    __VERIFY(fooTy.fields.e.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'e'");
    __VERIFY(fooTy.fields.e.type.bitFieldPositions.length == 30, "unexpected type system bitfield length for 'e'");
    __VERIFY(fooTy.fields.f.offset == 8, "unexpected type system offset for 'f'");
    __VERIFY(fooTy.fields.f.type.isBitField == false, "bitfield incorrectly shows up in type system for 'f'");

    foo.Delete();
    return true;
}

// Test_BitfieldWithManualLayout:
//
// Verify that we can create UDTs with bitfields with manual layout and get expected results.
//
function Test_BitfieldWithManualLayout()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    var fldA = foo.Fields.Add("a", "char", 0, { BitFieldPosition: 7, BitFieldLength: 1 });  // [0, 1)   7:7
    var fldB = foo.Fields.Add("b", "char", 0, { BitFieldPosition: 3, BitFieldLength: 2 });  // [0, 1)   3:4
    var fldC = foo.Fields.Add("c", "char", 1, { BitFieldPosition: 2, BitFieldLength: 4 });  // [1, 2)   2:5

    __VERIFY(fldA.Offset == 0, "unexpected offset of 'a'");
    __VERIFY(fldA.BitFieldPosition == 7, "unexpected bitfield position of 'a'");
    __VERIFY(fldA.BitFieldLength == 1, "unexpected bitfield length of 'a'");
    __VERIFY(fldB.Offset == 0, "unexpected offset of 'b'");
    __VERIFY(fldB.BitFieldPosition == 3, "unexpected bitfield position of 'b'");
    __VERIFY(fldB.BitFieldLength == 2, "unexpected bitfield length of 'b'");
    __VERIFY(fldC.Offset == 1, "unexpected offset of 'c'");
    __VERIFY(fldC.BitFieldPosition == 2, "unexpected bitfield position of 'c'");
    __VERIFY(fldC.BitFieldLength == 4, "unexpected bitfield length of 'c'");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.a.offset == 0, "unexpected type system offset for 'a'");
    __VERIFY(fooTy.fields.a.type.isBitField == true, "bitfield does not show up in type system for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.lsb == 7, "unexpected type system bitfield position for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.length == 1, "unexpected type system bitfield length for 'a'");
    __VERIFY(fooTy.fields.b.offset == 0, "unexpected type system offset for 'b'");
    __VERIFY(fooTy.fields.b.type.isBitField == true, "bitfield does not show up in type system for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.lsb == 3, "unexpected type system bitfield position for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.length == 2, "unexpected type system bitfield length for 'b'");
    __VERIFY(fooTy.fields.c.offset == 1, "unexpected type system offset for 'c'");
    __VERIFY(fooTy.fields.c.type.isBitField == true, "bitfield does not show up in type system for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.lsb == 2, "unexpected type system bitfield position for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.length == 4, "unexpected type system bitfield length for 'c'");

    foo.Delete();
    return true;
}

// Test_BitfieldWithMixedAutoManualLayout:
//
// Verify that we can create UDTs with bitfields having mixed auto and manual layout and get expected results.
//
function Test_BitfieldWithMixedAutoManualLayout()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    var fldA = foo.Fields.Add("a", "char", 0, { BitFieldPosition: 2, BitFieldLength: 2 });  // [0, 1) 2:3
    var fldB = foo.Fields.Add("b", "char:2");                                               // [0, 1) 4:5
    var fldC = foo.Fields.Add("c", "char", 0, { BitFieldPosition: 6, BitFieldLength: 1 });  // [0, 1) 6:6
    var fldD = foo.Fields.Add("d", "char:4");                                               // [1, 2) 0:3
    var fldE = foo.Fields.Add("e", "int", 4, { BitFieldPosition: 20, BitFieldLength: 5 });  // [4, 7) 20:24
    var fldF = foo.Fields.Add("f", "char:6");                                               // [8, 9) 0:5

    __VERIFY(fldA.Offset == 0, "unexpected offset of 'a'");
    __VERIFY(fldA.BitFieldPosition == 2, "unexpected bitfield position of 'a'");
    __VERIFY(fldA.BitFieldLength == 2, "unexpected bitfield length of 'a'");
    __VERIFY(fldB.Offset == 0, "unexpected offset of 'b'");
    __VERIFY(fldB.BitFieldPosition == 4, "unexpected bitfield position of 'b'");
    __VERIFY(fldB.BitFieldLength == 2, "unexpected bitfield length of 'b'");
    __VERIFY(fldC.Offset == 0, "unexpected offset of 'c'");
    __VERIFY(fldC.BitFieldPosition == 6, "unexpected bitfield position of 'c'");
    __VERIFY(fldC.BitFieldLength == 1, "unexpected bitfield length of 'c'");
    __VERIFY(fldD.Offset == 1, "unexpected offset of 'd'");
    __VERIFY(fldD.BitFieldPosition == 0, "unexpected bitfield position of 'd'");
    __VERIFY(fldD.BitFieldLength == 4, "unexpected bitfield length of 'd'");
    __VERIFY(fldE.Offset == 4, "unexpected offset of 'e'");
    __VERIFY(fldE.BitFieldPosition == 20, "unexpected bitfield position of 'e'");
    __VERIFY(fldE.BitFieldLength == 5, "unexpected bitfield length of 'e'");
    __VERIFY(fldF.Offset == 8, "unexpected offset of 'e'");
    __VERIFY(fldF.BitFieldPosition == 0, "unexpected bitfield position of 'e'");
    __VERIFY(fldF.BitFieldLength == 6, "unexpected bitfield length of 'e'");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.a.offset == 0, "unexpected type system offset for 'a'");
    __VERIFY(fooTy.fields.a.type.isBitField == true, "bitfield does not show up in type system for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.lsb == 2, "unexpected type system bitfield position for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.length == 2, "unexpected type system bitfield length for 'a'");
    __VERIFY(fooTy.fields.b.offset == 0, "unexpected type system offset for 'b'");
    __VERIFY(fooTy.fields.b.type.isBitField == true, "bitfield does not show up in type system for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.lsb == 4, "unexpected type system bitfield position for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.length == 2, "unexpected type system bitfield length for 'b'");
    __VERIFY(fooTy.fields.c.offset == 0, "unexpected type system offset for 'c'");
    __VERIFY(fooTy.fields.c.type.isBitField == true, "bitfield does not show up in type system for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.lsb == 6, "unexpected type system bitfield position for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.length == 1, "unexpected type system bitfield length for 'c'");
    __VERIFY(fooTy.fields.d.offset == 1, "unexpected type system offset for 'd'");
    __VERIFY(fooTy.fields.d.type.isBitField == true, "bitfield does not show up in type system for 'd'");
    __VERIFY(fooTy.fields.d.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'd'");
    __VERIFY(fooTy.fields.d.type.bitFieldPositions.length == 4, "unexpected type system bitfield length for 'd'");
    __VERIFY(fooTy.fields.e.offset == 4, "unexpected type system offset for 'e'");
    __VERIFY(fooTy.fields.e.type.isBitField == true, "bitfield does not show up in type system for 'e'");
    __VERIFY(fooTy.fields.e.type.bitFieldPositions.lsb == 20, "unexpected type system bitfield position for 'e'");
    __VERIFY(fooTy.fields.e.type.bitFieldPositions.length == 5, "unexpected type system bitfield length for 'e'");
    __VERIFY(fooTy.fields.f.offset == 8, "unexpected type system offset for 'f'");
    __VERIFY(fooTy.fields.f.type.isBitField == true, "bitfield does not show up in type system for 'f'");
    __VERIFY(fooTy.fields.f.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'f'");
    __VERIFY(fooTy.fields.f.type.bitFieldPositions.length == 6, "unexpected type system bitfield length for 'f'");

    foo.Delete();
    return true;
}

// Test_BitfieldMoveField:
//
// Verify that we can create UDTs with bitfields having automatic layout, move fields around, and get
// expected results.
//
function Test_BitfieldMoveField()
{
    var name = __getUniqueName("foo");
    var foo = __symbolBuilderSymbols.Types.Create(name);

    var fldA = foo.Fields.Add("a", "char:2");           // [0, 1) 0:1
    var fldB = foo.Fields.Add("b", "char:3");           // [0, 1) 2:4
    var fldC = foo.Fields.Add("c", "char:3");           // [0, 1) 5:7
    var fldD = foo.Fields.Add("d", "char:5");           // [1, 2) 0:4
    var fldE = foo.Fields.Add("e", "int:20");           // [4, 8) 0:19
    var fldF = foo.Fields.Add("f", "int:5");            // [4, 8) 20:24

    __VERIFY(fldA.Offset == 0, "unexpected offset of 'a'");
    __VERIFY(fldA.BitFieldPosition == 0, "unexpected bitfield position of 'a'");
    __VERIFY(fldA.BitFieldLength == 2, "unexpected bitfield length of 'a'");
    __VERIFY(fldB.Offset == 0, "unexpected offset of 'b'");
    __VERIFY(fldB.BitFieldPosition == 2, "unexpected bitfield position of 'b'");
    __VERIFY(fldB.BitFieldLength == 3, "unexpected bitfield length of 'b'");
    __VERIFY(fldC.Offset == 0, "unexpected offset of 'c'");
    __VERIFY(fldC.BitFieldPosition == 5, "unexpected bitfield position of 'c'");
    __VERIFY(fldC.BitFieldLength == 3, "unexpected bitfield length of 'c'");
    __VERIFY(fldD.Offset == 1, "unexpected offset of 'd'");
    __VERIFY(fldD.BitFieldPosition == 0, "unexpected bitfield position of 'd'");
    __VERIFY(fldD.BitFieldLength == 5, "unexpected bitfield length of 'd'");
    __VERIFY(fldE.Offset == 4, "unexpected offset of 'e'");
    __VERIFY(fldE.BitFieldPosition == 0, "unexpected bitfield position of 'e'");
    __VERIFY(fldE.BitFieldLength == 20, "unexpected bitfield length of 'e'");
    __VERIFY(fldF.Offset == 4, "unexpected offset of 'e'");
    __VERIFY(fldF.BitFieldPosition == 20, "unexpected bitfield position of 'e'");
    __VERIFY(fldF.BitFieldLength == 5, "unexpected bitfield length of 'e'");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.a.offset == 0, "unexpected type system offset for 'a'");
    __VERIFY(fooTy.fields.a.type.isBitField == true, "bitfield does not show up in type system for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.length == 2, "unexpected type system bitfield length for 'a'");
    __VERIFY(fooTy.fields.b.offset == 0, "unexpected type system offset for 'b'");
    __VERIFY(fooTy.fields.b.type.isBitField == true, "bitfield does not show up in type system for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.lsb == 2, "unexpected type system bitfield position for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.length == 3, "unexpected type system bitfield length for 'b'");
    __VERIFY(fooTy.fields.c.offset == 0, "unexpected type system offset for 'c'");
    __VERIFY(fooTy.fields.c.type.isBitField == true, "bitfield does not show up in type system for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.lsb == 5, "unexpected type system bitfield position for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.length == 3, "unexpected type system bitfield length for 'c'");
    __VERIFY(fooTy.fields.d.offset == 1, "unexpected type system offset for 'd'");
    __VERIFY(fooTy.fields.d.type.isBitField == true, "bitfield does not show up in type system for 'd'");
    __VERIFY(fooTy.fields.d.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'd'");
    __VERIFY(fooTy.fields.d.type.bitFieldPositions.length == 5, "unexpected type system bitfield length for 'd'");
    __VERIFY(fooTy.fields.e.offset == 4, "unexpected type system offset for 'e'");
    __VERIFY(fooTy.fields.e.type.isBitField == true, "bitfield does not show up in type system for 'e'");
    __VERIFY(fooTy.fields.e.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'e'");
    __VERIFY(fooTy.fields.e.type.bitFieldPositions.length == 20, "unexpected type system bitfield length for 'e'");
    __VERIFY(fooTy.fields.f.offset == 4, "unexpected type system offset for 'f'");
    __VERIFY(fooTy.fields.f.type.isBitField == true, "bitfield does not show up in type system for 'f'");
    __VERIFY(fooTy.fields.f.type.bitFieldPositions.lsb == 20, "unexpected type system bitfield position for 'f'");
    __VERIFY(fooTy.fields.f.type.bitFieldPositions.length == 5, "unexpected type system bitfield length for 'f'");

    //
    // Now move D to before B so that we have:
    //
    //     a: char:2                [0, 1) 0:1
    //     d: char:5                [0, 1) 2:6
    //     b: char:3                [1, 2) 0:2
    //     c: char:3                [1, 2) 3:5
    //     e: int: 20               [4, 8) 0:19
    //     f: int: 5                [4, 8) 20:24
    //
    // And reverify at both levels.
    //
    fldD.MoveBefore(fldB);

    __VERIFY(fldA.Offset == 0, "unexpected offset of 'a'");
    __VERIFY(fldA.BitFieldPosition == 0, "unexpected bitfield position of 'a'");
    __VERIFY(fldA.BitFieldLength == 2, "unexpected bitfield length of 'a'");
    __VERIFY(fldD.Offset == 0, "unexpected offset of 'd'");
    __VERIFY(fldD.BitFieldPosition == 2, "unexpected bitfield position of 'd'");
    __VERIFY(fldD.BitFieldLength == 5, "unexpected bitfield length of 'd'");
    __VERIFY(fldB.Offset == 1, "unexpected offset of 'b'");
    __VERIFY(fldB.BitFieldPosition == 0, "unexpected bitfield position of 'b'");
    __VERIFY(fldB.BitFieldLength == 3, "unexpected bitfield length of 'b'");
    __VERIFY(fldC.Offset == 1, "unexpected offset of 'c'");
    __VERIFY(fldC.BitFieldPosition == 3, "unexpected bitfield position of 'c'");
    __VERIFY(fldC.BitFieldLength == 3, "unexpected bitfield length of 'c'");
    __VERIFY(fldE.Offset == 4, "unexpected offset of 'e'");
    __VERIFY(fldE.BitFieldPosition == 0, "unexpected bitfield position of 'e'");
    __VERIFY(fldE.BitFieldLength == 20, "unexpected bitfield length of 'e'");
    __VERIFY(fldF.Offset == 4, "unexpected offset of 'e'");
    __VERIFY(fldF.BitFieldPosition == 20, "unexpected bitfield position of 'e'");
    __VERIFY(fldF.BitFieldLength == 5, "unexpected bitfield length of 'e'");

    var fooTy = host.getModuleType("notepad.exe", name);
    __VERIFY(fooTy.fields.a.offset == 0, "unexpected type system offset for 'a'");
    __VERIFY(fooTy.fields.a.type.isBitField == true, "bitfield does not show up in type system for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'a'");
    __VERIFY(fooTy.fields.a.type.bitFieldPositions.length == 2, "unexpected type system bitfield length for 'a'");
    __VERIFY(fooTy.fields.d.offset == 0, "unexpected type system offset for 'd'");
    __VERIFY(fooTy.fields.d.type.isBitField == true, "bitfield does not show up in type system for 'd'");
    __VERIFY(fooTy.fields.d.type.bitFieldPositions.lsb == 2, "unexpected type system bitfield position for 'd'");
    __VERIFY(fooTy.fields.d.type.bitFieldPositions.length == 5, "unexpected type system bitfield length for 'd'");
    __VERIFY(fooTy.fields.b.offset == 1, "unexpected type system offset for 'b'");
    __VERIFY(fooTy.fields.b.type.isBitField == true, "bitfield does not show up in type system for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'b'");
    __VERIFY(fooTy.fields.b.type.bitFieldPositions.length == 3, "unexpected type system bitfield length for 'b'");
    __VERIFY(fooTy.fields.c.offset == 1, "unexpected type system offset for 'c'");
    __VERIFY(fooTy.fields.c.type.isBitField == true, "bitfield does not show up in type system for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.lsb == 3, "unexpected type system bitfield position for 'c'");
    __VERIFY(fooTy.fields.c.type.bitFieldPositions.length == 3, "unexpected type system bitfield length for 'c'");
    __VERIFY(fooTy.fields.e.offset == 4, "unexpected type system offset for 'e'");
    __VERIFY(fooTy.fields.e.type.isBitField == true, "bitfield does not show up in type system for 'e'");
    __VERIFY(fooTy.fields.e.type.bitFieldPositions.lsb == 0, "unexpected type system bitfield position for 'e'");
    __VERIFY(fooTy.fields.e.type.bitFieldPositions.length == 20, "unexpected type system bitfield length for 'e'");
    __VERIFY(fooTy.fields.f.offset == 4, "unexpected type system offset for 'f'");
    __VERIFY(fooTy.fields.f.type.isBitField == true, "bitfield does not show up in type system for 'f'");
    __VERIFY(fooTy.fields.f.type.bitFieldPositions.lsb == 20, "unexpected type system bitfield position for 'f'");
    __VERIFY(fooTy.fields.f.type.bitFieldPositions.length == 5, "unexpected type system bitfield length for 'f'");

    foo.Delete();
    return true;
}

//**************************************************************************
// Initialization:
//

// __testSuite:
//
// Defines the test suite that we are going to run in the order it will be run.  This is returned
// from the initializeTests() method to tell the harness what to run and what each test should be called.
//
var __testSuite =
[
    //
    // General Tests:
    //
    { Name: "VerifyBuilderSymbols", Code: Test_VerifyBuilderSymbols },

    //
    // UDT Specific Tests:
    //
    { Name: "CreateAndDestroyEmptyUdt", Code: Test_CreateAndDestroyEmptyUdt },
    { Name: "UdtWithBasicFields", Code: Test_UdtWithBasicFields },
    { Name: "AutoLayoutAlignment", Code: Test_AutoLayoutAlignment },
    { Name: "NestedStructsWithAutoAlignment", Code: Test_NestedStructsWithAutoAlignment },
    { Name: "StructManualLayout", Code: Test_StructManualLayout },
    { Name: "StructMixedManualAutoLayout", Code: Test_StructMixedManualAutoLayout },
    { Name: "StructDeleteFields", Code: Test_StructDeleteFields },
    { Name: "StructChangeFieldType", Code: Test_StructChangeFieldType },
    { Name: "StructMoveField", Code: Test_StructMoveField },

    //
    // Pointer/Array Tests:
    //
    { Name: "ArrayCreation", Code: Test_ArrayCreation },
    { Name: "PointerCreation", Code: Test_PointerCreation },

    //
    // Enum Tests:
    //
    {Name: "EnumWithAutoLayout", Code: Test_EnumWithAutoLayout },
    {Name: "EnumWithMixedLayout", Code: Test_EnumWithMixedLayout },
    {Name: "EnumDeleteEnumerants", Code: Test_EnumDeleteEnumerants },
    {Name: "EnumWithNonDefaultBaseType", Code: Test_EnumWithNonDefaultBaseType },
    {Name: "EnumMoveEnumerant", Code: Test_EnumMoveEnumerant },

    //
    // Bitfield Tests:
    //
    {Name: "BitfieldWithAutoLayout", Code: Test_BitfieldWithAutoLayout },
    {Name: "BitfieldWithManualLayout", Code: Test_BitfieldWithManualLayout },
    {Name: "BitfieldWithMixedAutoManualLayout", Code: Test_BitfieldWithMixedAutoManualLayout },
    {Name: "BitfieldMoveField", Code: Test_BitfieldMoveField }

];

// initializeTests:
//
// The initializer that will be called to initialize the test suite.  It must return an array
// of test cases.
//
function initializeTests()
{
    //
    // For test initialization, we will ensure that the symbol builder extension is loaded
    // and subsequently create symbol builder symbols for notepad making sure that they
    // should be actively loaded.
    //
    __ctl = host.namespace.Debugger.Utility.Control;
    __ctl.ExecuteCommand(".load SymbolBuilderComposition.dll");
    __symBuilder = host.namespace.Debugger.Utility.SymbolBuilder;
    __symbolBuilderSymbols = __symBuilder.CreateSymbols("notepad.exe");
    __ctl.ExecuteCommand(".reload");

    return __testSuite;
}

//**************************************************************************
// General JS Initialization:
//

function initializeScript()
{
    return [new host.apiVersionSupport(1, 7)];
}
