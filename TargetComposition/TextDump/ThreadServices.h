//**************************************************************************
//
// ThreadServices.h
//
// Target composition services to provide thread information to the debugger
// from our "text dump" file format.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __THREADSERVICES_H__
#define __THREADSERVICES_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

class ThreadEnumerationService;

// Thread:
//
// Represents the single thread that we target in our "text dump" format.
//
// NOTE: The following interfaces are also relevant to threads:
//
//     ISvcDescription:                         // Optional (for many service provided objects *including* threads)
//
class Thread :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcThread,                             // Mandatory (for all threads)
        ISvcExecutionUnit                       // Mandatory (for all code executors *including* threads)
        >
{
public:

    //*************************************************
    // ISvcThread:
    //

    // GetContainingProcessKey():
    //
    // Gets the unique key of the process to which this thread belongs.  This is the same key returned
    // from the containing ISvcProcess's GetKey method.
    //
    IFACEMETHOD(GetContainingProcessKey)(_Out_ PULONG64 pContainingProcessKey)
    {
        //
        // As we only have one thread and one process (hard coded), we simply return what we know is the key here.
        //
        *pContainingProcessKey = 1;
        return S_OK;
    }

    // GetKey():
    //
    // Returns a unique key for the thread.
    //
    IFACEMETHOD(GetKey)(_Out_ PULONG64 pThreadKey)
    {
        //
        // We do not have a TID or the address of a kernel thread object, so we simply return "1" as our
        // unique key here.
        //
        *pThreadKey = 1;
        return S_OK;
    }

    // GetId():
    //
    // Returns the thread ID of the thread.
    //
    IFACEMETHOD(GetId)(_Out_ PULONG64 pThreadId)
    {
        //
        // We do not have a real PID.  Simply return "1".
        //
        *pThreadId = 1;
        return S_OK;
    }

    //*************************************************
    // ISvcExecutionUnit:
    //

    // GetContext():
    //
    // Gets a context record for the thread.
    //
    IFACEMETHOD(GetContext)(_In_ SvcContextFlags contextFlags,
                            _Out_ ISvcRegisterContext **ppRegisterContext);

    // SetContext():
    //
    // Sets a context record for the thread.
    //
    IFACEMETHOD(SetContext)(_In_ SvcContextFlags /*contextFlags*/,
                            _In_ ISvcRegisterContext * /*pRegisterContext*/)
    {
        return E_NOTIMPL;
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializer.
    //
    HRESULT RuntimeClassInitialize(_In_ ThreadEnumerationService *pThreadService,
                                   _In_ std::shared_ptr<TextDumpParser> &parsedFile)
    {
        m_spThreadService = pThreadService;
        m_spParsedFile = parsedFile;
        return S_OK;
    }

private:
    
    Microsoft::WRL::ComPtr<ThreadEnumerationService> m_spThreadService;
    std::shared_ptr<TextDumpParser> m_spParsedFile;

};

// ThreadEnumerator:
//
// A thread enumerator that enumerates the single thread that we target in our "text dump" format.
//
class ThreadEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcThreadEnumerator                    // Mandatory (for all thread enumerators)
        >
{
public:

    //*************************************************
    // ISvcProcessEnumerator:
    //

    // Reset():
    //
    // Resets the enumerator.
    //
    IFACEMETHOD(Reset)()
    {
        m_enumerated = false;
        return S_OK;
    }

    // GetNext():
    //
    // Gets the next thread from the enumerator.
    //
    IFACEMETHOD(GetNext)(_COM_Outptr_ ISvcThread **ppTargetThread)
    {
        //
        // If we have not yet enumerated our single thread, do so.
        //
        HRESULT hr = S_OK;
        *ppTargetThread = nullptr;

        if (m_enumerated)
        {
            return E_BOUNDS;
        }

        Microsoft::WRL::ComPtr<Thread> spThread;
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<Thread>(&spThread, m_spThreadService.Get(), m_spParsedFile));
        m_enumerated = true;

        *ppTargetThread = spThread.Detach();
        return hr;
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializer.
    //
    HRESULT RuntimeClassInitialize(_In_ ThreadEnumerationService *pThreadService,
                                   _In_ std::shared_ptr<TextDumpParser> &parsedFile)
    {
        m_spThreadService = pThreadService;
        m_spParsedFile = parsedFile;
        return Reset();
    }

private:

    Microsoft::WRL::ComPtr<ThreadEnumerationService> m_spThreadService;
    std::shared_ptr<TextDumpParser> m_spParsedFile;
    bool m_enumerated;

};

// ThreadEnumerationService:
//
// A thread enumeration service that provides the target process in the text dump to the debugger.
//
class ThreadEnumerationService :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugServiceLayer,                     // Mandatory (for all services)
        ISvcThreadEnumeration                   // Mandatory (for all thread enumeration services)
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
        IfFailedReturn(pServiceManager->RegisterService(DEBUG_SERVICE_THREAD_ENUMERATOR, this));
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
                                        _Out_writes_(sizeSoftDependencies) GUID * /*pSoftDependencies*/,
                                        _Out_ ULONG64 *pNumSoftDependencies)
    {
        HRESULT hr = S_OK;

        //
        // We need information about the machine architecture to set register values according to what's
        // in the "text dump" so we must express the dependency here.
        //
        if (sizeHardDependencies == 0 && sizeSoftDependencies == 0)
        {
            *pNumHardDependencies = 1;
            *pNumSoftDependencies = 0;
            return S_OK;
        }

        if (sizeHardDependencies < 1)
        {
            return E_INVALIDARG;
        }

        pHardDependencies[0] = DEBUG_SERVICE_ARCHINFO;

        *pNumHardDependencies = 1;
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
                                    _In_ IDebugServiceManager *pServiceManager,
                                    _In_ REFIID /*serviceGuid*/)
    {
        HRESULT hr = S_OK;
  
        //
        // Because we have expressed a hard dependency on DEBUG_SERVICE_ARCHINFO, it is guaranteed to be in the
        // container and initialized prior to our initialization (or the overall initialization of the service
        // container would have failed with a missing dependency).  Therefore, we can safely get it here and fail
        // if, for any reason, it is not present (such would be erroneous).
        //
        // One thing to note here, *WE* did not place DEBUG_SERVICE_ARCHINFO in the container (although it is perfectly
        // legal for us to do so).  Because it is required and the debugger has an understanding of this machine
        // architecture since our DEBUG_SERVICE_MACHINE declared IMAGE_FILE_MACHINE_AMD64, the debugger inserts this
        // on our behalf.
        //
        IfFailedReturn(pServiceManager->QueryService(DEBUG_SERVICE_ARCHINFO, IID_PPV_ARGS(&m_spMachineArch)));

        //
        // NOTE: It is *TOO EARLY* during debugger initialization to ask the service to enumerate registers here.
        //       We will do this later, upon demand.
        //

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
        //
        // Services can come and go (or change) dynamically.  This doesn't typically happen for a static
        // target like a dump.  Note, however, that plug-ins can come in and make such changes.  We need to be
        // responsive to such.
        //
        // It's highly unlikely that someone would change the architecture service underneath us; however, the code
        // is shown here as a template of dealing with changes.
        //
        HRESULT hr = S_OK;
        if (serviceGuid == DEBUG_SERVICE_ARCHINFO)
        {
            m_spMachineArch = nullptr;
            if (pNewService != nullptr)
            {
                IfFailedReturn(pNewService->QueryInterface(IID_PPV_ARGS(&m_spMachineArch)));
            }
            IfFailedReturn(InitializeRegisterMappings());
        }

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
    // ISvcThreadEnumerator
    //

    // FindThread():
    //
    // Finds a thread by a unique key.
    //
    IFACEMETHOD(FindThread)(_In_ ISvcProcess *pProcess,
                            _In_ ULONG64 threadKey,
                            _COM_Outptr_ ISvcThread **ppTargetThread);

    // EnumerateThreads():
    //
    // Returns an enumerator which enumerates all threads in our "text dump".
    //
    IFACEMETHOD(EnumerateThreads)(_In_ ISvcProcess *pProcess, _COM_Outptr_ ISvcThreadEnumerator **ppEnum);

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

    // GetRegisterMappings():
    //
    // Gets our understanding of the mappings of register name -> canonical ID for the architecture
    // that we are targeting.
    //
    HRESULT GetRegisterMappings(_Out_ std::unordered_map<std::wstring, ULONG> const **ppRegisterMappings)
    {
        HRESULT hr = S_OK;
        if (m_registerMappings.empty() && m_spMachineArch != nullptr)
        {
            IfFailedReturn(InitializeRegisterMappings());
        }
        *ppRegisterMappings = &m_registerMappings;
        return hr;
    }

    // GetMachineArch():
    //
    // Gets our access to the machine architecture service.
    //
    ISvcMachineArchitecture *GetMachineArch() const { return m_spMachineArch.Get(); }

private:

    // InitializeRegisterMappings():
    //
    // Go ask the machine architecture service for all the registers and go and get their mappings.
    //
    HRESULT InitializeRegisterMappings();

    std::unordered_map<std::wstring, ULONG> m_registerMappings;
    Microsoft::WRL::ComPtr<ISvcMachineArchitecture> m_spMachineArch;
    std::shared_ptr<TextDumpParser> m_spParsedFile;
};

} // TextDump
} // Services
} // TargetComposition
} // Debugger

#endif // __THREADSERVICES_H__
