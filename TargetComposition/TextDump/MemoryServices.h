//**************************************************************************
//
// MemoryServices.h
//
// Target composition services to provide memory access to the debugger
// from our "text dump" file format.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __MEMORYSERVICES_H__
#define __MEMORYSERVICES_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

// VirtualMemoryRegion:
//
// A description of a memory region in our "text dump" format.
//
class VirtualMemoryRegion :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcMemoryRegion                        // Mandatory (for any memory region)
        >
{
public:

    //*************************************************
    // ISvcMemoryRegion:
    //

    // GetRange():
    //
    // Gets the bounds of this memory region.
    //
    IFACEMETHOD(GetRange)(_Out_ SvcAddressRange *pRange)
    {
        *pRange = { m_startAddress, m_size };
        return S_OK;
    }

    // IsReadable():
    //
    // Indicates whether this region of memory is readable.  If the implementation cannot make a determination
    // of whether the range is readable or not, E_NOTIMPL may legally be returned.
    //
    IFACEMETHOD(IsReadable)(_Out_ bool *pIsReadable)
    {
        //
        // Since our "text dump" format is only including "readable" memory, indicate that it is readable.
        // For any other property (W/X), we will indicate that we do not know via an E_NOTIMPL return.
        //
        *pIsReadable = true;
        return S_OK;
    }

    // IsWriteable():
    //
    // Indicates whether this region of memory is writeable.  If the implementation cannot make a determination
    // of whether the range is writeable or not, E_NOTIMPL may legally be returned.
    //
    STDMETHOD(IsWriteable)(_Out_ bool * /*pIsWriteable*/)
    {
        return E_NOTIMPL;
    }

    // IsExecutable():
    //
    // Indicates whether this region of memory is executable.  If the implementation cannot make a determination
    // of whether the range is executable or not, E_NOTIMPL may legally be returned.
    //
    STDMETHOD(IsExecutable)(_Out_ bool * /*pIsExecutable*/)
    {
        return E_NOTIMPL;
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the virtual memory region.
    //
    HRESULT RuntimeClassInitialize(_In_ ULONG64 startAddress,
                                   _In_ ULONG64 size)
    {
        m_startAddress = startAddress;
        m_size = size;
        return S_OK;
    }

private:

    ULONG64 m_startAddress;
    ULONG64 m_size;

};

// VirtualMemoryRegionEnumerator:
//
// An enumerator which can enumerate all the virtual memory regions within our "text dump" format.
//
class VirtualMemoryRegionEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcMemoryRegionEnumerator              // Mandatory (for any memory region enumerator)
        >
{
public:

    //*************************************************
    // ISvcMemoryRegionEnumerator:
    //

    // Reset():
    //
    // Resets the enumerator back to its initial creation state.
    //
    IFACEMETHOD(Reset)()
    {
        m_pos = 0;
        return S_OK;
    }

    // GetNext():
    //
    // Gets the next memory region.
    //
    IFACEMETHOD(GetNext)(_COM_Outptr_ ISvcMemoryRegion **ppRegion)
    {
        *ppRegion = nullptr;

        auto&& memoryRegions = m_spParsedFile->GetMemoryRegions();

        //
        // When we hit the end of the enumerator, we return the specific E_BOUNDS error.
        //
        if (m_pos >= memoryRegions.size())
        {
            return E_BOUNDS;
        }

        MemoryRegion const& region = memoryRegions[m_pos];
        ++m_pos;

        Microsoft::WRL::ComPtr<VirtualMemoryRegion> spRegion;
        HRESULT hr = Microsoft::WRL::MakeAndInitialize<VirtualMemoryRegion>(&spRegion, 
                                                                            region.StartAddress,
                                                                            region.EndAddress - region.StartAddress);
        if (FAILED(hr))
        {
            //
            // If we failed to create the region, just put the enumerator at the end of the list.
            //
            m_pos = memoryRegions.size();
            return hr;
        }

        *ppRegion = spRegion.Detach();
        return S_OK;
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the memory region enumerator.
    //
    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> &parsedFile)
    {
        m_spParsedFile = parsedFile;
        return Reset();
    }

private:

    std::shared_ptr<TextDumpParser> m_spParsedFile;
    size_t m_pos;

};

// VirtualMemoryService:
//
// A virtual memory service that provides the "memory regions" in the text dump to the debugger.
//
class VirtualMemoryService :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugServiceLayer,                     // Mandatory (for any service)
        ISvcMemoryAccess,                       // Mandatory (for any memory service: virtual or otherwise)
        ISvcMemoryInformation,                  // Optional (for any memory service)
        ISvcDiagnosticLoggableControl           // Optional (for any service)
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
        IfFailedReturn(pServiceManager->RegisterService(DEBUG_SERVICE_VIRTUAL_MEMORY, this));
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
    IFACEMETHOD(InitializeServices)(_In_ ServiceNotificationKind notificationKind,
                                    _In_ IDebugServiceManager *pServiceManager,
                                    _In_ REFIID /*serviceGuid*/)
    {
        HRESULT hr = S_OK;

        //
        // Listen for the first complete module enumeration (we fire this event) as a performance optimization
        // around *WHEN* to enable image backed virtual memory.
        //
        // NOTE: When the image backed virtual memory service comes in and initializes itself, it will pass a stacked
        //       notification down.  WE **DO NOT** want to do anything when that happens!
        //
        if (notificationKind == ServiceManagerNotification)
        {
            IfFailedReturn(pServiceManager->RegisterEventNotification(DEBUG_TEXTDUMPEVENT_MODULEENUMERATIONCOMPLETE, this));
        }

        //
        // Get the diagnostic logging service.  If this fails, it does not matter -- we simply won't produce
        // log messages.
        //
        (void)pServiceManager->QueryService(DEBUG_SERVICE_DIAGNOSTIC_LOGGING, IID_PPV_ARGS(&m_spDiagnosticLogging));

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

        if (serviceGuid == DEBUG_SERVICE_DIAGNOSTIC_LOGGING)
        {
            m_spDiagnosticLogging = nullptr;
            if (pNewService != nullptr)
            {
                IfFailedReturn(pNewService->QueryInterface(IID_PPV_ARGS(&m_spDiagnosticLogging)));
            }
        }

        return hr;
    }

    // NotifyEvent():
    //
    // Called to notify this component that an event of interest occurred.
    //
    IFACEMETHOD(NotifyEvent)(_In_ IDebugServiceManager *pServiceManager,
                             _In_ REFIID eventGuid,
                             _In_opt_ IUnknown *pEventArgument);

    //*************************************************
    // ISvcMemoryAccess:
    //

    // ReadMemory():
    //
    // Reads virtual memory from the "text dump"
    //
    IFACEMETHOD(ReadMemory)(_In_ ISvcAddressContext *pAddressContext,
                            _In_ ULONG64 offset,
                            _Out_writes_(bufferSize) PVOID pBuffer,
                            _In_ ULONG64 bufferSize,
                            _Out_ PULONG64 pBytesRead);

    // WriteMemory():
    //
    // Writes virtual memory to the "text dump"
    //
    IFACEMETHOD(WriteMemory)(_In_ ISvcAddressContext * /*pAddressContext*/,
                             _In_ ULONG64 /*offset*/,
                             _In_reads_(bufferSize) PVOID /*pBuffer*/,
                             _In_ ULONG64 bufferSize,
                             _Out_ PULONG64 /*pBytesWritten*/)
    {
        UNREFERENCED_PARAMETER(bufferSize);
        return E_NOTIMPL;
    }

    //*************************************************
    // ISvcMemoryInformation:
    //
    // NOTE: It is completely optional to implement this.  It allows more functionality within the debugger;
    //       however, a basic memory service is only REQUIRED to implement ISvcMemoryAccess.
    //

    // FindMemoryRegion():
    //
    // If Offset is contained within a valid memory region in the given address space, an ISvcMemoryRegion
    // describing that memory region is returned along with an S_OK result.  If, on the other hand, Offset
    // is not within a valid memory region in the given address space, the implementation will find the
    // next valid memory region with a starting address greater than Offset within the address space and
    // return an ISvcMemoryRegion describing that along with an S_FALSE result.  If there is no "next higher"
    // address region, E_BOUNDS will be returned.
    //
    IFACEMETHOD(FindMemoryRegion)(_In_opt_ ISvcAddressContext *pAddressContext,
                                  _In_ ULONG64 offset,
                                  _COM_Outptr_ ISvcMemoryRegion **ppRegion);

    // EnumerateMemoryRegions():
    //
    // Enumerates all memory regions in the address space in *ARBITRARY ORDER*.  One can achieve a monotonically
    // increasing enumeration by repeatedly calling FindMemoryRegion starting with an Offset of zero.
    //
    IFACEMETHOD(EnumerateMemoryRegions)(_In_opt_ ISvcAddressContext *pAddressContext,
                                        _COM_Outptr_ ISvcMemoryRegionEnumerator **ppRegionEnum);

    //*************************************************
    // ISvcDiagnosticLoggableControl:
    //

    IFACEMETHOD_(DiagnosticLogLevel, GetLoggingLevel)() { return m_diagLevel; }
    IFACEMETHOD_(void, SetLoggingLevel)(_In_ DiagnosticLogLevel level) { m_diagLevel = level; }

    //*************************************************
    // Internal:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the virtual memory service.
    //
    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> const& parsedFile)
    {
        m_spParsedFile = parsedFile;
        m_diagLevel = DiagnosticLevelInfo;
        return S_OK;
    }

private:

    // FindTextDumpMemoryRegion():
    //
    // Gets the memory region within the text dump for the given offset.  This will return nullptr if no such
    // memory region can be found.
    //
    MemoryRegion const* FindTextDumpMemoryRegion(_In_ ULONG64 address);

    std::shared_ptr<TextDumpParser> m_spParsedFile;

    //
    // Optional: Diagnostic logging service
    //
    Microsoft::WRL::ComPtr<ISvcDiagnosticLogging> m_spDiagnosticLogging;
    DiagnosticLogLevel m_diagLevel;
};
	

} // TextDump
} // Services
} // TargetComposition
} // Debugger

#endif // __MEMORYSERVICES_H_
