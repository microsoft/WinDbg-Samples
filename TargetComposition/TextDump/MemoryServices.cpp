//**************************************************************************
//
// MemoryServices.cpp
//
// Target composition services to provide memory access to the debugger
// from our "text dump" file format.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "TextDump.h"

extern IDebugTargetComposition *g_pCompositionManager;

using namespace Microsoft::WRL;

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

HRESULT VirtualMemoryService::ReadMemory(_In_ ISvcAddressContext * /*pAddressContext*/,
                                         _In_ ULONG64 offset,
                                         _Out_writes_(bufferSize) PVOID pBuffer,
                                         _In_ ULONG64 bufferSize,
                                         _Out_ PULONG64 pBytesRead)
{
    //
    // NOTE: For this particular format, the "address context" is largely not relevant.  If we were targeting
    //       multiple processes in a user mode form, the address context would represent which process we were
    //       attempting to read memory within.  If we were targeting some kernel mode form, the address context
    //       would either represent a particular process or a particular core.
    //

    //
    // We need to read "bufferSize" bytes at "offset".  While it is unlikely in the "text dump" format that
    // we would have this come from multiple regions, the code here is written to deal with that as an example
    // since it is incredibly common in many formats.
    //
    // This method returns either:
    //
    //     - S_OK: All the bytes were successfully read.  *pBytesRead == bufferSize at return.
    //     - S_FALSE: Some bytes were successfully read.  *pBytesRead < bufferSize at return.
    //     - <FAILURE>: No bytes were read.  Other return values are irrelevant.
    //

    ULONG64 bytesRead = 0;
    ULONG64 curOffset = offset;
    ULONG64 remaining = bufferSize;
    unsigned char *pCurBuffer = reinterpret_cast<unsigned char *>(pBuffer);

    while(remaining > 0)
    {
        MemoryRegion const* pCurRegion = FindTextDumpMemoryRegion(curOffset);
        if (pCurRegion == nullptr)
        {
            break;
        }

        ULONG64 chunkOffset = curOffset - pCurRegion->StartAddress;
        ULONG64 chunkRemaining = pCurRegion->EndAddress - chunkOffset;
        ULONG64 chunkToRead = (chunkRemaining > remaining) ? remaining : chunkRemaining;

        memcpy(pCurBuffer, &(pCurRegion->Data[static_cast<size_t>(chunkOffset)]), static_cast<size_t>(chunkToRead));

        remaining -= chunkToRead;
        bytesRead += chunkToRead;
        pCurBuffer += chunkToRead;
        curOffset += chunkToRead;
    }

    if (bytesRead == 0)
    {
        return E_FAIL;
    }

    *pBytesRead = bytesRead;
    return (*pBytesRead == bufferSize ? S_OK : S_FALSE);
}

HRESULT VirtualMemoryService::FindMemoryRegion(_In_opt_ ISvcAddressContext * /*pAddressContext*/,
                                               _In_ ULONG64 offset,
                                               _COM_Outptr_ ISvcMemoryRegion **ppRegion)
{
    HRESULT hr = S_OK;
    *ppRegion = nullptr;

    //
    // This method returns:
    //
    //     S_OK:      offset is within the region described by *ppRegion
    //     S_FALSE:   offset is not within any memory region.  The region described by *ppRegion is the next higher
    //                valid memory address within the virtual address space.
    //     E_BOUNDS:  offset is not within any memory region.  There is no next higher valid memory address within
    //                the virtual address space
    //     <FAILURE>: other failure (e.g.: memory allocation error)
    //

    ComPtr<VirtualMemoryRegion> spRegion;
    MemoryRegion const* pNextHigherOffsetRegion = nullptr;

    auto&& memoryRegions = m_spParsedFile->GetMemoryRegions();

    for (auto&& region : memoryRegions)
    {
        if (offset >= region.StartAddress && offset < region.EndAddress)
        {
            IfFailedReturn(MakeAndInitialize<VirtualMemoryRegion>(&spRegion, 
                                                                  region.StartAddress,
                                                                  region.EndAddress - region.StartAddress));

            //
            // S_OK: It is within the described region.
            //
            *ppRegion = spRegion.Detach();
            return S_OK;
        }
        else if (region.StartAddress > offset)
        {
            //
            // It's a higher memory region.  Is it the closest one we have yet found...?
            //
            if (pNextHigherOffsetRegion == nullptr || region.StartAddress < pNextHigherOffsetRegion->StartAddress)
            {
                pNextHigherOffsetRegion = &region;
            }
        }
    }

    //
    // If we've gotten here, "offset" is not within any region in our format.  If pNextHigherOffsetRegion is set,
    // we have found the "next higher" address region and must return that.
    //
    if (pNextHigherOffsetRegion == nullptr)
    {
        return E_BOUNDS;
    }

    IfFailedReturn(MakeAndInitialize<VirtualMemoryRegion>(
        &spRegion,
        pNextHigherOffsetRegion->StartAddress,
        pNextHigherOffsetRegion->EndAddress - pNextHigherOffsetRegion->StartAddress));

    *ppRegion = spRegion.Detach();

    //
    // S_FALSE: We returned a region.  "offset" is not contained within it.  It is the next higher VA region
    //
    return S_FALSE;
}

HRESULT VirtualMemoryService::EnumerateMemoryRegions(_In_opt_ ISvcAddressContext * /*pAddressContext*/,
                                                     _COM_Outptr_ ISvcMemoryRegionEnumerator **ppRegionEnum)
{
    HRESULT hr = S_OK;
    *ppRegionEnum = nullptr;

    ComPtr<VirtualMemoryRegionEnumerator> spEnum;
    IfFailedReturn(MakeAndInitialize<VirtualMemoryRegionEnumerator>(&spEnum, m_spParsedFile));

    *ppRegionEnum = spEnum.Detach();
    return hr;
}

HRESULT VirtualMemoryService::NotifyEvent(_In_ IDebugServiceManager *pServiceManager,
                                          _In_ REFIID eventGuid,
                                          _In_opt_ IUnknown * /*pEventArgument*/)
{
    HRESULT hr = S_OK;

    //
    // The only event we care about is the "module enumeration complete" event that we fire (and registered
    // for an event notification).
    //
    if (eventGuid == DEBUG_TEXTDUMPEVENT_MODULEENUMERATIONCOMPLETE)
    {
        //
        // Once we have heard that the debugger has fully queried our module list, we are going to *ALTER* the 
        // service container to enable memory reads within image backed VA regions to be satisfied from images
        // pulled from the symbol server.
        //
        // It is perfectly legal to do this at the outset (in our activator); however, that will tend to cause
        // the *CURRENT* debugger to pull all images from the symbol server immediately upon startup as it tries
        // to read header bytes.  
        //
        // This plug-in defers this particular bit until after the first complete module enumeration for this reason.
        // This is a current performance optimization only (and may not be necessary in the future).
        //
        if (g_pCompositionManager)
        {
            ComPtr<IDebugServiceLayer> spImageBackedVirtualMemory;
            IfFailedReturn(g_pCompositionManager->CreateComponent(DEBUG_COMPONENTSVC_IMAGEBACKED_VIRTUALMEMORY,
                                                                  &spImageBackedVirtualMemory));

            //
            // The image backed virtual memory component has an initializer...  That initializer is
            // the IComponentImageBackedVirtualMemoryInitializer interface.  Make sure to initialize the
            // component before putting it in the service container!
            //
            ComPtr<IComponentImageBackedVirtualMemoryInitializer> spImageBackedVirtualMemoryInitializer;
            IfFailedReturn(spImageBackedVirtualMemory.As(&spImageBackedVirtualMemoryInitializer));
            IfFailedReturn(spImageBackedVirtualMemoryInitializer->Initialize(this, true));

            //
            // It is important to note that after this call, the virtual memory service is now stacked:
            //
            //     <Image Backed Virtual Memory>
            //                 ^
            //                 |
            //                 v
            //         <Our Virtual Memory>
            //
            // The image backed virtual memory service will delegate to the service it sits atop (our
            // virtual memory service).  If any bytes to read are reported as unavailable from our virtual
            // memory service, it will attempt to provide them from image files.  In order for this service
            // to do such, it must have access to:
            //
            //     - An image provider service
            //     - An image parse provider service
            //
            // We already added those to the service container above.
            //
            IfFailedReturn(spImageBackedVirtualMemory->RegisterServices(pServiceManager));

            (void)DiagnosticLog(m_spDiagnosticLogging.Get(),
                                DiagnosticLevelInfo,
                                m_diagLevel,
                                L"TextDump",
                                L"VirtualMemory",
                                L"Mapping image backed pages into virtual address space");
        }
    }

    return hr;
}

MemoryRegion const* VirtualMemoryService::FindTextDumpMemoryRegion(_In_ ULONG64 address)
{
    for (auto&& region : m_spParsedFile->GetMemoryRegions())
    {
        if (address >= region.StartAddress && address < region.EndAddress)
        {
            return &region;
        }
    }

    return nullptr;
}

} // TextDump
} // Services
} // TargetComposition
} // Debugger

