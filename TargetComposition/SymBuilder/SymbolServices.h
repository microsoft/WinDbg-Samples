//**************************************************************************
//
// SymbolServices.h
//
// Services related to providing symbols.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __SYMBOLSERVICES_H__
#define __SYMBOLSERVICES_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

// SymbolProvider:
//
// Our "symbol provider" service.  Any time the debugger goes and looks for symbols for a particular module, it
// will look to see if there is a symbol provider in the service container.  If so, it will call it to ask whether
// or not it wants to provide symbols.
//
// Note that multiple symbol providers can be aggregated together and can be asked, each in turn, whether they 
// have symbols for a particular module.
//
class SymbolProvider :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugServiceLayer,
        ISvcSymbolProvider
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
        IfFailedReturn(pServiceManager->RegisterService(DEBUG_SERVICE_SYMBOL_PROVIDER, this));
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

        pHardDependencies[0] = DEBUG_PRIVATE_SERVICE_SYMBOLBUILDER_MANAGER;

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
        // We have a hard dependency on the symbol builder manager.
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
        (void)pServiceManager->QueryService(DEBUG_PRIVATE_SERVICE_SYMBOLBUILDER_MANAGER, IID_PPV_ARGS(&m_spSymManager));

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
        if (serviceGuid == DEBUG_PRIVATE_SERVICE_SYMBOLBUILDER_MANAGER)
        {
            //
            // If the symbol builder manager service changed, alter our cached copy of it so that we are calling
            // the correct service!
            //
            m_spSymManager = nullptr;
            if (pNewService != nullptr)
            {
                //
                // We *MUST* support that interface.  In reality, no one should come in and change this since it's
                // a private interface that only we know about.  But still...
                //
                IfFailedReturn(pNewService->QueryInterface(IID_PPV_ARGS(&m_spSymManager)));
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
    // ISvcSymbolProvider:
    //

    // LocateSymbolsForImage():
    //
    // For a given image (identified by an ISvcModule), find the set of symbolic information available for the image
    // and return a symbol set.
    //
    IFACEMETHOD(LocateSymbolsForImage)(_In_ ISvcModule *pImage,
                                       _COM_Outptr_ ISvcSymbolSet **ppSymbolSet);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes a new symbol builder symbol provider.
    //
    HRESULT RuntimeClassInitialize()
    {
        return S_OK;
    }

private:

    //
    // Cached copy of the symbol builder manager that we placed in the container.  This tracks
    // everything associated with what symbols we have constructed, etc...
    //
    Microsoft::WRL::ComPtr<ISvcSymbolBuilderManager> m_spSymManager;

};

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger

#endif // __SYMBOLSERVICES_H__

