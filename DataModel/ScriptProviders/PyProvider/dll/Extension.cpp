//**************************************************************************
//
// Extension.cpp
//
// Core extension loading for the Python script provider.
//
// Note: This is **THE ONLY FILE** in the extension which can touch any legacy
//       IDebug* interfaces.  The script provider is intended to be portable
//       between data model hosts at some point in the future.  Minimizing
//       dependencies with the legacy DbgEng IDebug* interfaces is imperative
//       for the future direction of this component.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "PyProvider.h"

#define KDEXT_64BIT
#include <dbgeng.h>

using namespace Microsoft::WRL;
using namespace Debugger::DataModel;
using namespace Debugger::DataModel::ScriptProvider;
using namespace Debugger::DataModel::ScriptProvider::Python;

//*************************************************
// Global State
//
// This is global state required to be a data model provider extension using the
// DbgModelClientEx.h library.
//

IDataModelManager *g_pManager = nullptr;
IDebugHost *g_pHost = nullptr;

namespace Debugger::DataModel::ClientEx
{
    IDataModelManager *GetManager() { return g_pManager; }
    IDebugHost *GetHost() { return g_pHost; }
}
  
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
HMODULE hPython = nullptr;

//*************************************************
// Core Extension Initialization:
//

HRESULT LoadPython()
{
    HRESULT hr = S_OK;
#if 0
    wchar_t szFolderPath[MAX_PATH];

    if (!GetModuleFileName(HINST_THISCOMPONENT, szFolderPath, ARRAYSIZE(szFolderPath)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    
    size_t cchPath = wcsnlen_s(szFolderPath, ARRAYSIZE(szFolderPath));
    for (wchar_t *pszCur = szFolderPath + cchPath - 1; pszCur > szFolderPath; pszCur--)
    {
        if (*pszCur == L'\\')
        {
            // Found the end of the containing folder name.
            *pszCur = L'\0';
            break;
        }
    }

    wcscat_s(szFolderPath, L"\\Python\\Python311.dll");

    hPython = LoadLibraryEx(szFolderPath, NULL, 0);
    if (hPython == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
#endif // 0

    return hr;
}

HRESULT InitializeProvider()
{
    HRESULT hr = S_OK;

    ComPtr<PythonProvider> spScriptProvider;
    ComPtr<IDataModelScriptManager> spScriptManager;
    ComPtr<IDebugHostScriptHost> spScriptHost;

    if (hPython == nullptr)
    {
        IfFailedReturn(LoadPython());
    }

    if (PythonProvider::GetState() == PythonProvider::ProviderState::Uninitialized)
    {
        //
        // Get access to the data model manager, the host, and the script manager and register
        // this extension as the canonical provider of Python script services.
        //
        ComPtr<IDebugClient> spClient;
        IfFailedReturn(DebugCreate(__uuidof(IDebugClient), (void **)&spClient));

        ComPtr<IDebugControl4> spDebugControl;
        IfFailedReturn(spClient.As(&spDebugControl));

        ComPtr<IHostDataModelAccess> spAccess;
        IfFailedReturn(spClient.As(&spAccess));

        IfFailedReturn(spAccess->GetDataModel(&g_pManager, &g_pHost));

        IfFailedReturn(g_pManager->QueryInterface(IID_PPV_ARGS(&spScriptManager)));
        IfFailedReturn(g_pHost->QueryInterface(IID_PPV_ARGS(&spScriptHost)));

        IfFailedReturn(MakeAndInitialize<PythonProvider>(&spScriptProvider, 
                                                         g_pManager, 
                                                         spScriptManager.Get(),
                                                         spScriptHost.Get()));
    }
    else
    {
        //
        // The only way we should ever get here is when a pending unload was canceled by reloading the
        // extension.  Instead of rebuilding everything, we simply resurrect the old provider.  This is 
        // safe for two reasons:
        //
        // 1) We are guaranteed by the definition of the model that there aren't objects being deleted out
        //    from underneath us (on another thread) during an attempt to load/unload.
        //
        // 2) If there's a live object that was keeping the DLL from unloading, there's a chain of reference
        //    back to the script provider by the design of this component!
        //
        spScriptProvider = PythonProvider::UnsafeGet();
        spScriptManager = spScriptProvider->GetScriptManager();
        spScriptHost = spScriptProvider->GetScriptHost();
    }

    IfFailedReturn(spScriptManager->RegisterScriptProvider(spScriptProvider.Get()));
    spScriptProvider->FinishInitialization();

    return S_OK;
}

void UninitializeProvider()
{
    PythonProvider *pProvider = PythonProvider::Get();
    if (pProvider != nullptr)
    {
        HRESULT const hr = pProvider->Unregister();
    }
}

extern "C"
HRESULT CALLBACK DebugExtensionInitialize(PULONG /*pVersion*/, PULONG /*pFlags*/)
{
    HRESULT hr = InitializeProvider();
    if (FAILED(hr))
    {
        UninitializeProvider();
    }
    return hr;
}

extern "C"
HRESULT CALLBACK DebugExtensionCanUnload(void)
{
    //
    // We can successfully unload if there are *NO OBJECTS* left.  When we uninitialize, we will unlink the
    // provider and release our global reference.  If there are any objects left, they will reference scripts
    // which will in turn reference the provider and the entire chain will stay around.
    //
    // Only if there are zero objects left can we successfully unload.
    //
    auto objCount = Microsoft::WRL::Module<InProc>::GetModule().GetObjectCount();
    return (objCount == 0) ? S_OK : S_FALSE;
}

#if 0
extern ULONG64 g_cacheStubObjectCount;
#endif // 0

extern "C"
BOOL CALLBACK TestCanUnloadMinusCacheStubs(void)
{
    //
    // Indicates whether we can unload or can unload EXCEPTING the cache stub count.
    //
    auto objCount = Microsoft::WRL::Module<InProc>::GetModule().GetObjectCount();
    if (objCount == 0 /* || objCount - g_cacheStubObjectCount == 0 */)
    {
        return TRUE;
    }

    return FALSE;
}

extern "C"
void CALLBACK DebugExtensionUninitialize()
{
    UninitializeProvider();
}

extern "C"
void CALLBACK DebugExtensionUnload()
{
    if (hPython != nullptr)
    {
        FreeLibrary(hPython);
        hPython = nullptr;
    }
}

extern "C"
BOOL WINAPI DllMain(HANDLE /* Instance */, ULONG Reason, PVOID /* Reserved*/)
{
    switch (Reason)
    {
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}

