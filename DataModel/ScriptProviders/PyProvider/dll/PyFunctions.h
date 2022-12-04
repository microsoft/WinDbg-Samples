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

    static PyObject *call_va(PyObject *pData, PyObject *pArgs)
    {
        PythonFunction *pFunction = reinterpret_cast<PythonFunction *>(PyCapsule_GetPointer(pData, nullptr));
        return (pFunction->InvokeVa(pArgs));
    }

    static PyObject *call_vakw(PyObject *pData, PyObject *pArgs, PyObject *pKwArgs)
    {
        PythonFunction *pFunction = reinterpret_cast<PythonFunction *>(PyCapsule_GetPointer(pData, nullptr));
        return (pFunction->InvokeVaKw(pArgs, pKwArgs));
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


};
