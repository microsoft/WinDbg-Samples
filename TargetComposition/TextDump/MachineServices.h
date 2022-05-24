//**************************************************************************
//
// MachineServices.h
//
// Target composition services to provide information about the type of machine
// our "text dump" file format is running on.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __MACHINESERVICES_H__
#define __MACHINESERVICES_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

// MachineService:
//
// A machine service that provides configuration information about the "text dump" that we are
// targeting to the debugger.
//
// NOTE: The following interfaces are also relevant to machine services:
//
//     ISvcMachineDebug:                        // Mandatory (for targeting hardware or a "kernel mode" connection)
//     ISvcMachineConfiguration2:               // Mandatory (for targeting a custom architecture)
//
class MachineService :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugServiceLayer,                     // Mandatory (for any service)
        ISvcMachineConfiguration                // Mandatory (for any machine service)
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
        IfFailedReturn(pServiceManager->RegisterService(DEBUG_SERVICE_MACHINE, this));
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
    // ISvcMachineConfiguration
    //

    // GetArchitecture():
    //
    // Returns the archtiecture of the machine as an IMAGE_FILE_MACHINE_* constant.
    //
    IFACEMETHOD_(ULONG, GetArchitecture)()
    {
        //
        // Our "text dump" format is hard coded to x64 at the moment.  For more real world targets,
        // this would read some information from the dump and provide the appropriate constant.
        //
        return IMAGE_FILE_MACHINE_AMD64;
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the machine service.
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

#endif // __MACHINESERVICES_H__
