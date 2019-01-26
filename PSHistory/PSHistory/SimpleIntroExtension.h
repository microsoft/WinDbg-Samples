//**************************************************************************
//
// SimpleIntroExtension.h
//
// A sample C++ debugger extension which adds a new example property "Hello"
// to the debugger's notion of a process.
//
//**************************************************************************

IDataModelManager *GetManager();
IDebugHost *GetHost();

extern ComPtr<IDebugControl4> g_Control4;
extern PDEBUG_CLIENT g_DebugClient;

size_t
Contains(
    vector<wstring> &Input,
    LPCWSTR targetString,
    vector<wstring> &Output
);

size_t
Split(
    const wstring &txt,
    std::vector<wstring> &strs,
    wchar_t ch
);
