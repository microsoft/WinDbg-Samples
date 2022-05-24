//**************************************************************************
//
// Activator.cpp
//
// File activator for our "text dumps" as a post-mortem dump target.  The file activator 
// is what the debugger will call to determine whether you handle a particular file format 
// and, if you do, to fill a target composition container with the services required to actually
// debug a particular target.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "TextDump.h"

extern IDebugTargetComposition *g_pCompositionManager;

using namespace Microsoft::WRL;
using namespace Debugger::TargetComposition::Services::TextDump;

namespace Debugger
{
namespace TargetComposition
{
namespace FileActivators
{

HRESULT TextDumpActivator::DiagnosticLog(_In_ IDebugServiceManager *pServiceManager,
                                         _In_ DiagnosticLogLevel level,
                                         _In_opt_ PCWSTR pwszComponent,
                                         _In_opt_ PCWSTR pwszCategory,
                                         _In_ PCWSTR pwszLogMessage)
{
    HRESULT hr = S_FALSE;
    ComPtr<ISvcDiagnosticLogging> spLog;
    if (SUCCEEDED(pServiceManager->QueryService(DEBUG_SERVICE_DIAGNOSTIC_LOGGING, IID_PPV_ARGS(&spLog))))
    {
        hr = spLog->Log(level, pwszLogMessage, pwszComponent, pwszCategory);
    }
    
    return hr;
}

HRESULT TextDumpActivator::IsRecognizedFile(_In_ IDebugServiceManager *pServiceManager,
                                            _In_ ISvcDebugSourceFile *pFile,
                                            _Out_ bool *pIsRecognized)
{
    //
    // NOTE: This is the first thing called by the debugger when a potentially matching file is opened.
    //
    // The activator is registered for a file extension of ".txt" and so this method will be called for *EVERY*
    // .txt file that the debugger tries to open as a post-mortem dump file.  It is the responsibility of this 
    // method to determine whether this is the CORRECT FORMAT (our "text dump" format). 
    //
    // Only one activator may claim a particular file (by setting *pIsRecognized to true) in order for it to 
    // open successfully.
    //
    HRESULT hr = S_OK;
    *pIsRecognized = false;

    (void)DiagnosticLog(pServiceManager,
                        DiagnosticLevelInfo, 
                        L"TextDump",
                        L"FileActivator",
                        L"Checking for text dump signature");

    TextDumpParser p(pFile);
    *pIsRecognized = SUCCEEDED(p.Initialize());

    if (*pIsRecognized)
    {
        (void)DiagnosticLog(pServiceManager,
                            DiagnosticLevelInfo,
                            L"TextDump",
                            L"FileActivator",
                            L"Recognized text dump signature for file open");
    }

    return hr;
}

HRESULT TextDumpActivator::InitializeServices(_In_ IDebugServiceManager *pServiceManager)
{
    //
    // Assuming we returned *pIsRecognized = true from IsRecognizedFile above, this method will be called
    // afterward.
    //
    // This method should insert any services into pServiceManager which are required to expose the functionality
    // of the format to the debugger.  For us, such services include:
    //
    //     - A process enumeration service which returns one process (representing what we are debugging)
    //
    //     - A thread enumeration service which returns one thread (representing the single thread in the dump)
    //
    //     - A module enumeration service which returns one module per module info in the text dump
    //
    //     - A virtual memory service which returns the memory regions in the text dump
    //
    //     - An image provider which can look up image files for the modules in the text dump
    // 
    //     - A stack provider which can return the stack unwind regardless of whether the memory sections
    //       contain the actual bytes of the stack or not.  We expose a stack provider (the higher level of abstraction
    //       of a stack and frames) rather than a stack unwinder (the lower level abstraction of being able to unwind
    //       through a function given an address and register context) because we may not have the information required
    //       for that lower level construct.  A plug-in can provide either (or both) as needed.
    //
    HRESULT hr = S_OK;

    //
    // Go get the underlying file and create a parser on top of it.  The parser (and its resulting data) will be
    // shared between all of the services we create.
    //
    ComPtr<ISvcDebugSourceFile> spFile;
    IfFailedReturn(pServiceManager->QueryService(DEBUG_PRIVATE_SERVICE_DEBUGSOURCE, IID_PPV_ARGS(&spFile)));

    std::shared_ptr<TextDumpParser> spParser = std::make_shared<TextDumpParser>(spFile.Get());
    IfFailedReturn(spParser->Initialize());
    IfFailedReturn(spParser->Parse());

    //
    // *ALL* targets must have a machine service that indicates what kind of machine is being targeted.  Some
    // machine services may provide more information through optional interfaces (e.g.: kernel targets providing
    // the number of cores and access to cores)
    //
    ComPtr<MachineService> spMachine;
    IfFailedReturn(MakeAndInitialize<MachineService>(&spMachine, spParser));
    IfFailedReturn(spMachine->RegisterServices(pServiceManager));

    //
    // As we are representing a user mode process target, there must be process enumeration services available.
    // Thread enumeration services are, likewise, required for this kind of target.
    //
    ComPtr<ProcessEnumerationService> spProcessEnumeration;
    IfFailedReturn(MakeAndInitialize<ProcessEnumerationService>(&spProcessEnumeration, spParser));
    IfFailedReturn(spProcessEnumeration->RegisterServices(pServiceManager));

    ComPtr<ThreadEnumerationService> spThreadEnumeration;
    IfFailedReturn(MakeAndInitialize<ThreadEnumerationService>(&spThreadEnumeration, spParser));
    IfFailedReturn(spThreadEnumeration->RegisterServices(pServiceManager));

    //
    // If the file in question has memory region(s) described, add a virtual memory service to the container.
    // We also do this if there are module informations as we can map image memory into the VA space even if we do not
    // have specific memory bytes and doing that requires an "underlying virtual memory service" even if such
    // service reports no readable memory.
    //
    ComPtr<VirtualMemoryService> spVirtualMemory;
    if (spParser->HasMemoryRegions() || spParser->HasModuleInformations())
    {
        IfFailedReturn(MakeAndInitialize<VirtualMemoryService>(&spVirtualMemory, spParser));
        IfFailedReturn(spVirtualMemory->RegisterServices(pServiceManager));
    }

    //
    // If the file in question has module information(s) described, add a module enumeration service to the container.
    // In addition, we will add a number of other services that will allow us to pull symbols and read image bytes.
    // Those services are:
    //
    //     - A module index provider: since the debugger cannot read the symbol server keys from the memory of
    //       the process (we do not capture that), we can provide a module index provider which will give the necessary
    //       keys.  This will allow the images to be downloaded...  and subsequently symbols to be found.
    //
    //     - A PE image provider: a *SYSTEM* provided component which knows how to find PE images in the search path
    //       (including the symbol server) from an indexing key (provided by our module index provider)
    //
    //     - A PE image parser: a *SYSTEM* provided component which can parse and understand the internals of PE images
    //
    //     - A stacked virtual memory service: a *SYSTEM* provided component which will sit *STACKED* on top of our
    //       virtual memory service and provide image bytes into the VA space from PE images in our search path or
    //       downloaded from the symbol server.  *NOTE*: we defer this particular bit until after the first module
    //       enumeration.  See "VirtualMemoryService::NotifyEvent" for details.
    //  
    // System provided components can be created from the composition manager.  Note that some of these components
    // have initializer interfaces which *MUST* be called after creating the component.  Some of them do not.
    // Documentation and the DbgServices.h header will indicate the appropriate initializer interface, if any.
    //
    if (spParser->HasModuleInformations())
    {
        ComPtr<ModuleEnumerationService> spModuleEnumeration;
        IfFailedReturn(MakeAndInitialize<ModuleEnumerationService>(&spModuleEnumeration, spParser));
        IfFailedReturn(spModuleEnumeration->RegisterServices(pServiceManager));

        ComPtr<ModuleIndexService> spModuleIndex;
        IfFailedReturn(MakeAndInitialize<ModuleIndexService>(&spModuleIndex, spParser));
        IfFailedReturn(spModuleIndex->RegisterServices(pServiceManager));

        //
        // If we initialized successfully, this should *ALWAYS* be set.  Safety regardless...
        //
        if (g_pCompositionManager)
        {
            ComPtr<IDebugServiceLayer> spPEImageProvider;
            IfFailedReturn(g_pCompositionManager->CreateComponent(DEBUG_COMPONENTSVC_PEIMAGE_IMAGEPROVIDER,
                                                                  &spPEImageProvider));
            IfFailedReturn(spPEImageProvider->RegisterServices(pServiceManager));

            ComPtr<IDebugServiceLayer> spPEImageParser;
            IfFailedReturn(g_pCompositionManager->CreateComponent(DEBUG_COMPONENTSVC_PEIMAGE_IMAGEPARSEPROVIDER,
                                                                  &spPEImageParser));
            IfFailedReturn(spPEImageParser->RegisterServices(pServiceManager));
        }
    }

    //
    // If the file in question has stack frame(s) described, add a stack provider service to the container. 
    // Such may or may not be necessary depending on what is in the format.  For our "text dump" that may
    // not include memory or register context, it is.  See comments in the provider for more details.
    //
    if (spParser->HasStackFrames())
    {
        ComPtr<StackProviderService> spStackProvider;
        IfFailedReturn(MakeAndInitialize<StackProviderService>(&spStackProvider, spParser));
        IfFailedReturn(spStackProvider->RegisterServices(pServiceManager));
    }

    (void)DiagnosticLog(pServiceManager,
                        DiagnosticLevelInfo,
                        L"TextDump",
                        L"FileActivator",
                        L"Recognized and successfully opened a text dump file");

    return hr;
}

} // FileActivators
} // TargetComposition
} // Debugger

