//----------------------------------------------------------------------------
//
// dllmain.h
//
// Declaration of the module class.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

class CStaticExdiSampleModule : public ATL::CAtlDllModuleT< CStaticExdiSampleModule >
{
public :
	DECLARE_LIBID(LIBID_ExdiKdSampleLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_STATICEXDISAMPLE, "{B1C91B48-34B6-406F-9481-FFB6A16D4C4F}")
};

extern class CStaticExdiSampleModule _AtlModule;
