//**************************************************************************
//
// ScriptTemplates.h
//
// Template content for Python scripts
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

namespace Debugger::DataModel::ScriptProvider::Python
{

struct TemplateData
{
    PCWSTR TemplateContent;
    PCWSTR TemplateName;
    PCWSTR TemplateDescription;
};

// GetDefaultTemplateData():
//
// Returns the data for the default template. 
//
TemplateData* GetDefaultTemplateData();

//
// PythonScriptTemplate:
//
// Represents a single set of template content for a script.
//
class PythonScriptTemplate :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDataModelScriptTemplate
        >
{
public:

    //*************************************************
    // IDataModelScriptTemplate:
    //

    // GetName():
    //
    // Returns the name of the template (or fails with E_NOTIMPL if the template has no name).
    //
    IFACEMETHOD(GetName)(_Out_ BSTR *pTemplateName);

    // GetDescription():
    //
    // Returns the description of the template (or fails with E_NOTIMPL if the template has no description).
    //
    IFACEMETHOD(GetDescription)(_Out_ BSTR *pTemplateDescription);

    // GetContent():
    //
    // Gets the content of the template as a stream.
    //
    IFACEMETHOD(GetContent)(_COM_Outptr_ IStream **ppContentStream);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the template.
    //
    HRESULT RuntimeClassInitialize(_In_ TemplateData *pTemplateData);
    
private:

    TemplateData *m_pTemplateData;
};

//
// PythonScriptTemplateEnumerator:
//
// An enumerator of script templates.
//
class PythonScriptTemplateEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDataModelScriptTemplateEnumerator
        >
{
public:

    //*************************************************
    // IDataModelScriptTemplateEnumerator:
    //

    // Reset():
    //
    // Resets the enumerator.
    //
    IFACEMETHOD(Reset)();

    // GetNext():
    //
    // Gets the next script template which is being enumerated and moves the enumerator
    // to the next position.  E_BOUNDS will be returned at the end of enumeration.
    //
    IFACEMETHOD(GetNext)(_COM_Outptr_ IDataModelScriptTemplate **ppTemplate);

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the template enumerator.
    //
    HRESULT RuntimeClassInitialize()
    {
        m_pos = 0;
        return S_OK;
    }

private:

    size_t m_pos;

};

} // Debugger::DataModel::ScriptProvider::Python

