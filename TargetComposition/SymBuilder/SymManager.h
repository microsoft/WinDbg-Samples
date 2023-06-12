//**************************************************************************
//
// SymManager.h
//
// A management object which keeps track of the symbol sets that have been
// created and which modules they are assigned to.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __SYMMANAGER_H__
#define __SYMMANAGER_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

class SymbolBuilderManager;

// SymbolBuilderProcess:
//
// Tracks what modules we have defined symbols for within a given process context.
//
class SymbolBuilderProcess :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IUnknown
        >
{
public:

    // TryGetSymbolsForModule():
    //
    // Checks whether we have symbols for the given module by its unique "key".  If so, true is returned
    // and a pointer to the symbol set is passed back; otherwise, false is returned.
    //
    // This method does not create a new symbol set.
    //
    bool TryGetSymbolsForModule(_In_ ULONG64 moduleKey,
                                _COM_Outptr_ SymbolSet **ppSymbols)
    {
        *ppSymbols = nullptr;
        auto it = m_symbols.find(moduleKey);
        if (it == m_symbols.end())
        {
            return false;
        }

        Microsoft::WRL::ComPtr<SymbolSet> spSymbols = it->second;
        *ppSymbols = spSymbols.Detach();
        return true;
    }

    // CreateSymbolsForModule():
    //
    // Creates a new symbol set for a given module by its unique "key".  This method will fail if symbols already
    // exist for the module.  The caller has responsibility to check first.
    //
    HRESULT CreateSymbolsForModule(_In_ ISvcModule *pModule,
                                   _In_ ULONG64 moduleKey,
                                   _COM_Outptr_ SymbolSet **ppSymbols);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes a new SymbolBuilderProcess.
    //
    HRESULT RuntimeClassInitialize(_In_ bool isKernel,
                                   _In_ ULONG64 processKey, 
                                   _In_ SymbolBuilderManager *pOwningManager)
    {
        m_isKernel = isKernel;
        m_processKey = processKey;
        m_pOwningManager = pOwningManager;
        return S_OK;
    }

    // GetSymbolBuilderManager():
    //
    // Gets the symbol builder manager that this process was created from.
    //
    SymbolBuilderManager *GetSymbolBuilderManager() const
    {
        return m_pOwningManager;
    }

    // GetServiceManager():
    //
    // Gets the service manager that this process was created from.
    //
    IDebugServiceManager *GetServiceManager() const;

    // GetArchInfo():
    //
    // Gets information about the architecture we are targeting.
    //
    ISvcMachineArchitecture *GetArchInfo() const;

    // GetVirtualMemory():
    //
    // Gets the virtual memory access interface for what we are targeting.
    //
    ISvcMemoryAccess *GetVirtualMemory() const;

    // GetProcessKey():
    //
    // Gets the process key for this process.  This will be zero if this represents the kernel and its
    // set of modules.
    //
    ULONG64 GetProcessKey() const { return m_processKey; }

    // IsKernel():
    //
    // Indicates whether or not this represents the kernel.
    //
    bool IsKernel() const { return m_isKernel; }

private:

    // Indicates if this represents the kernel and its set of modules
    bool m_isKernel;

    // The "key" used to identify the process we represent.  This will be zero if we represent the kernel.
    ULONG64 m_processKey;

    // A map tracking the "symbol sets" we have created for modules in this process.  This a map
    // of module "keys" to symbol sets within the context of this process.
    std::unordered_map<ULONG64, Microsoft::WRL::ComPtr<SymbolSet>> m_symbols;

    // Weak pointer back to our owning manager.
    SymbolBuilderManager *m_pOwningManager;
};

struct DECLSPEC_UUID("AF4E77D9-1100-4c40-BAB0-67450027FCA5") ISvcSymbolBuilderManager;

// RegisterInformation:
//
// Information about a register for the machine architecture in use.
//
struct RegisterInformation
{
    std::wstring Name;
    ULONG Id;
    ULONG Size;
    ULONG ParentId;                     // (ULONG)-1 or the ID of the parent register if a sub-register
    ULONG SubLsb;                       // If a subregister, LSB of the mapping
    ULONG SubMsb;                       // If a subregister, MSB of the mapping
    std::vector<ULONG> SubRegisters;    // The list of sub-register IDs for this register
};

// ISvcSymbolBuilderManager:
//
// This is an **INTERNAL ONLY** interface that we place on our management service.  When a request comes
// in to load symbols, we go into the service container, locate the management service, query for this interface,
// and use it to find whether we have symbols (or add them).
//
// Because this is an **INTERNAL ONLY** interface, we can use **INTERNAL** types like SymbolBuilderProcess within
// its definition.
//
#undef INTERFACE
#define INTERFACE ISvcSymbolBuilderManager
DECLARE_INTERFACE_(ISvcSymbolBuilderManager, IUnknown)
{
    // GetKernelAddressContext():
    //
    // If the target in question is a kernel mode target, this returns the address context for kernel mode
    // (non process specific) reads.  If not, this will fail.
    //
    STDMETHOD(GetKernelAddressContext)(_COM_Outptr_ ISvcAddressContext **ppKernelAddressContext) PURE;

    // ProcessKeyToProcess():
    //
    // Converts a process key to the process object for it.
    //
    STDMETHOD(ProcessKeyToProcess)(_In_ ULONG64 processKey,
                                   _COM_Outptr_ ISvcProcess **ppProcess) PURE;
    
    // PidToProcess():
    //
    // Converts a process id to the process object for it.
    //
    STDMETHOD(PidToProcess)(_In_ ULONG64 pid,
                            _COM_Outptr_ ISvcProcess **ppProcess) PURE;

    // ModuleBaseToModule():
    //
    // Converts a module base to the module object for it.
    //
    STDMETHOD(ModuleBaseToModule)(_In_opt_ ISvcProcess *pProcess,
                                  _In_ ULONG64 moduleBase,
                                  _COM_Outptr_ ISvcModule **ppModule) PURE;

    // TrackProcessForModule():
    //
    // For a given module, find its associated process, and create tracking structures associated with
    // that process.
    //
    STDMETHOD(TrackProcessForModule)(_In_ bool isKernel,
                                     _In_ ISvcModule *pModule, 
                                     _COM_Outptr_ SymbolBuilderProcess **ppProcess) PURE;

    // TrackProcessForKey():
    //
    // Create tracking structures associated with a process by its unique key.
    //
    STDMETHOD(TrackProcessForKey)(_In_ bool isKernel,
                                  _In_ ULONG64 processKey,
                                  _COM_Outptr_ SymbolBuilderProcess **ppProcess) PURE;

    // TrackProcess():
    //
    // Create tracking structures associated with a process by its interface.
    //
    STDMETHOD(TrackProcess)(_In_ bool isKernel,
                            _In_opt_ ISvcProcess *pProcess,
                            _COM_Outptr_ SymbolBuilderProcess **ppProcess) PURE;

    // FindInformationForRegisterByName():
    //
    // Find information for a given register by its name.
    //
    STDMETHOD(FindInformationForRegister)(_In_ PCWSTR pwszRegisterName,
                                          _Out_ RegisterInformation **ppRegisterInfo) PURE;

    // FindInformationForRegisterById():
    //
    // Find information for a given register by its canonical id.
    //
    STDMETHOD(FindInformationForRegisterById)(_In_ ULONG id,
                                              _Out_ RegisterInformation **ppRegisterInfo) PURE;

    // ParseLocation():
    //
    // Parses a location string to get a SvcSymbolLocation.
    //
    STDMETHOD(ParseLocation)(_In_ PCWSTR pwszLocation,
                             _Out_ SvcSymbolLocation *pLocation) PURE;

    // LocationToString():
    //
    // Converts a location to a displayable string for the associated architecture.
    //
    STDMETHOD(LocationToString)(_In_ SvcSymbolLocation const *pLocation,
                                _Out_ std::wstring *pString) PURE;

    // GetDefaultCallingConvention():
    //
    // Gets an abstraction for the platform's default calling convention if we are aware of it; otherwise, 
    // this will fail.
    //
    STDMETHOD(GetDefaultCallingConvention)(_Outptr_ CallingConvention **ppDefaultConvention) PURE;
};

// SymbolBuilderManager:
//
// A management object that we place in the service container in order to track information about what
// processes and modules we have symbol sets for.
//
// We have a dependency on the process and module enumeration services in order to find what processes things 
// refer to and find modules.  In addition, we will listen to certain events to notify us of modules which come 
// and go in order to delete symbols which are no longer relevant.
//
class SymbolBuilderManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugServiceLayer,
        ISvcSymbolBuilderManager
        >
{
public:

    SymbolBuilderManager() : m_pOwningManager(nullptr)
    {
    }

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
        IfFailedReturn(pServiceManager->RegisterService(DEBUG_PRIVATE_SERVICE_SYMBOLBUILDER_MANAGER, this));
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
                                        _Out_writes_(sizeHardDependencies) GUID *pHardDependencies,
                                        _Out_ ULONG64 *pNumHardDependencies,
                                        _In_ ULONG64 sizeSoftDependencies,
                                        _Out_writes_(sizeSoftDependencies) GUID *pSoftDependencies,
                                        _Out_ ULONG64 *pNumSoftDependencies)
    {
        HRESULT hr = S_OK;
        if (sizeHardDependencies == 0 && sizeSoftDependencies == 0)
        {
            *pNumHardDependencies = 3;
            *pNumSoftDependencies = 2;
            return S_OK;
        }

        if (sizeHardDependencies < 3)
        {
            return E_INVALIDARG;
        }

        if (sizeSoftDependencies < 2)
        {
            return E_INVALIDARG;
        }

        pHardDependencies[0] = DEBUG_SERVICE_PROCESS_ENUMERATOR;
        pHardDependencies[1] = DEBUG_SERVICE_MODULE_ENUMERATOR;
        pHardDependencies[2] = DEBUG_SERVICE_ARCHINFO;

        //
        // We can absolutely function without the VM service.  We only need this for importing symbols from
        // some sources (e.g.: DbgHelp).  The import may fail without this, but the rest of the builder's symbols
        // will work properly.  Thus, it is a *soft* dependency and not a *hard* one (e.g.: it's optional)
        //
        pSoftDependencies[0] = DEBUG_SERVICE_VIRTUAL_MEMORY;

        //
        // We can absolutely function without the OS information service.  We only need this for detecting which
        // platform we are running on to identify calling conventions for some advanced functionality.
        //
        pSoftDependencies[1] = DEBUG_SERVICE_OS_INFORMATION;

        *pNumHardDependencies = 3;
        *pNumSoftDependencies = 2;
        return hr;
    }

    // InitializeServices():
    //
    // Performs initialization of the services in a service layer / component.  Services which aggregate, 
    // encapsulate, or stack on top of other services must pass down the initialization notification in an 
    // appropriate manner (with notificationKind set to LayeredNotification).
    //
    IFACEMETHOD(InitializeServices)(_In_ ServiceNotificationKind /*notificationKind*/,
                                    _In_ IDebugServiceManager *pServiceManager,
                                    _In_ REFIID /*serviceGuid*/)
    {
        HRESULT hr = S_OK;
        m_pOwningManager = pServiceManager;

        //
        // We have a hard dependency on the process & module enumerators and arch info.  This means:
        //
        //     1) If we are inserted into the service container *PRIOR* to its spin up (e.g.: as part
        //        of a file activation), it is guaranteed to be in the service container and initialized
        //        by this point (initialization follows the ordering of a DAG of dependencies).
        //
        //     2) If we are inserted into the service container dynamically, there **IS NO GUARANTEE** that
        //        the process enumeration service is there despite the dependency.  As we load in a dynamic
        //        fashion, we must be prepared to deal with this!  Hence, we do *NOT* fail initialization
        //        and all our calls check m_spProcEnum.
        //
        (void)pServiceManager->QueryService(DEBUG_SERVICE_PROCESS_ENUMERATOR, IID_PPV_ARGS(&m_spProcEnum));
        (void)pServiceManager->QueryService(DEBUG_SERVICE_MODULE_ENUMERATOR, IID_PPV_ARGS(&m_spModEnum));
        (void)pServiceManager->QueryService(DEBUG_SERVICE_ARCHINFO, IID_PPV_ARGS(&m_spArchInfo));
        (void)pServiceManager->QueryService(DEBUG_SERVICE_VIRTUAL_MEMORY, IID_PPV_ARGS(&m_spVirtualMemory));
        (void)pServiceManager->QueryService(DEBUG_SERVICE_OS_INFORMATION, IID_PPV_ARGS(&m_spOSPlatformInformation));

        //
        // We want to listen to modules that disappear so that we can "unload" our cached copy of the symbols.
        //
        IfFailedReturn(pServiceManager->RegisterEventNotification(DEBUG_SVCEVENT_MODULEDISAPPEARANCE, this));

        if (m_spArchInfo != nullptr)
        {
            IfFailedReturn(InitArchBased());
        }

        return hr;
    }

    // NotifyServiceChange():
    //
    // Called when there is a change in the component registered as a service in the
    // target composition stack.
    //
    IFACEMETHOD(NotifyServiceChange)(_In_ ServiceNotificationKind /*notificationKind*/,
                                     _In_ IDebugServiceManager * /*pServiceManager*/,
                                     _In_ REFIID serviceGuid,
                                     _In_opt_ IDebugServiceLayer * /*pPriorService*/,
                                     _In_opt_ IDebugServiceLayer *pNewService)
    {
        HRESULT hr = S_OK;
        if (serviceGuid == DEBUG_SERVICE_PROCESS_ENUMERATOR)
        {
            //
            // If the process enumerator service changed, alter our cached copy of it so that we are calling
            // the correct service!
            //
            m_spProcEnum = nullptr;
            if (pNewService != nullptr)
            {
                //
                // *EVERY* process enumeration service is *REQUIRED* to support ISvcProcessEnumeration!
                //
                IfFailedReturn(pNewService->QueryInterface(IID_PPV_ARGS(&m_spProcEnum)));
            }
        }
        else if (serviceGuid == DEBUG_SERVICE_MODULE_ENUMERATOR)
        {
            //
            // If the module enumerator service changed, alter our cached copy of it so that we are calling
            // the correct service!
            //
            m_spModEnum = nullptr;
            if (pNewService != nullptr)
            {
                //
                // *EVERY* module enumeration service is *REQUIRED* to support ISvcModuleEnumeration!
                //
                IfFailedReturn(pNewService->QueryInterface(IID_PPV_ARGS(&m_spModEnum)));
            }
        }
        else if (serviceGuid == DEBUG_SERVICE_ARCHINFO)
        {
            //
            // If the arch info service changed, alter our cached copy of it so that we are calling
            // the correct service.
            //
            m_spArchInfo = nullptr;
            if (pNewService != nullptr)
            {
                //
                // *EVERY* arch info service is *REQUIRED* to support ISvcMachineArchitecture
                //
                IfFailedReturn(pNewService->QueryInterface(IID_PPV_ARGS(&m_spArchInfo)));
                IfFailedReturn(InitArchBased());
            }
        }
        else if (serviceGuid == DEBUG_SERVICE_VIRTUAL_MEMORY)
        {
            //
            // If the VM service changed, alter our cached copy of it so that we are calling
            // the correct service.
            //
            m_spVirtualMemory = nullptr;
            if (pNewService != nullptr)
            {
                //
                // *EVERY* VM service is *REQUIRED* to support ISvcMemoryAccess
                //
                IfFailedReturn(pNewService->QueryInterface(IID_PPV_ARGS(&m_spVirtualMemory)));
            }
        }
        else if (serviceGuid == DEBUG_SERVICE_OS_INFORMATION)
        {
            //
            // If the OS platform service changed, alter our cached copy of it so that we are calling
            // the correct service.
            //
            m_spOSPlatformInformation = nullptr;
            if (pNewService != nullptr)
            {
                if (FAILED(pNewService->QueryInterface(IID_PPV_ARGS(&m_spOSPlatformInformation))))
                {
                    m_spOSPlatformInformation = nullptr;
                }

                //
                // As part of arch initialization is platform calling conventions, reinitialize...
                //
                IfFailedReturn(InitArchBased());
            }
        }

        return hr;
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
    // ISvcSymbolBuilderManager:
    //

    // GetKernelAddressContext():
    //
    // If the target in question is a kernel mode target, this returns the address context for kernel mode
    // (non process specific) reads.  If not, this will fail.
    //
    IFACEMETHOD(GetKernelAddressContext)(_COM_Outptr_ ISvcAddressContext **ppKernelAddressContext);

    // ProcessKeyToProcess():
    //
    // Converts a process key to the process object for it.
    //
    IFACEMETHOD(ProcessKeyToProcess)(_In_ ULONG64 processKey,
                                   _COM_Outptr_ ISvcProcess **ppProcess);

    // PidToProcess():
    //
    // Converts a process id to the process object for it.
    //
    IFACEMETHOD(PidToProcess)(_In_ ULONG64 pid,
                              _COM_Outptr_ ISvcProcess **ppProcess);

    // ModuleBaseToModule():
    //
    // Converts a module base to the module object for it.
    //
    IFACEMETHOD(ModuleBaseToModule)(_In_opt_ ISvcProcess *pProcess,
                                    _In_ ULONG64 moduleBase,
                                    _COM_Outptr_ ISvcModule **ppModule)
    {
        *ppModule = nullptr;
        if (m_spModEnum == nullptr)
        {
            return E_FAIL;
        }

        return m_spModEnum->FindModuleAtAddress(pProcess, moduleBase, ppModule);
    }

    // TrackProcessForModule():
    //
    // For a given module, find its associated process, and create tracking structures associated with
    // that process.
    //
    IFACEMETHOD(TrackProcessForModule)(_In_ bool isKernel, 
                                       _In_ ISvcModule *pModule, 
                                       _COM_Outptr_ SymbolBuilderProcess **ppProcess);

    // TrackProcessForKey():
    //
    // Create tracking structures associated with a process by its unique key.
    //
    IFACEMETHOD(TrackProcessForKey)(_In_ bool isKernel,
                                    _In_ ULONG64 processKey,
                                    _COM_Outptr_ SymbolBuilderProcess **ppProcess);

    // TrackProcess():
    //
    // Create tracking structures associated with a process by its interface.
    //
    IFACEMETHOD(TrackProcess)(_In_ bool isKernel,
                              _In_ ISvcProcess *pProcess,
                              _COM_Outptr_ SymbolBuilderProcess **ppProcess);

    // FindInformationForRegisterByName():
    //
    // Find information for a given register by its name.
    //
    IFACEMETHOD(FindInformationForRegister)(_In_ PCWSTR pwszRegisterName,
                                            _Out_ RegisterInformation **ppRegisterInfo);

    // FindInformationForRegisterById():
    //
    // Find information for a given register by its canonical id.
    //
    IFACEMETHOD(FindInformationForRegisterById)(_In_ ULONG id,
                                                _Out_ RegisterInformation **ppRegisterInfo);

    // ParseLocation():
    //
    // Parses a location string to get a SvcSymbolLocation.
    //
    IFACEMETHOD(ParseLocation)(_In_ PCWSTR pwszLocation,
                               _Out_ SvcSymbolLocation *pLocation);

    // LocationToString():
    //
    // Converts a location to a displayable string for the associated architecture.
    //
    IFACEMETHOD(LocationToString)(_In_ SvcSymbolLocation const *pLocation,
                                  _Out_ std::wstring *pString);

    // GetDefaultCallingConvention():
    //
    // Gets an abstraction for the platform's default calling convention if we are aware of it; otherwise, 
    // this will fail.
    //
    IFACEMETHOD(GetDefaultCallingConvention)(_COM_Outptr_ CallingConvention **ppDefaultConvention);

    // GetServiceManager():
    //
    // Gets the service manager that this process was created from.
    //
    IDebugServiceManager *GetServiceManager() const
    {
        return m_pOwningManager;
    }

    // GetArchInfo():
    //
    // Gets information about the machine architecture that we are targeting.
    //
    ISvcMachineArchitecture *GetArchInfo() const
    {
        return m_spArchInfo.Get();
    }

    // GetVirtualMemory():
    //
    // Gets the virtual memory access interface for what we are targeting.
    //
    ISvcMemoryAccess *GetVirtualMemory() const
    {
        return m_spVirtualMemory.Get();
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialzie():
    //
    // Initializes the symbol builder manager for a given service container (e.g.: target).  If the 
    // target in question is a kernel mode (or similar hardware centric) target, a "default" address context
    // can be passed as 'pKernelAddressContext' such that memory reads to the kernel can take place outside
    // of the context of any particular process.
    //
    HRESULT RuntimeClassInitialize(_In_opt_ ISvcAddressContext *pKernelAddressContext = nullptr)
    {
        m_spKernelAddressContext = pKernelAddressContext;
        return S_OK;
    }

private:

    // InitArchBased():
    //
    // Initializes architecture based information.
    //
    HRESULT InitArchBased();

    // Parse*():
    //
    // Helpers for ParseLocation which parses individual elements out of the location string.
    //
    bool ParseHex(_In_ wchar_t const *pc, _Out_ wchar_t const **ppn, _Out_ ULONG64 *pValue);
    bool ParseReg(_In_ wchar_t const *pc, _Out_ wchar_t const **ppn, _Out_ RegisterInformation **ppReg);

    // A listing of our tracked processes.
    std::unordered_map<ULONG64, Microsoft::WRL::ComPtr<SymbolBuilderProcess>> m_trackedProcesses;

    // Information about registers so that we can manage live range information for variables.
    std::unordered_map<ULONG, RegisterInformation> m_regInfosById;
    std::unordered_map<std::wstring, ULONG> m_regIds;

    // Our container's process enumeration service.
    Microsoft::WRL::ComPtr<ISvcProcessEnumeration> m_spProcEnum;

    // Our container's module enumeration service.
    Microsoft::WRL::ComPtr<ISvcModuleEnumeration> m_spModEnum;

    // Our container's arch info service.
    Microsoft::WRL::ComPtr<ISvcMachineArchitecture> m_spArchInfo;

    // Our container's VM service.
    Microsoft::WRL::ComPtr<ISvcMemoryAccess> m_spVirtualMemory;

    // Our container's platform information.
    Microsoft::WRL::ComPtr<ISvcOSPlatformInformation> m_spOSPlatformInformation;

    // Our understanding of the default calling convention of the underlying platform (if we are aware of it)
    std::unique_ptr<CallingConvention> m_spDefaultCallingConvention;
	
    // If we are included for a kernel mode / hardware centric target, this is the default address
    // context of the kernel.
    Microsoft::WRL::ComPtr<ISvcAddressContext> m_spKernelAddressContext;

    // The service manager which contains and owns our lifetime.
    IDebugServiceManager *m_pOwningManager;

};


} // SymbolBuilder
} // Services
} // TargetComposition
} // Services

#endif __SYMMANAGER_H__
