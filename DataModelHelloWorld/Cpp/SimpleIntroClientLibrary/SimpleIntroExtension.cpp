//**************************************************************************
//
// SimpleIntroExtension.cpp
//
// A simple debugger extension which adds a new example property "Hello" to the debugger's
// notion of a process.
//
// This extension is written against the Data Model C++17 Helper Library.  It is far
// preferable to write extensions against this library rather than the raw COM ABI
// due to the volume (and complexity) of glue code which is required.
//
// In order to better understand the varying ways to write a debugger extension with
// the data model, there are three versions of this extension:
//
//     C++17     : This version -- written against the Data Model C++17 Client Library
//     COM       : A version written against the raw COM ABI (only suing WRL for COM helpers)
//     JavaScript: A version written in JavaScript
//
//**************************************************************************

#include "SimpleIntro.h"
#include <DbgEng.h>

using namespace Debugger::DataModel::ClientEx;
using namespace Debugger::DataModel::ProviderEx;

Debugger::DataModel::Libraries::ExtensionProvider *g_pProvider = nullptr;
IDataModelManager *g_pManager = nullptr;
IDebugHost *g_pHost = nullptr;

namespace Debugger::DataModel::ClientEx
{
    IDataModelManager *GetManager() { return g_pManager; }
    IDebugHost *GetHost() { return g_pHost; }
}

namespace Debugger::DataModel::Libraries
{

ExtensionProvider::ExtensionProvider()
{
    m_spHelloProvider = std::make_unique<Hello::HelloProvider>();
}

} // Debugger::DataModel::Libraries

//**************************************************************************
// Standard DbgEng Extension Exports:
//

// DebugExtensionInitialize:
//
// Called to initialize the debugger extension.  For a data model extension, this acquires
// the necessary data model interfaces from the debugger and instantiates singleton instances
// of any of the extension classes which provide the functionality of the debugger extension.
//
extern "C"
HRESULT CALLBACK DebugExtensionInitialize(PULONG /*pVersion*/, PULONG /*pFlags*/)
{
    HRESULT hr = S_OK;

    try
    {
        Microsoft::WRL::ComPtr<IDebugClient> spClient;
        Microsoft::WRL::ComPtr<IHostDataModelAccess> spAccess;

        //
        // Create a client interface to the debugger and ask for the data model interfaces.  The client
        // library requires an implementation of Debugger::DataModel::ClientEx::(GetManager and ::GetHost)
        // which return these interfaces when called.
        //
        hr = DebugCreate(__uuidof(IDebugClient), (void **)&spClient);
        if (SUCCEEDED(hr))
        {
            hr = spClient.As(&spAccess);
        }

        if (SUCCEEDED(hr))
        {
            hr = spAccess->GetDataModel(&g_pManager, &g_pHost);
        }

        if (SUCCEEDED(hr))
        {
            //
            // Create the provider class which itself is a singleton and holds singleton instances of
            // all extension classes.
            //
            g_pProvider = new Debugger::DataModel::Libraries::ExtensionProvider();
        }
    }
    catch(...)
    {
        return E_FAIL;
    }

    if (FAILED(hr))
    {
        if (g_pManager != nullptr)
        {
            g_pManager->Release();
            g_pManager = nullptr;
        }

        if (g_pHost != nullptr)
        {
            g_pHost->Release();
            g_pHost = nullptr;
        }
    }

    return hr;
}

// DebugExtensionCanUnload:
//
// Called after DebugExtensionUninitialize to determine whether the debugger extension can
// be unloaded.  A return of S_OK indicates that it can.  A failure (or return of S_FALSE) indicates
// that it cannot.
//
// Extension libraries are responsible for ensuring that there are no live interfaces back into the
// extension before unloading!
//
extern "C"
HRESULT CALLBACK DebugExtensionCanUnload(void)
{
    auto objCount = Microsoft::WRL::Module<InProc>::GetModule().GetObjectCount();
    return (objCount == 0) ? S_OK : S_FALSE;
}

// DebugExtensionUninitialize:
//
// Called before unloading (and before DebugExtensionCanUnload) to prepare the debugger extension for
// unloading.  Any manipulations done during DebugExtensionInitialize should be undone and any interfaces
// released.
//
// Deleting the singleton instances of extension classes should unlink them from the data model.  There still
// may be references into the extension alive from scripts, other debugger extensions, debugger variables,
// etc...  The extension cannot return S_OK from DebugExtensionCanUnload until there are no such live
// references.
//
// If DebugExtensionCanUnload returns a "do not unload" indication, it is possible that DebugExtensionInitialize
// will be called without an interveining unload.
//
extern "C"
void CALLBACK DebugExtensionUninitialize()
{
    if (g_pProvider != nullptr)
    {
        delete g_pProvider;
        g_pProvider = nullptr;
    }

    if (g_pManager != nullptr)
    {
        g_pManager->Release();
        g_pManager = nullptr;
    }

    if (g_pHost != nullptr)
    {
        g_pHost->Release();
        g_pHost = nullptr;
    }
}

// DebugExtensionUnload:
//
// A final callback immediately before the DLL is unloaded.  This will only happen after a successful
// DebugExtensionCanUnload.
//
extern "C"
void CALLBACK DebugExtensionUnload()
{
}

