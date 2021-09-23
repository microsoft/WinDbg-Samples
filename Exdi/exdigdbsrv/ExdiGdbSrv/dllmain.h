// dllmain.h : Declaration of module class.

class CExdiGdbSrvModule : public ATL::CAtlDllModuleT< CExdiGdbSrvModule >
{
public :
	DECLARE_LIBID(LIBID_ExdiGdbSrvLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_EXDIGDBSRV, "{1FC9AD2A-EEC4-467E-AA40-951987327C81}")
};

extern class CExdiGdbSrvModule _AtlModule;
