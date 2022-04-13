//----------------------------------------------------------------------------
//
// dllmain.cpp
//
// Implementation of DllMain().
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#include "stdafx.h"
#include "resource.h"
#include "ExdiKdSample.h"
#include "dllmain.h"

CStaticExdiSampleModule _AtlModule;

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	hInstance;
	return _AtlModule.DllMain(dwReason, lpReserved); 
}
