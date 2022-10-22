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
    { Name: "VerifyBuilderSymbols", Code: Test_VerifyBuilderSymbols },
    { Name: "CreateAndDestroyEmptyUdt", Code: Test_CreateAndDestroyEmptyUdt },
    { Name: "UdtWithBasicFields", Code: Test_UdtWithBasicFields },
    { Name: "AutoLayoutAlignment", Code: Test_AutoLayoutAlignment },
    { Name: "NestedStructsWithAutoAlignment", Code: Test_NestedStructsWithAutoAlignment },
    { Name: "StructManualLayout", Code: Test_StructManualLayout },
    { Name: "StructMixedManualAutoLayout", Code: Test_StructMixedManualAutoLayout}
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
