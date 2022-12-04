This is a currently **IN DEVELOPMENT** extension DLL which bridges the 3.11 version of CPython to the data model
as a script provider.  The eventual intent is that it will allow for extensions to be written in Python much as
they are able to be written in JavaScript today.

Note that this will also serve as sample code for the data model APIs which allow for dynamic language bridging.

This code is **INCREDIBLY EARLY** and is not expected to be largely useful at this point in time.  Experiment at your
own risk.

# Getting Started

As the code here is **EARLY DEVELOPMENT**, it is only intended to build x64/debug.  After building the extension DLL,
you can load it in WinDbg or WinDbg Preview with a standard ".load" and subsequently use .scriptload (or the script
ribbon) to load/execute scripts.

## Building the Sample

The following steps should be taken to build the sample:

* Install Python 3.11 for Windows (installation should be local user / %LocalAppData%\Programs\Python\Python311
* Open the Visual Studio Solution.
* Build x64\debug

## Using the Sample

The following steps should be taken to use the sample:

* Download the Python 3.11 Windows embeddable package (64-bit) and extract it into a directory (e.g.: d:\xtn)
* Copy the built PyProvider.dll (& pdb) to the directory with the embeddable package (e.g.: d:\xtn)
* .load the PyProvider.dll from the directory in the above steps
* .scriptload a script

## Debugging the Sample

As this is an **EARLY IN DEVELOPMENT** sample, many things will not work properly.  It may be necessary to debug
the sample.  It is significantly easier to have a debug build of Python to accomplish this:

* Clone the Python GitHub repo
* Switch to the 3.11 branch
* Open the Visual Studio solution and build it x64\debug
* Take the resulting python311_d.dll/pdb and copy it over the Python311.dll in the embeddable package directory created earlier
* Attach one instance of the debugger (the outer debugger) to another (the one running scripts).  Typically, it is easiest to use WinDbg Preview to debug WinDbg Classic where WinDbg Classic is running the scripts.

Within the *Diagnostics* directory, there is a PythonXtn.js script which is the start of a WinDbg data model extension
to help with debugging Python issues.  It requires the PDB for the embedded Python, so building a debug build from
source is helpful here.

