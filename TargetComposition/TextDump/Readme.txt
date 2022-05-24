//**************************************************************************
// Debugger Target Composition API Sample Walkthrough
//
// "Text Dump" File Format Handler
//**************************************************************************

//*************************************************
// INTRODUCTION
//*************************************************

The debugger's target composition API (DbgServices.h) allows plug-ins to come in and either teach
the debugger new functionality or alter behaviors of the debugger.  At present, this API is *MOSTLY*
focused on post-mortem targets (e.g.: dumps or similar) although certain functionality can be
useful in live targets as well.

The main notion of "target composition" is that a target that is being debugged is not a monolithic
construct.  It is, instead, a container of interdependent services, each of which can be implemented
by different plug-ins.  These services, in aggregate, provide the functionality which you observe in
the debugger.  As an example, when you issue a command in the debugger such as "db 1000", the
implementation of that command will go to the service container and find the appropriate service
to satisfy the request.  In the case of "db 1000", the debugger will ask for the virtual memory service
as identified by a GUID (DEBUG_SERVICE_VIRTUAL_MEMORY), get the memory access interface (ISvcMemoryAccess) 
from that service, and call a method (ReadMemory) on that interface.

Plug-ins for target composition are DbgEng style extensions that can be loaded and utilized in several different 
ways.  They can either be brought in to handle a particular post-mortem file format or they can be brought in
dynamically when certain conditions are met (e.g.: the underlying operating system is Windows or Linux,
the debugger is targeting a kernel mode or user mode target, a particular module is present in the module list,
etc...).

This sample is an example of the first of these (handling a particular file format).  The sample plug-in itself
handles a new post-mortem file format that we'll call a "text dump" format.  In reality, this is a UTF-8 or UTF-16LE
text file which has some headers above what is, for the most part, a cut and paste of commands executed in a debugger
session.  The "text dump" which is included with this sample is taken from a post-mortem dump of notepad.exe with
the file/open dialog active.

//*************************************************
// "TEXT DUMP" FILE FORMAT
//*************************************************

The text dump format that we handle here has a few characteristics:

    * The file begins with a header that must be a line "*** TEXTUAL DEMONSTRATION FILE"

    * Sections in the file must begin with "*** <SECTION NAME>" and must end with a blank line

    * Comments and blank lines are allowed between sections (but not in sections)

    * The dump is always assumed to be for the x64 architecture

The format allows for four sections (each of which is optional -- you can experiment with the plug-in
and various behaviors in the debugger by removing sections)

    * The stack section.  The header is "*** STACK" and the data is a cut and paste of a "k" command
      in the debugger for the x64 target.

    * The memory section.  The header is "*** MEMORY" and the data is a cut and paste of a "db" command
      in the debugger for a particular address.  "??" cannot be present in the memory bytes.  The addresses
      within the memory section need not be contiguous or in increasing order of virtual addresses; however, 
      there should not be multiple copies of the same memory addresses within the section.

    * The module information section.  The header is "*** MODULEINFO" and the data is a cut and paste of
      pieces of an lmvm command.  Each line contains the following:

      <Start Address> <End Address> <Module Name> <Quoted Module Path> <Time Date Stamp> <Size Of Image>

    * The registers section.  The header is "*** REGISTERS" and the data is a cut and paste of a "r" command
      in the debugger for the x64 target.

//*************************************************
// BUILDING AND INSTALLATION
//*************************************************

The following steps should get the sample built and installed.  These installation steps will add the plug-in
for *EVERY* version of the debugger used by the current user account.  Note that these instructions are specifically
intended for the x64 version of the debugger.

    * Build the sample with the included Visual Studio solution

    * Go to the %LocalAppData%\dbg\UserExtensions folder

    * Make a directory named "TextDumpCompositionPackage".  This directory must have this name because
      it is the package name as indicated by the manifest:

      <Name>TextDumpCompositionPackage</Name>

    * Inside "TextDumpCompositionPackage", copy "TextDump_GalleryManifest.xml" and rename it to 
      "GalleryManifest.xml".  More recent debuggers may not require the rename step.

    * Inside "TextDumpCompositionPackage", make a new directory "1.0.0.0".  This directory must have this name
      because it is the package version as indicated by the manifest:

      <Version>1.0.0.0</Version>

    * Inside "1.0.0.0", make a new directory "amd64".  This directory must have this name because it is explicitly
      specified in the manifest as the location for the amd64 version of the extension DLL:

      <File Architecture="amd64" Module="amd64\TextDumpComposition.dll" FilePathKind="RepositoryRelative"/>

    * Copy the built TextDumpComposition.dll into the created "amd64" directory.

At the end, you should have a structure which looks like what follows:

    %LocalAppData%\dbg\UserExtensions
        TextDumpCompositionPackage
            GalleryManifest.xml
            1.0.0.0
                amd64
                    TextDumpComposition.dll

Once the plug-in is "installed", you can start the debugger on the included sample dump file:

    * windbgx -z TextDump.txt

You can also install the package into a particular version of the debugger package that you have permissions to
write into (e.g.: a copy of the legacy windbg package):

    * Build the sample with the included Visual Studio solution

    * Go to your debugger install

    * Copy "TextDump_GalleryManifest.xml" into the "OptionalExtensions" folder of the debugger install.

    * Open "TextDump_GalleryManifest.xml" in the "OptionalExtensions" folder and change the line which says:

      <File Architecture="amd64" Module="amd64\TextDumpComposition.dll" FilePathKind="RepositoryRelative"/>

      to:

      <File Architecture="amd64" Module="winext\TextDumpComposition.dll" FilePathKind="RepositoryRelative"/>

    * Copy the built "TextDumpComposition.dll" into the "winext" directory of the debugger install.

//*************************************************
// SOURCES TOUR
//*************************************************

The sample plug-in is composed of the following groups of files:

Files required to be an extension / hook up to the debugger:

    * Extension.cpp                             -- Code necessary to be a DbgEng extension and initialize the plug-in
    * Activator.cpp / Activator.h               -- The main activation code.  Sets up the container for "text dumps"
    * TextDump.h                                -- Main header for the plug-in
    * TextDump.rc                               -- Resource definitions for the plug-in
    * TextDump.def                              -- .def (exports) for the plug-in
    * TextDump_GalleryManifest.xml              -- The manifest for the plug-in (when/how we should load)

Files specific to our "text dump" format:

    * FileParser.cpp / FileParser.h             -- Parsing code for our "text dump" file format
    * TextDump.txt                              -- A sample "text dump" of notepad with file/open active

Implementation of services:

    * MachineServices.h                         -- Services related to reporting the "machine type" of x64
    * MemoryServices.cpp / MemoryServices.h     -- Services related to providing virtual memory
    * ModuleServices.cpp / ModuleServices.h     -- Services related to providing the module list (and index keys)
    * ProcessServices.cpp / ProcessServices.h   -- Services related to providing the target process
    * StackServices.cpp / StackServices.h       -- Services related to providing the call stack
    * ThreadServices.cpp / ThreadServices.h     -- Services related to providing the target thread
    * InternalGuids.h                           -- GUID definitions for the plug-in

//*************************************************
// STARTING OUT: MANIFEST TO ACTIVATION
//*************************************************

Once the plug-in has been installed, you can open ".txt" files from WinDbg Preview and they will cause the
"text dump" plug-in to load.  Assuming the file is the proper format and parses, you should be able to debug
a "text dump" much like you'd debug a standard Windows minidump (with some minor differences).

If you take a look at the manifest (TextDump_GalleryManifest.xml) for the plug-in, you'll note that there is
a section called <LoadTriggers>:

   <LoadTriggers>
        <TriggerSet>
            <IdentifyTargetTrigger FileExtension="txt" />
        </TriggerSet>
    </LoadTriggers>

This section tells the debugger *WHEN* to load the extension.  The <LoadTriggers> element contains one or more
<TriggerSet> elements.  The load trigger is satisfied when the conditions specified by one or more <TriggerSet>
elements is met (in other words, the group of <TriggerSet> elements is a logical "or").  Each <TriggerSet>
contains one or more triggers.  The set is satisfied when all conditions within the set are met (in other words,
the group of triggers within a trigger set is a logical "and").  The following triggers are currently available:

    <IdentifyTargetTrigger FileExtension="txt" />

        Indicates that the extension should be loaded when we attempt to identify the file format of a file
        with the given extension.  The file extension can be a specific extension (e.g.: "txt") or it can be
        a wildcard (e.g.: "*").  The extension *MUST* call RegisterFileActivatorForExtension with an
        identically specified extension during its DebugExtensionInitialize call.

    <IdentifyTargetProtocolTrigger ProtocolName="test" />

        Indicates that the extension should be loaded when we attempt to identify a handler for a particular
        type of protocol connection.  The protocol must be specifically named (e.g.: "test") and the extension
        *MUST* call RegisterProtocolActivatorForProtocolString with an identically specified protocol name
        during its DebugExtensionInitialize call.

    <AlwaysTrigger />

        Indicates that the extension should always be loaded.  This cannot be combined with other trigger types.

    <OnDemandTrigger />

        Indicates that the extension should be loaded on demand from other means that are not specified as
        triggers (e.g.: particular extensibility points, visualizers, etc...).  This cannot be combined 
        with other trigger types.

    <ModuleTrigger Name="mscorwks.dll" />
    <ModuleTrigger Regex="MRT[\d]{3,}_app.dll" />

        Indicates that the extension should load when a given module is found in the module list.  A ModuleTrigger
        can specify either a module by name or can specify a regular expression to match the module name against.

    <OSTrigger Name="Windows" />

        Indicates that the extension should load when the debugger detects that the operating system it is
        targeting is one of a specific set of known operating systems.  The available names are:

        "Windows"  - The Windows operating system
        "Mac OS X" - The Mac operating system (OS X)
        "Linux"    - The Linux operating system
        "UNIX"     - A generic UNIX

    <TargetTrigger Type="Kernel" />

        Indicates that the extension should load when the debugger detects that it is targeting a particular type
        of target (e.g.: kernel mode or user mode).  The following types are defined:

        "Kernel"   - Targeting a known kernel mode target (live, dump, or otherwise)
        "User"     - Targeting a known user mode target (live, dump, or otherwise)
        "Dump"     - Targeting a post-mortem dump target

    <ExceptionTrigger ExceptionCode="0x04242420" />

        Indicates that the extension should load when the debugger observes a particular exception (by code) thrown
        by the target.  The exception code is specified as a hexidecimal value.

For our "text dump" plug-in, we utilize an <IdentifyTargetTrigger> in order to tell the debugger to load us
"when you need to identify a particular post-mortem file format opened by the debugger."  In the case of this
plug-in, that trigger is bound to the extension "txt" so that our extension is only loaded when a ".txt" file is
opened.

Because the manifest indicated that the plug-in should be loaded and initialized upon trying to open a "txt" file,
the debugger will load the extension and call the standard DebugExtensionInitialize method (see Extension.cpp).
For a target composition plug-in, that method has a responsibility to get the interfaces for target composition
and hook up certain callbacks.

For the "text dump" format, this means that it must create an IDebugClient, query it for IDebugTargetCompositionBridge,
and subsequently register an activator.  The call of interest here is:

    IfFailedReturn(spCompositionBridge->RegisterFileActivatorForExtension(L"txt", spActivator.Get()));

The file extension that is passed to RegisterFileActivatorForExtension *MUST* match what is in the plug-in's 
manifest.

Once the plug-in has been loaded and initialized, the activator is called to ask whether the *PARTICULAR* ".txt" file
which was opened is one that the plug-in handles or not.  This is accomplished via the IsRecognizedFile method
on the activator (see Activator.cpp):

    HRESULT TextDumpActivator::IsRecognizedFile(_In_ IDebugServiceManager *pServiceManager,
                                                _In_ ISvcDebugSourceFile *pFile,
                                                _Out_ bool *pIsRecognized)

If the file represented by "pFile" is recognized, the plug-in sets *pIsRecognized to true and returns success.  If
it is not recognized, the plug-in sets *pIsRecognized to false and returns success.  Only *ONE* plug-in may claim
that a file format is recognized in order for the debugger to proceed further.

Assuming that the "text dump" plug-in recognizes the file, it is then asked to fill a service container with the 
services required to debug that file format.  Such is via the InitializeServices method:

    HRESULT TextDumpActivator::InitializeServices(_In_ IDebugServiceManager *pServiceManager)

The sample's InitializeServices method creates and adds a number of services:

    * A machine service which reports the target architecture as x64.  This is a *REQUIRED* service for any
      target -- whether representing something like a "kernel mode" target or a "user mode target".  As we
      represent a user mode target, this machine service need only report architecture.  To represent a kernel mode
      target, the service would need to implement ISvcMachineDebug.

      See MachineServices.h for the implementation of this service.

    * A process enumeration service which returns the one process we target.  This is a *REQUIRED* service for
      any target -- whether user or kernel mode.

      See ProcessServices.* for the implementation of this service. 

    * A thread enumeration service which returns the one thread we target.  This is a *REQUIRED* service for
      any target -- whether user or kernel mode.

      See ThreadServices.* for the implementation of this service.

    * A module enumeration service which returns the list of loaded modules in the address space

      See ModuleServices.* for the implementation of this service.
    
    * A virtual memory service which returns the memory regions in the text dump.  Later, it will also stack
      another memory service atop this which will provide image backed pages.

      See MemoryServices.* for the implementation of this service.

    * A stack provider service which can return the call stack.  For a dump target which is guaranteed to have
      stack memory, register context, and ways to get images/symbols, this may not be necessary.  As our plug-in
      functions for "text dumps" which only have a callstack and nothing else, we require this.

      See StackServices.* for the implementation of this service.

//*************************************************
// EXPLORATION
//*************************************************

The "text dump" plug-in is designed to function with anywhere from one single section in the dump (e.g.: only
the stack section or only the memory section) to all of the sections present in the dump.  To understand how
things are represented in the debugger from various aspects of the target composition model, it is worth taking
the "TextDump.txt" file, stripping out sections, and exploring how the debugger projection of the post-mortem
target via the sample plug-in functions.

