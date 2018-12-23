"use strict";

/*************************************************
Copyright (c) Microsoft Corporation. All rights reserved.
Licensed under the MIT License.
*************************************************/

function invokeScript()
{
    var path = "C:\\Users\\aluhrs\\Desktop\\HelloWorld.txt";

    if(host.namespace.Debugger.Utility.FileSystem.FileExists(path)){
        readFile(path);
    }else{
        writeFile(path);
    }

}

function writeFile(name)
{
  var file = host.namespace.Debugger.Utility.FileSystem.CreateFile(name);
  var textWriter = host.namespace.Debugger.Utility.FileSystem.CreateTextWriter(file);
  textWriter.WriteLine("Hello World");
  file.Close();
}

function readFile(name)
{
  var file = host.namespace.Debugger.Utility.FileSystem.OpenFile(name);
  var textReader = host.namespace.Debugger.Utility.FileSystem.CreateTextReader(file);
  host.diagnostics.debugLog(textReader.ReadLine());
  file.Close();
}
