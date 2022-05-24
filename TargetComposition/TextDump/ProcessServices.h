//**************************************************************************
//
// ProcessServices.h
//
// Target composition services to provide process information to the debugger
// from our "text dump" file format.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __PROCESSSERVICES_H__
#define __PROCESSSERVICES_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

// Process:
//
// Represents the single process that we target in our "text dump" format.  Note that if we had more information
// about the target process (e.g.: it's name, arguments, or parent process), we could implement
// the ISvcProcessBasicInformation interface.  That's optional and we do not implement it here.
//
// It's important to note that *anything* which is or represents an address context must implement
// ISvcAddressContext in addition to their default interface.  That includes processes; however, it also includes
// things like CPU cores (in a kernel mode target) which have an implicit address context by way of the hardware
// registers which point to a page directory / set of page tables.
//
// NOTE: The following interfaces are also relevant to processes:
//
//     ISvcDescription:                         // Optional (for many service provided objects *including* processes)
//     ISvcProcessBasicInformation:             // Optional (for any process)
//
class Process :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcProcess,                            // Mandatory (for all processes)
        ISvcAddressContext                      // Mandatory (for all address contexts *including* processes)
        >
{
public:

    //*************************************************
    // ISvcProcess:
    //

    // GetKey():
    //
    // Returns a unique key for the process.
    //
    IFACEMETHOD(GetKey)(_Out_ PULONG64 pProcessKey)
    {
        //
        // We do not have a PID or the address of a kernel process object, so we simply return "1" as our
        // unique key here.
        //
        *pProcessKey = 1;
        return S_OK;
    }

    // GetId():
    //
    // Returns the process ID for the process.
    //
    IFACEMETHOD(GetId)(_Out_ PULONG64 pProcessId)
    {
        //
        // We do not have a real PID.  Simply return "1".
        //
        *pProcessId = 1;
        return S_OK;
    }

    //*************************************************
    // ISvcAddressContext:
    //

    // GetAddressContextKind():
    //
    // Gets the kind of address context.  As this represents a process, it returns such.
    //
    IFACEMETHOD_(AddressContextKind, GetAddressContextKind)()
    {
        return AddressContextProcess;
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializer.
    //
    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> &parsedFile)
    {
        m_spParsedFile = parsedFile;
        return S_OK;
    }

private:
    
    std::shared_ptr<TextDumpParser> m_spParsedFile;

};

// ProcessEnumerator:
//
// A process enumerator that enumerates the single process that we target in our "text dump" format.
//
class ProcessEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcProcessEnumerator                   // Mandatory (for all process enumerators)
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
    // Gets the next process from the enumerator.
    //
    IFACEMETHOD(GetNext)(_COM_Outptr_ ISvcProcess **ppTargetProcess)
    {
        //
        // If we have not yet enumerated our single process, do so.
        //
        HRESULT hr = S_OK;
        *ppTargetProcess = nullptr;

        if (m_enumerated)
        {
            return E_BOUNDS;
        }

        Microsoft::WRL::ComPtr<Process> spProcess;
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<Process>(&spProcess, m_spParsedFile));
        m_enumerated = true;

        *ppTargetProcess = spProcess.Detach();
        return hr;
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializer.
    //
    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> &parsedFile)
    {
        m_spParsedFile = parsedFile;
        return Reset();
    }

private:

    std::shared_ptr<TextDumpParser> m_spParsedFile;
    bool m_enumerated;

};

// ProcessEnumerationService:
//
// A process enumeration service that provides the target process in the text dump to the debugger.
//
class ProcessEnumerationService :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugServiceLayer,                     // Mandatory (for all services)
        ISvcProcessEnumeration                  // Mandatory (for all process enumeration services)
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
        IfFailedReturn(pServiceManager->RegisterService(DEBUG_SERVICE_PROCESS_ENUMERATOR, this));
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
    // ISvcProcessEnumerator
    //

    // FindProcess():
    //
    // Finds a process by a unique key.
    //
    IFACEMETHOD(FindProcess)(_In_ ULONG64 processKey,
                             _COM_Outptr_ ISvcProcess **ppTargetProcess);

    // EnumerateProcesses():
    //
    // Returns an enumerator which enumerates all processes in our "text dump"
    //
    IFACEMETHOD(EnumerateProcesses)(_COM_Outptr_ ISvcProcessEnumerator **ppEnum);

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
