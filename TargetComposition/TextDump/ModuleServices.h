//**************************************************************************
//
// ModuleServices.h
//
// Target composition services to provide module information to the debugger
// from our "text dump" file format.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __MODULESERVICES_H__
#define __MODULESERVICES_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

//*************************************************
// Module Provider:
//

class ModuleEnumerationService;

// Module:
//
// Represents a module loaded in the single process that we target in our "text dump" format.  
//
// NOTE: The following interfaces are also relevant to modules:
//
//     ISvcAddressRangeEnumeration:             // Mandatory (for modules which are non-contiguous; optional otherwise)
//     ISvcMappingInformation:                  // Mandatory (for systems which may have flat/loader mapped images)
//     ISvcModuleWithTimestampAndChecksum:      // Optional (for PE images; may be deprecated in the future)
//
class Module :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcModule                             // Mandatory (for any module)
        >
{
public:

    //*************************************************
    // ISvcModule:
    //

    // GetContainingProcessKey():
    //
    // Gets the unique key of the process to which this thread belongs.  This is the same key returned
    // from the containing ISvcProcess's GetKey method.
    //
    IFACEMETHOD(GetContainingProcessKey)(_Out_ PULONG64 pContainingProcessKey)
    {
        //
        // Since we only have a single process that we have used "1" as the process key for, we can return
        // that here.  Otherwise, we would return the key for whatever process actually enumerated this
        // particular module.
        //
        *pContainingProcessKey = 1;
        return S_OK;
    }

    // GetKey():
    //
    // Gets the unique "per-process" module key.  The interpretation of this key is dependent upon
    // the service which provides this interface.  This may be the base address of the module.
    //
    IFACEMETHOD(GetKey)(_Out_ PULONG64 pModuleKey)
    {
        //
        // We will use the base load address of a module as our key.  It does not matter what we use so long
        // as the value is unique in any given process.
        //
        return GetBaseAddress(pModuleKey);
    }

    // GetBaseAddress():
    //
    // Gets the base address of the module.
    //
    IFACEMETHOD(GetBaseAddress)(_Out_ PULONG64 pModuleBaseAddress)
    {
        *pModuleBaseAddress = m_pModuleInfo->StartAddress;
        return S_OK;
    }

    // GetSize():
    //
    // Gets the size of the module.
    //
    IFACEMETHOD(GetSize)(_Out_ PULONG64 pModuleSize)
    {
        *pModuleSize = m_pModuleInfo->EndAddress - m_pModuleInfo->StartAddress;
        return S_OK;
    }

    // GetName():
    //
    // Gets the name of the module.
    //
    IFACEMETHOD(GetName)(_Out_ BSTR *pModuleName);

    // GetPath():
    //
    // Gets the load path of the module.
    //
    IFACEMETHOD(GetPath)(_Out_ BSTR *pModulePath);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializer.
    //
    HRESULT RuntimeClassInitialize(_In_ ModuleEnumerationService *pModuleService,
                                   _In_ std::shared_ptr<TextDumpParser> &parsedFile,
                                   _In_ ModuleInformation const *pModuleInfo)
    {
        m_spModuleService = pModuleService;
        m_spParsedFile = parsedFile;
        m_pModuleInfo = pModuleInfo;
        return S_OK;
    }

private:
    
    Microsoft::WRL::ComPtr<ModuleEnumerationService> m_spModuleService;
    std::shared_ptr<TextDumpParser> m_spParsedFile;
    ModuleInformation const *m_pModuleInfo;

};

// ModuleEnumerator:
//
// A module enumerator that enumerates the modules loaded in our process for our "text dump" format.
//
class ModuleEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcModuleEnumerator                    // Mandatory (for all module enumerators)
        >
{
public:

    //*************************************************
    // ISvcModuleEnumerator:
    //

    // Reset():
    //
    // Resets the enumerator.
    //
    IFACEMETHOD(Reset)()
    {
        m_pos = 0;
        return S_OK;
    }

    // GetNext():
    //
    // Gets the next module from the enumerator.
    //
    IFACEMETHOD(GetNext)(_COM_Outptr_ ISvcModule **ppTargetModule);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializer.
    //
    HRESULT RuntimeClassInitialize(_In_ ModuleEnumerationService *pModuleService,
                                   _In_ std::shared_ptr<TextDumpParser> &parsedFile)
    {
        m_spModuleService = pModuleService;
        m_spParsedFile = parsedFile;
        return Reset();
    }

private:

    Microsoft::WRL::ComPtr<ModuleEnumerationService> m_spModuleService;
    std::shared_ptr<TextDumpParser> m_spParsedFile;
    size_t m_pos;

};

// ModuleEnumerationService:
//
// A module enumeration service that provides the list of modules loaded into our process for our
// "text dump" format.  If we had understanding of which module was the "main executable" and other such
// attributes, we could also optionally implement ISvcPrimaryModules.  We do not.
//
class ModuleEnumerationService :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugServiceLayer,                     // Mandatory (for all services)
        ISvcModuleEnumeration                   // Mandatory (for all module enumeration services)
        >
{
public:

    //*************************************************
    // IDebugServiceLayer:
    //

    // RegisterServices():
    //
    // Registers all services contained in this component with the services manager.
    //
    IFACEMETHOD(RegisterServices)(_In_ IDebugServiceManager *pServiceManager)
    {
        HRESULT hr = S_OK;
        IfFailedReturn(pServiceManager->RegisterService(DEBUG_SERVICE_MODULE_ENUMERATOR, this));

        //
        // Keep a *WEAK* back pointer to the service manager that owns us.  This will allow us to later
        // fire event notifications back into the service container.
        //
        m_pServiceManager = pServiceManager;

        return hr;
    }

    // GetServiceDependencies():
    //
    // Returns the set of services which this service layer / component depends on.  Having sizeHardDependencies or
    // sizeSoftDependencies set to 0 will pass back the number of dependencies and do nothing else.
    //
    IFACEMETHOD(GetServiceDependencies)(_In_ ServiceNotificationKind /*notificationKind*/,
                                        _In_ IDebugServiceManager * /*pServiceManager*/,
                                        _In_ REFGUID /*serviceGuid*/,
                                        _In_ ULONG64 sizeHardDependencies,
                                        _Out_writes_(sizeHardDependencies) GUID * /*pHardDependencies*/,
                                        _Out_ ULONG64 *pNumHardDependencies,
                                        _In_ ULONG64 sizeSoftDependencies,
                                        _Out_writes_(sizeSoftDependencies) GUID * /*pSoftDependencies*/,
                                        _Out_ ULONG64 *pNumSoftDependencies)
    {
        HRESULT hr = S_OK;
        if (sizeHardDependencies == 0 && sizeSoftDependencies == 0)
        {
            *pNumHardDependencies = 0;
            *pNumSoftDependencies = 0;
            return S_OK;
        }

        *pNumHardDependencies = 0;
        *pNumSoftDependencies = 0;
        return hr;
    }

    // InitializeServices():
    //
    // Performs initialization of the services in a service layer / component.  Services which aggregate, 
    // encapsulate, or stack on top of other services must pass down the initialization notification in an 
    // appropriate manner (with notificationKind set to LayeredNotification).
    //
    IFACEMETHOD(InitializeServices)(_In_ ServiceNotificationKind /*notificationKind*/,
                                    _In_ IDebugServiceManager * /*pServiceManager*/,
                                    _In_ REFIID /*serviceGuid*/)
    {
        return S_OK;
    }

    // NotifyServiceChange():
    //
    // Called when there is a change in the component registered as a service in the
    // target composition stack.
    //
    IFACEMETHOD(NotifyServiceChange)(_In_ ServiceNotificationKind /*notificationKind*/,
                                     _In_ IDebugServiceManager * /*pServiceManager*/,
                                     _In_ REFIID /*serviceGuid*/,
                                     _In_opt_ IDebugServiceLayer * /*pPriorService*/,
                                     _In_opt_ IDebugServiceLayer * /*pNewService*/)
    {
        return S_OK;
    }

    // NotifyEvent():
    //
    // Called to notify this component that an event of interest occurred.
    //
    IFACEMETHOD(NotifyEvent)(_In_ IDebugServiceManager * /*pServiceManager*/,
                             _In_ REFIID /*eventGuid*/,
                             _In_opt_ IUnknown * /*pEventArgument*/)
    {
        return S_OK;
    }

    //*************************************************
    // ISvcModuleEnumerator
    //

    // FindModule():
    //
    // Finds a module by a unique key.
    //
    IFACEMETHOD(FindModule)(_In_opt_ ISvcProcess *pProcess,
                            _In_ ULONG64 moduleKey,
                            _COM_Outptr_ ISvcModule **ppTargetModule);

    // FindModuleAtAddress():
    //
    // Finds a module given an address within its VA space.
    //
    IFACEMETHOD(FindModuleAtAddress)(_In_opt_ ISvcProcess *pProcess,
                                     _In_ ULONG64 moduleAddress,
                                     _COM_Outptr_ ISvcModule **ppTargetModule);

    // EnumerateModules():
    //
    // Returns an enumerator which enumerates all modules in the core.
    //
    IFACEMETHOD(EnumerateModules)(_In_opt_ ISvcProcess *pProcess, _COM_Outptr_ ISvcModuleEnumerator **ppEnum);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the virtual memory service.
    //
    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> const& parsedFile)
    {
        m_pServiceManager = nullptr;
        m_spParsedFile = parsedFile;
        m_firstEnumerationComplete = false;
        return S_OK;
    }

    // CompleteModuleEnumeration():
    //
    // Called whenever module enumeration hits the end of enumeration.
    //
    void CompleteModuleEnumeration();

private:

    std::shared_ptr<TextDumpParser> m_spParsedFile;
    bool m_firstEnumerationComplete;

    //
    // Keep a *WEAK* back pointer to the service manager that owns us!
    //
    IDebugServiceManager *m_pServiceManager;

};

//*************************************************
// Module Index Provider:
//

// ModuleIndexService:
//
// A module index service that provides the indexing keys for our modules.  These *COULD* be read out of the
// VA space of the modules is such pages were captured; however, our "text dump" format does not record such pages.
// The indexing keys (time/date stamp and image size) for our modules are captured in our module information.  In
// order to get the module images found (and subsequently symbols downloaded), we need to either provide an index 
// provider...  or an entire image provider...  
//
class ModuleIndexService :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugServiceLayer,                     // Mandatory (for all services)
        ISvcModuleIndexProvider                 // Mandatory (for all module index services)
        >
{
public:

    //*************************************************
    // IDebugServiceLayer:
    //

    // RegisterServices():
    //
    // Registers all services contained in this component with the services manager.
    //
    IFACEMETHOD(RegisterServices)(_In_ IDebugServiceManager *pServiceManager)
    {
        HRESULT hr = S_OK;
        IfFailedReturn(pServiceManager->RegisterService(DEBUG_SERVICE_MODULE_INDEX_PROVIDER, this));
        return hr;
    }

    // GetServiceDependencies():
    //
    // Returns the set of services which this service layer / component depends on.  Having sizeHardDependencies or
    // sizeSoftDependencies set to 0 will pass back the number of dependencies and do nothing else.
    //
    IFACEMETHOD(GetServiceDependencies)(_In_ ServiceNotificationKind /*notificationKind*/,
                                        _In_ IDebugServiceManager * /*pServiceManager*/,
                                        _In_ REFGUID /*serviceGuid*/,
                                        _In_ ULONG64 sizeHardDependencies,
                                        _Out_writes_(sizeHardDependencies) GUID * /*pHardDependencies*/,
                                        _Out_ ULONG64 *pNumHardDependencies,
                                        _In_ ULONG64 sizeSoftDependencies,
                                        _Out_writes_(sizeSoftDependencies) GUID * /*pSoftDependencies*/,
                                        _Out_ ULONG64 *pNumSoftDependencies)
    {
        HRESULT hr = S_OK;
        if (sizeHardDependencies == 0 && sizeSoftDependencies == 0)
        {
            *pNumHardDependencies = 0;
            *pNumSoftDependencies = 0;
            return S_OK;
        }

        *pNumHardDependencies = 0;
        *pNumSoftDependencies = 0;
        return hr;
    }

    // InitializeServices():
    //
    // Performs initialization of the services in a service layer / component.  Services which aggregate, 
    // encapsulate, or stack on top of other services must pass down the initialization notification in an 
    // appropriate manner (with notificationKind set to LayeredNotification).
    //
    IFACEMETHOD(InitializeServices)(_In_ ServiceNotificationKind /*notificationKind*/,
                                    _In_ IDebugServiceManager * /*pServiceManager*/,
                                    _In_ REFIID /*serviceGuid*/)
    {
        return S_OK;
    }

    // NotifyServiceChange():
    //
    // Called when there is a change in the component registered as a service in the
    // target composition stack.
    //
    IFACEMETHOD(NotifyServiceChange)(_In_ ServiceNotificationKind /*notificationKind*/,
                                     _In_ IDebugServiceManager * /*pServiceManager*/,
                                     _In_ REFIID /*serviceGuid*/,
                                     _In_opt_ IDebugServiceLayer * /*pPriorService*/,
                                     _In_opt_ IDebugServiceLayer * /*pNewService*/)
    {
        return S_OK;
    }

    // NotifyEvent():
    //
    // Called to notify this component that an event of interest occurred.
    //
    IFACEMETHOD(NotifyEvent)(_In_ IDebugServiceManager * /*pServiceManager*/,
                             _In_ REFIID /*eventGuid*/,
                             _In_opt_ IUnknown * /*pEventArgument*/)
    {
        return S_OK;
    }

    //*************************************************
    // ISvcModuleIndexProvider
    //

    // GetModuleIndexKey():
    //
    // Retrieves a key for the given module which serves as an index for it on the symbol server.
    //
    IFACEMETHOD(GetModuleIndexKey)(_In_ ISvcModule *pModule,
                                   _Out_ BSTR *pModuleIndex,
                                   _Out_ GUID *pModuleIndexKind);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the virtual memory service.
    //
    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> const& parsedFile)
    {
        m_spParsedFile = parsedFile;
        return S_OK;
    }

private:

    std::shared_ptr<TextDumpParser> m_spParsedFile;
};

} // TextDump
} // Services
} // TargetComposition
} // Debugger

#endif // __PROCESSSERVICES_H__
