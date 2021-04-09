// dllmain.h : Declaration of module class.

class CExdiGdbSrvSampleModule : public ATL::CAtlDllModuleT< CExdiGdbSrvSampleModule >
{
public :
	DECLARE_LIBID(LIBID_ExdiGdbSrvSampleLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_EXDIGDBSRVSAMPLE, "{1FC9AD2A-EEC4-467E-AA40-951987327C81}")
};

extern class CExdiGdbSrvSampleModule _AtlModule;
