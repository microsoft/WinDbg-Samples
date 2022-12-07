//**************************************************************************
//
// PyFunctions.h
//
// General support (and implementations of) Python callable functions.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

namespace Debugger::DataModel::ScriptProvider::Python::Functions
{

// PythonFunction:
//
// An object which represents a function callable from Python in either varargs or varargs/kwargs style.  Instances
// of this class should not be directly created and should be utilized through PythonVaFunction or PythonVaKwFunction.
//
class PythonFunction :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IUnknown
        >
{
public:

    virtual ~PythonFunction();

    //*************************************************
    // Bridged Python APIs:
    // 

    virtual PyObject *InvokeVa(_In_ PyObject * /*pArgs*/) { return nullptr; }
    virtual PyObject *InvokeVaKw(_In_ PyObject * /*pArgs*/, _In_ PyObject * /*pKwArgs*/ ) { return nullptr; }

    //*************************************************
    // Internal APIs:
    //

    // GetName():
    //
    // Gets the name of the function.
    //
    const char *GetName() const
    {
        return m_methodDef.ml_name;
    }

    // AddToObject():
    //
    // Adds this function into an object.
    //
    HRESULT AddToObject(_In_ PyObject *pObject);

    // GetFunctionObject():
    //
    // Gets the function object.
    //
    PyObject *GetFunctionObject() const
    {
        assert(m_pFunction != nullptr);
        return m_pFunction;
    }

protected:

    // BaseInitializeVa:
    //
    // Dynamically creates the function object and necessary capsule for the class. 
    //
    HRESULT BaseInitializeVa(_In_ const char* pName, _In_ Marshal::PythonMarshaler *pMarshaler)
    {
        return InternalInitialize(pName, METH_VARARGS, pMarshaler);
    }

    HRESULT BaseInitializeVaKw(_In_ const char* pName, _In_ Marshal::PythonMarshaler *pMarshaler)
    {
        return InternalInitialize(pName, METH_KEYWORDS, pMarshaler);
    }

    // GetMarshaler():
    //
    // Gets the marshaler.
    //
    Marshal::PythonMarshaler *GetMarshaler() const
    {
        return m_pMarshaler;
    }

    // Destruct():
    //
    // The destructor callback from Python for this function.
    //
    void Destruct()
    {
        Release();
    }

private:

    // InternalInitialize():
    //
    // Performs the function initialization.
    //
    HRESULT InternalInitialize(_In_ const char *pName, 
                               _In_ int flags,
                               _In_ Marshal::PythonMarshaler *pMarshaler);

    // DefineMethod():
    //
    // Initializes the PyMethodDef for this function.
    //
    void DefineMethod(_In_ const char *pName, _In_ void *pFunction, _In_ int flags)
    {
        m_methodDef.ml_name = pName;
        m_methodDef.ml_meth = reinterpret_cast<PyCFunction>(pFunction);
        m_methodDef.ml_flags = flags;
        m_methodDef.ml_doc = nullptr;
    }

    // call_va():
    //
    // The varargs form of the invocation.
    //
    static PyObject *call_va(PyObject *pData, PyObject *pArgs)
    {
        PythonFunction *pFunction = reinterpret_cast<PythonFunction *>(PyCapsule_GetPointer(pData, nullptr));
        return (pFunction->InvokeVa(pArgs));
    }

    // call_vakw():
    //
    // The varargs/kwargs form of the invocation.
    //
    static PyObject *call_vakw(PyObject *pData, PyObject *pArgs, PyObject *pKwArgs)
    {
        PythonFunction *pFunction = reinterpret_cast<PythonFunction *>(PyCapsule_GetPointer(pData, nullptr));
        return (pFunction->InvokeVaKw(pArgs, pKwArgs));
    }

    // destruct():
    //
    // The capsule destructor.
    //
    static void destruct(_In_ PyObject *pData)
    {
        PythonFunction *pFunction = reinterpret_cast<PythonFunction *>(PyCapsule_GetPointer(pData, nullptr));
        return (pFunction->Destruct());
    }

    // The method definition for this function
    PyMethodDef m_methodDef;

    // The actual function object
    PyObject *m_pFunction;

    // Other data
    Marshal::PythonMarshaler *m_pMarshaler;

};

// PythonVaFunction:
//
// An object which represents a varargs function callable from Python.
//
class PythonVaFunction : public PythonFunction
{
public:

    HRESULT BaseInitialize(_In_ const char *pName, _In_ Marshal::PythonMarshaler *pMarshaler)
    {
        return BaseInitializeVa(pName, pMarshaler);
    }

};

// PythonVaKwFunction:
//
// An object which represents a varargs/kwargs function callable from Python.
//
class PythonVaKwFunction : public PythonFunction
{
public:

    HRESULT BaseInitialize(_In_ const char *pName, _In_ Marshal::PythonMarshaler *pMarshaler)
    {
        return BaseInitializeVaKw(pName, pMarshaler);
    }

};

// PythonFunctionTable:
//
// A set of functions.
//
class PythonFunctionTable :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IUnknown
        >
{
public:

    // ~PythonFunctionTable:
    //
    // Destroys a Python function table.
    //
    ~PythonFunctionTable()
    {
    }

    //*************************************************
    // Internal APIs:
    //

    // Lookup():
    //
    // Looks up a function by name in the function table hash.
    //
    PythonFunction *Lookup(_In_ PCSTR pszName)
    {
        PythonFunction *pFunc = nullptr;
        ConvertException([&](){
            auto it = m_functionMap.find(pszName);
            if (it != m_functionMap.end())
            {
                pFunc = it->second;
            }
            return S_OK;
        });
        return pFunc;
    }

    // AddToObject():
    //
    // Adds every function in the table to the given Python object.
    //
    HRESULT AddToObject(_In_ PyObject *pPyObject)
    {
        HRESULT hr = S_OK;

        for (auto&& func : m_functions)
        {
            IfFailedReturn(func->AddToObject(pPyObject));
        }

        return hr;
    }

    // RuntimeClassInitialize():
    //
    // Initializes the function table.
    //
    HRESULT RuntimeClassInitialize()
    {
        return S_OK;
    }

    template<typename TFN, typename... TArgs>
    HRESULT NewFunction(_In_ Marshal::PythonMarshaler *pMarshaler, _In_ TArgs&&... args)
    {
        HRESULT hr = S_OK;
        Microsoft::WRL::ComPtr<TFN> spFn;
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<TFN>(&spFn, pMarshaler, std::forward<TArgs>(args)...));
        IfFailedReturn(AddFunction(spFn.Get()));
        return hr;
    }

private:

    typedef std::unordered_map<std::string, PythonFunction *> FunctionMap;

    HRESULT AddFunction(_In_ PythonFunction *pPythonFunction)
    {
        HRESULT hr = S_OK;
        Microsoft::WRL::ComPtr<PythonFunction> spFn(pPythonFunction);
        IfFailedReturn(ConvertException([&](){
            m_functions.push_back(spFn);
            m_functionMap.insert(FunctionMap::value_type(spFn->GetName(), spFn.Get()));
            return hr;
        }));

        return hr;
    }

    FunctionMap m_functionMap;
    std::vector<Microsoft::WRL::ComPtr<PythonFunction>> m_functions;
    bool m_temporaryTable;
};



};
