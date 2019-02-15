//**************************************************************************
//
// SimpleIntroExtension.cpp
//
// A simple debugger extension which adds a new example property "Hello" to the 
// debugger's notion of a process.
//
// This extension is written against the raw COM ABI of the Debugger Data Model using
// only WRL (the Windows Runtime Library) as a general helper for COM objects.  While
// this sample serves as an introduction to the COM API for the Debugger Data Model,
// it is far easier and more productive to write extensions against higher level frameworks.
//
// In order to better understand the varying ways to write a debugger extension with
// the data model, there are three versions of this extension:
//
//     COM       : This version -- written against the raw COM ABI (only using WRL for COM helpers)
//     C++17     : A version written against the Data Model C++17 Client Library
//     JavaScript: A version written in JavaScript
//
//**************************************************************************

#include "SimpleIntro.h"
#include <DbgEng.h>

using namespace Microsoft::WRL;

IDataModelManager *g_pManager = nullptr;
IDebugHost *g_pHost = nullptr;
HelloExtension *g_pHelloExtension = nullptr;

// GetManager():
//
// Gets our interface to the data model manager.
//
IDataModelManager *GetManager()
{
	return g_pManager;
}

// GetHost():
//
// Gets our interface to the debug host.
//
IDebugHost *GetHost()
{
	return g_pHost;
}

// InitializeExtension():
//
// Creates and adds the necessary
//
HRESULT InitializeExtension()
{
	g_pHelloExtension = new(std::nothrow) HelloExtension;
	if (g_pHelloExtension == nullptr)
	{
		return E_OUTOFMEMORY;
	}

	return g_pHelloExtension->Initialize();
}

// UninitializeExtension():
//
// 
void UninitializeExtension()
{
	if (g_pHelloExtension != nullptr)
	{
		g_pHelloExtension->Uninitialize();
		g_pHelloExtension = nullptr;
	}
}

//**************************************************************************
// Standard DbgEng Extension Exports:
//

// DebugExtensionInitialize:
//
// Called to initialize the debugger extension.  For a data model extension, this acquires
// the necessary data model interfaces from the debugger, acquires the extensibility points from the
// data model manager, and extends them using parent models or other registration mechanisms.
//
extern "C"
HRESULT CALLBACK DebugExtensionInitialize(PULONG /*pVersion*/, PULONG /*pFlags*/)
{
	HRESULT hr = S_OK;

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
		hr = InitializeExtension();
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
// If DebugExtensionCanUnload returns a "do not unload" indication, it is possible that DebugExtensionInitialize
// will be called without an interveining unload.
//
extern "C"
void CALLBACK DebugExtensionUninitialize()
{
	if (g_pHost != nullptr)
	{
		g_pHost->Release();
		g_pHost = nullptr;
	}

	if (g_pManager != nullptr)
	{
		g_pManager->Release();
		g_pManager = nullptr;
	}

	if (g_pHelloExtension != nullptr)
	{
		g_pHelloExtension->Uninitialize();
		delete g_pHelloExtension;
		g_pHelloExtension = nullptr;
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

