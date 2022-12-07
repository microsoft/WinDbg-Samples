//**************************************************************************
//
// ScriptProvider.h
//
// The core script provider for Python.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

namespace Debugger::DataModel::ScriptProvider::Python
{

// PythonScriptState:
//
// Maintains the state of a single "execution" of a script.  If script content changes
// or is updated, there is new "state" and a new "script context" but not a new "script".
//
// This provides that unit of differentiation.
//
class PythonScriptState :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IUnknown
        >
{
public:

    // ~PythonScriptState():
    //
    // Destructor which ensures full uninitialization.
    //
    virtual ~PythonScriptState() { }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the script state.  This creates a new script context as far as Python is concerned.
    //
    HRESULT RuntimeClassInitialize(_In_ PythonScript *pScript,
                                   _In_ std::vector<BYTE> const& scriptContent,
                                   _In_opt_ PCWSTR scriptFullPathName);

    // Execute():
    //
    // Executes the content of the script and bridges it to a detached namespace object.
    //
    HRESULT Execute();

    // InitializeScript():
    //
    // Called to initialize the script after it has been executed.  This invokes the Python InitializeScript
    // method (if it is present) and sets up the bridging which the returned object indicates.
    //
    HRESULT InitializeScript();

    // UninitializeScript():
    //
    // Called to uninitialize the script after it has been executed.  This invokes the Python Uninitialize
    // method (if it is present) and then undoes any bridging which was set up by the Python InitializeScript
    // method (if it was present at init time).
    //
    HRESULT UninitializeScript() { return E_NOTIMPL; }

    // FinalizeInitialization():
    //
    // Finalizes the initialization of the state of the script.  This bridges things into the script context which
    // should not be available during the initialization process.
    //
    HRESULT FinalizeInitialization();

    // InvokeMainFunction():
    //
    // Invokes the main function of the script.  Returns E_NOTIMPL if the script has no main function.
    //
    HRESULT InvokeMainFunction();

    // HasMainFunction():
    //
    // Indicates whether the script has a main 'invokeScript' function.  This may only be validly called after a successful
    // execute.
    //
    bool HasMainFunction() const
    {
        return m_pythonMainFunction != nullptr;
    }

    // GetScript():
    //
    // Returns the owning script.
    //
    PythonScript *GetScript() const
    {
        return m_spScript.Get();
    }

    // GetPythonLibrary():
    //
    // Returns the Python "support library".
    //
    Library::PythonLibrary *GetPythonLibrary() const
    {
        return m_spPythonLibrary.get();
    }

    // GetModule():
    //
    // Gets the Python module (script context) which presently represents this script.
    //
    PyObject *GetModule() const
    {
        return m_pModule;
    }

    // GetContent():
    //
    // Gets the script content of this script state.
    //
    std::vector<BYTE>& GetContent()
    {
        return m_scriptContent;
    }

    // EnterScript():
    //
    // Enters the script context and returns an RAII object by value whose DTOR will restore the appropriate
    // script context.
    //
    ScriptSwitcher EnterScript();

    // GetActiveScriptState():
    //
    // Gets the currently active script state.
    //
    static PythonScriptState *GetActiveScriptState();

private:

    //
    // The present state of *THIS* script state.
    //
    enum class ScriptStateState
    {
        //*************************************************
        // One Time States:
        //
        // The state passes through this machinery only once -- regardless of how many times
        // it is unlinked or flows through an uninit/init cycle due to an errant re-execution of
        // changed content.
        //

        // This is a brand new script state which has been created and minimally initialized
        Created = 0,

        // The root code of the script has been called and is within the Python script context.
        Executed,

        //*************************************************
        // Multiple Time States:

        // The InitializeScript method has been called and executed.
        UserInitialized,

        // Bridging to the namespace has *SUCCEEDED* and the script state is considered active.
        // Once the state reaches this point, the provider can get rid of the previously
        // executed state and Python context.
        Active,

        // This is no longer the active state.  It is pending delete (perhaps due to remaining
        // live objects)
        Inactive
    };

    ScriptStateState m_state;

    // The content of this script (converted to UTF-8 per Python)
    std::vector<BYTE> m_scriptContent;

    // The library of Python routines which we must call to support the script.  All of these are specific
    // to the script context.
    std::unique_ptr<Library::PythonLibrary> m_spPythonLibrary;

    // The script's main function.
    PinnedReference m_pythonMainFunction;

    // The model object which represents the "namespace" of the script.  This is where all script
    // content is bridged.  Note that this is *NOT* the same as the namespace object returned
    // from the script host.  The two are linked together to provide rollback capability
    // for subsequent Populate/Execute cycles.
    Debugger::DataModel::ClientEx::Object m_namespaceObject;

    // Pointer to the owning script.
    Microsoft::WRL::ComPtr<PythonScript> m_spScript;

    // Python state
    PyObject *m_pModule;
};

// PythonScript:
//
// Represents a single script which the provider manages.  Such script may be in one of various states
// (empty, populated, executed & linked to the namespace, etc...)
//
class PythonScript :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        Microsoft::WRL::ChainInterfaces<IDataModelScript2, IDataModelScript>
#if 0
        Microsoft::WRL::ChainInterfaces<IDataModelScriptDebug2, IDataModelScriptDebug>
#endif // 0
        >
{
public:

    //*************************************************
    // Public Typedefs:

    //*************************************************
    // IDataModelScript
    //

    // GetName():
    //
    // Gets the name of the script.
    //
    IFACEMETHOD(GetName)(_Out_ BSTR* pbstrScriptName);

    // Rename():
    //
    // Renames the script.
    //
    IFACEMETHOD(Rename)(_In_ PCWSTR pwszScriptName);

    // Populate():
    //
    // Called by the host (or client) in order to change/synchronize the "content" of the script.
    // This will update our internal cached copy of the script content but will do nothing to execute
    // the contents of the script.
    //
    IFACEMETHOD(Populate)(_In_ IStream *pContentStream);

    // Execute():
    //
    // Executes the content of the script and bridges its namespace with that of the host via the data
    // model.
    //
    IFACEMETHOD(Execute)(_In_ IDataModelScriptClient *pScriptClient);

    // Unlink():
    //
    // Undoes the Execute() operation and unlinks the namespace of the script from that of the host.
    //
    IFACEMETHOD(Unlink)() { return E_NOTIMPL; }

    // IsInvocable():
    //
    // Returns whether or not the script is invocable (e.g.: it has some semblance of a main function as defined by
    // its provider).  This method may only be called after a successful Execute.
    //
    // If this method returns true in its argument, InvokeMain() may be called.
    //
    IFACEMETHOD(IsInvocable)(_Out_ bool *pIsInvocable);

    // InvokeMain():
    //
    // If the script has a main function which is intended to execute from a UI invocation, the provider can indicate
    // such in metadata and the call to this method will "invoke" the script.  Note that this is distinct from *Execute*
    // which runs all root code and bridges the script to the namespace of the underlying host.
    //
    // This method may fail with E_NOTIMPL if the script does not contain a "named main function" or the provider does
    // not define a "main" method.
    //
    IFACEMETHOD(InvokeMain)(_In_ IDataModelScriptClient *pScriptClient) { return E_NOTIMPL; }

    //*************************************************
    // IDataModelScript2
    //

    // GetScriptFullFilePathName():
    //
    // Gets the script full file path name.
    //
    IFACEMETHOD(GetScriptFullFilePathName)(_Out_ BSTR *pbstrScriptFullPathName);

    // SetScriptFullFilePathName():
    //
    // Sets the script full file path name
    //
    IFACEMETHOD(SetScriptFullFilePathName)(_In_ PCWSTR scriptFullPathName);

    //*************************************************
    // Internal APIs:
    //

    // ~PythonScript():
    //
    // Destroy the script.
    //
    ~PythonScript() { }

    // RuntimeClassInitialize():
    //
    // Create a new script.
    //
    HRESULT RuntimeClassInitialize(_In_ PythonProvider *pScriptProvider);

    // InternalExecute():
    //
    // Performs the action of Execute(...) in the right script context.  In the case of failure, it
    // will leave the old namespace bridge set up to the old script context.  The caller is responsible
    // for an uninit/init cycle of old script around an InternalExecute.
    //
    HRESULT InternalExecute();

    // ReportError():
    //
    // Reports an error to the error sink (if one is attached)
    //
    HRESULT ReportError(_In_ ErrorClass errClass,
                        _In_ HRESULT hrError,
                        _In_ ULONG line,
                        _In_ ULONG pos,
                        _In_ ULONG rscId,
                        ...)
    {
        va_list va;
        va_start(va, rscId);
        return InternalReportError(errClass, hrError, line, pos, rscId, va);
    }

    HRESULT ReportError(_In_ ErrorClass errClass,
                        _In_ HRESULT hrError,
                        _In_ ULONG rscId,
                        ...)
    {
        va_list va;
        va_start(va, rscId);
        return InternalReportError(errClass, hrError, 0, 0, rscId, va);
    }

    // ReportExceptioonOrError():
    //
    // Reports an error th the error sink (if one is attached).  If there is an exception on the Python
    // interpreter, the exception details will be used for the error.  If there is not, the message passed
    // will be used in lieu of more specific exception information.
    //
    HRESULT ReportExceptionOrError(_In_ HRESULT hr,
                                   _Out_ HRESULT *pConvertedResult,
                                   _In_ ErrorClass errClass,
                                   _In_ ULONG rscId,
                                   ...);

    // GetProvider():
    //
    // Returns the back pointer to the Python provider.
    //
    PythonProvider *GetProvider() const
    {
        return m_spProvider.Get();
    }

    // GetActiveState():
    //
    // Gets the currently active script state.
    //
    PythonScriptState *GetActiveState() const
    {
        return m_spActiveState.Get();
    }

    // GetPythonLibrary():
    //
    // Returns the Python "support library".
    //
    Library::PythonLibrary *GetPythonLibrary() const
    {
        Library::PythonLibrary *pPythonLibrary = nullptr;
        auto pActiveState = GetActiveState();
        if (pActiveState != nullptr)
        {
            pPythonLibrary = pActiveState->GetPythonLibrary();
        }
        return pPythonLibrary;
    }

    // GetModule():
    //
    // Gets the Python module which presently represents this script.
    //
    PyObject *GetModule() const
    {
        return GetActiveState()->GetModule();
    }

    // GetScriptHostContext():
    //
    // Gets the host context for the script.
    //
    IDataModelScriptHostContext *GetScriptHostContext() const
    {
        return m_spScriptHostContext.Get();
    }

    // GetHostNamespace():
    //
    // Gets the host namespace object for this script.  This is "per script" and not
    // "per script state".  Each script state has its own namespace object which is 
    // linked to this as a parent in order to facilitate easy hook up and tear down
    // on change of content.
    //
    Debugger::DataModel::ClientEx::Object& GetHostNamespace()
    {
        return m_hostNamespace;
    }

    // GetMarshaler():
    //
    // Returns the marshaler for use with this script.
    //
    Marshal::PythonMarshaler *GetMarshaler() const;

private:

    // ScriptState:
    //
    // Tracks the internal state of the script
    //
    enum class ScriptState
    {
        // The starting state of the script
        Unpopulated,
        
        // Content has been populated.  The script has never been executed.
        Populated,

        // The script is executed and live.
        Executed,

        // The script is executed and live but updated content has been populated and not executed.
        Repopulated,

        // The script has been unlinked from execution.  
        Unlinked
    };

    // InternalReportError():
    //
    // Internal error reporting.
    //
    HRESULT InternalReportError(_In_ ErrorClass errClass,
                                _In_ HRESULT hrError,
                                _In_ ULONG line,
                                _In_ ULONG pos,
                                _In_ ULONG rscId,
                                _In_ va_list va);

    HRESULT InternalReportError(_In_ ErrorClass errClass,
                                _In_ HRESULT hrError,
                                _In_ ULONG line,
                                _In_ ULONG pos,
                                _In_ PCSTR pszMsg,
                                _In_ va_list va);

    ScriptState m_state;

    // The name of the script (as given by the client)
    std::wstring m_scriptName;

    // The full path name of the script (as given by the client)
    std::wstring m_scriptFullPathName;

    // The presently active state of the script.
    Microsoft::WRL::ComPtr<PythonScriptState> m_spActiveState;

    // The content of the script as last populated by the client
    std::vector<BYTE> m_scriptContent;

    // The content of the script as last executed by the client
    std::vector<BYTE> m_executedContent;

    Microsoft::WRL::ComPtr<PythonProvider> m_spProvider;

    // Attributes of the host context
    Microsoft::WRL::ComPtr<IDataModelScriptHostContext> m_spScriptHostContext;
    Debugger::DataModel::ClientEx::Object m_hostNamespace;

    // The client to which we are reporting.  This is only valid during the context of an *EXECUTE* call at
    // present.  It is stored and removed.
    Microsoft::WRL::ComPtr<IDataModelScriptClient> m_spReportingClient;
};


// PythonProvider:
//
// The script provider object which hosts the Python runtime and bridges its world into the world of
// the data model.  This is the single canonical provider of Python to the debugger.  It registers
// against the ".py" extension as the means of loading such scripts.
//
class PythonProvider :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDataModelScriptProvider
        >
{
public:

    //
    // The state of the canonical provider within the extension DLL.
    //
    enum class ProviderState
    {
        Registered,
        PendingUnload,
        Uninitialized
    };

    //*************************************************
    // IDataModelScriptProvider:
    //

    // GetName():
    //
    // Gets the name of this script provider.  This will be presented in various forms of client UI.
    //
    IFACEMETHOD(GetName)(_Out_ BSTR *pbstrName);

    // GetExtension():
    //
    // Gets the extension which the script provider handles.  Any script file ending in this extension
    // will be directed towards the Python provider to load and handle.
    //
    IFACEMETHOD(GetExtension)(_Out_ BSTR *pbstrExtension);

    // CreateScript():
    //
    // Creates a new empty script object and returns it to the caller.
    //
    IFACEMETHOD(CreateScript)(_COM_Outptr_ IDataModelScript **ppScript);

    // GetDefaultTemplateContent():
    //
    // Gets the default template content for any script content.  
    //
    IFACEMETHOD(GetDefaultTemplateContent)(_COM_Outptr_ IDataModelScriptTemplate **ppTemplateContent);

    // EnumerateTemplates():
    //
    // Enumerates all available template content.
    //
    IFACEMETHOD(EnumerateTemplates)(_COM_Outptr_ IDataModelScriptTemplateEnumerator **ppTemplateEnumerator);

    //*************************************************
    // Internal APIs:
    //

    virtual ~PythonProvider() { }

    // RuntimeClassInitialize():
    //
    // Initializes the script provider.
    //
    HRESULT RuntimeClassInitialize(_In_ IDataModelManager *pManager,
                                   _In_ IDataModelScriptManager *pScriptManager,
                                   _In_ IDebugHostScriptHost *pScriptHost);

    // GetStringResource():
    //
    // Copies a string resource from the resource module into a newly allocated buffer.
    //
    HRESULT GetStringResource(_In_ ULONG rscId,
                              _Out_ std::unique_ptr<char[]> *pStringResource);

    // GetDataModelManager():
    //
    // Gets our back pointer to the data model manager.
    //
    IDataModelManager *GetDataModelManager() const
    {
        return m_spManager.Get();
    }

    // GetHost():
    //
    // Gets the host interface.
    //
    IDebugHost *GetHost() const
    {
        return m_spHost.Get();
    }

    // GetHostStatus():
    //
    // Gets the host status interface.
    //
    IDebugHostStatus *GetHostStatus() const
    {
        return m_spHostStatus.Get();
    }

    // GetHostSymbols():
    //
    // Gets the host symbols interface.
    //
    IDebugHostSymbols *GetHostSymbols() const
    {
        return m_spHostSymbols.Get();
    }

    // GetHostEvaluator():
    //
    // Gets the host evaluator interface.
    //
    IDebugHostEvaluator *GetHostEvaluator() const
    {
        return m_spHostEvaluator.Get();
    }

    // GetHostMemory():
    //
    // Gets the host memory interface.
    //
    IDebugHostMemory *GetHostMemory() const
    {
        return m_spHostMemory.Get();
    }

    // GetHostExtensibility():
    //
    // Gets the host extensibility interface.  As this is an optional interface, it may return
    // nullptr.
    //
    IDebugHostExtensibility *GetHostExtensibility() const
    {
        return m_spHostExtensibility.Get();
    }

    // GetScriptManager():
    //
    // Gets our back pointer to the script manager.
    //
    IDataModelScriptManager *GetScriptManager() const
    {
        return m_spScriptManager.Get();
    }

    // GetScriptHost():
    //
    // Gets our back pointer to the script host.
    //
    IDebugHostScriptHost *GetScriptHost() const
    {
        return m_spScriptHost.Get();
    }

    // GetMarshaler():
    //
    // Gets the marshaler which can marshal objects between Python and the data model.
    //
    Marshal::PythonMarshaler *GetMarshaler() const
    {
        return m_spMarshaler.get();
    }

    // GetState():
    //
    // Gets the current state of the singleton provider.
    //
    static ProviderState GetState()
    {
        return s_State;
    }


    // Get():
    //
    // Gets the currently registered canonical script provider for Python.  If the provider is pending unload,
    // this will still return the provider which is pending unload.  If this needs to be explicitly checked, the
    // caller can explicitly check the state after a non nullptr returning call to Get().  There are various
    // cases which might utilize this -- uninitialization of a script, having the script call one of a set of
    // library support methods in host.* (or other host supplied objects)
    //
    static PythonProvider* Get()
    {
        auto state = GetState();
        if (state == ProviderState::Registered || state == ProviderState::PendingUnload)
        {
            return UnsafeGet();
        }

        return nullptr;
    }

    // UnsafeGet():
    //
    // Returns the provider.  This provider may *NOT* be the canonical registered provider.
    //
    static PythonProvider* UnsafeGet()
    {
        return s_pScriptProvider;
    }

    // FinishInitialization():
    //
    // Marks this provider as the canonical provider and stashes the global reference to it.
    //
    void FinishInitialization();

    // Unregister():
    //
    // Unregisters the script provider as the canonical provider of scripting capabilities.
    //
    HRESULT Unregister() { return E_FAIL; }

private:

    // Reference to the resource module
    HMODULE m_resourceModule;

    //
    // Strong back ref to the provider.  This creates a cyclic reference link which must be broken
    // via explicit unregistration or an attempt to unload this extension.
    //
    Microsoft::WRL::ComPtr<IDataModelManager> m_spManager;
    Microsoft::WRL::ComPtr<IDataModelScriptManager> m_spScriptManager;

    // The script host.
    Microsoft::WRL::ComPtr<IDebugHostScriptHost> m_spScriptHost;
    Microsoft::WRL::ComPtr<IDebugHost> m_spHost;
    Microsoft::WRL::ComPtr<IDebugHostSymbols> m_spHostSymbols;
    Microsoft::WRL::ComPtr<IDebugHostEvaluator> m_spHostEvaluator;
    Microsoft::WRL::ComPtr<IDebugHostMemory> m_spHostMemory;
    Microsoft::WRL::ComPtr<IDebugHostStatus> m_spHostStatus;
    Microsoft::WRL::ComPtr<IDebugHostExtensibility> m_spHostExtensibility;

    // The marshaler objects
    std::unique_ptr<Marshal::PythonMarshaler> m_spMarshaler;

    static ProviderState s_State;
    static PythonProvider *s_pScriptProvider;

};

} // Debugger::DataModel::ScriptProvider::Python
