//**************************************************************************
//
// SymbolServices.cpp
//
// Services related to providing symbols.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "SymBuilder.h"

using namespace Microsoft::WRL;

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace SymbolBuilder
{

HRESULT SymbolProvider::LocateSymbolsForImage(_In_ ISvcModule *pImage,
                                              _COM_Outptr_ ISvcSymbolSet **ppSymbolSet)
{
    HRESULT hr = S_OK;
    *ppSymbolSet = nullptr;

    //
    // Be a good citizen if there are multiple symbol providers in the container.  In such cases, we are expected
    // to return E_UNHANDLED_REQUEST_TYPE if the symbol format isn't one that we support.  Given that we are
    // "on demand" and only when created by an API, this should be **OUR** default error code.  It may not be the
    // default error code of some other symbol provider.
    //
    // E_UNHANDLED_REQUEST_TYPE generally means "not me...  move on to the next provider in order"
    //

    if (m_spSymManager == nullptr)
    {
        return E_UNHANDLED_REQUEST_TYPE;
    }

    ULONG64 moduleKey;
    IfFailedReturn(pImage->GetKey(&moduleKey));

    ComPtr<SymbolBuilderProcess> spSymProcess;
    IfFailedReturn(m_spSymManager->TrackProcessForModule(pImage, &spSymProcess));

    ComPtr<SymbolSet> spSymbolSet;
    if (spSymProcess->TryGetSymbolsForModule(moduleKey, &spSymbolSet))
    {
        *ppSymbolSet = spSymbolSet.Detach();
        return S_OK;
    }

    return E_UNHANDLED_REQUEST_TYPE;
}

} // SymbolBuilder
} // Services
} // TargetComposition
} // Debugger
