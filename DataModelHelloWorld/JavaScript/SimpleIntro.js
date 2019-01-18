"use strict";

//**************************************************************************
// SimpleIntro.js:
//
// A sample JavaScript debugger extension which adds a new example property "Hello"
// to the debugger's notion of a process.
//
// In order to better understand the varying ways to write a debugger extension with
// the data model, there are three versions of this extension:
// 
//     JavaScript: This version -- written in JavaScript
//     C++17     : A version written against the Data Model C++17 Client Library
//     COM       : A version written against the raw COM ABI (only using WRL for COM helpers)
//
// Any names in the script which begin with '__' are private to the script and are automatically
// made inaccessible outside the script by the JavaScript provider.
//
//**************************************************************************

// __HelloObject:
//
// A sample JavaScript object which will get returned outside of this script.
//
// [C++17: This is equivalent to the HelloObject and Details::Hello classes]
// [COM  : This is equivalent to the data model created in HelloExtension::Initialize]
//
class __HelloObject
{
    constructor(text)
    {
        this.__text = text;
    }

    // [C++17: This is implemented via data binding in the HelloObject constructor]
    // [COM  : This is equivalent to the WorldProperty class]
    get World()
    {
        return this.__text;
    }

    // [C++17: This is equivalent to the Get_Test method on the HelloObject class]
    // [COM  : This is equivalent to the TestProperty class]
    get Test()
    {
        return {A: 42, B: "Hello World"};
    }

    // [C++17: This is equivalent to the GetStringConversion method on the HelloObject class]
    // [COM  : This is equivalent to the HelloStringConversion class]
    toString()
    {
        return "JavaScript Object: " + this.__text;
    }
}

// __HelloExtension:
//
// A class which will extend the debugger's notion of a process with some new properties.  
// 
// [C++17: This is equivalent to the HelloExtension class]
// [COM  : This is equivalent to the HelloExtension and HelloExtensionModel classes]
//
class __HelloExtension
{
    // [C++17: This is equivalent to the Get_Hello method on the HelloExtension class]
    // [COM  : This is equivalent to the HelloProperty class]
    get Hello()
    {
        return new __HelloObject("Hello World");
    }
};

// initializeScript():
//
// This is the function which will be called after the script is loaded in order to inform the debugger
// how the script is attached to the data model.  An array of initialization records is returned.
//
// The target cannot be touched during this method.
//
// See http://aka.ms/JsDbgExt
//
function initializeScript()
{
    //
    // apiVersionSupport: Indicate that we support version 1.3 of the JsProvider API
    // namedModelParent : Indicate that the class __ProcessExtension extends "Debugger.Models.Process"
    //
    return [new host.apiVersionSupport(1, 3),
            new host.namedModelParent(__HelloExtension, "Debugger.Models.Process")];
}
