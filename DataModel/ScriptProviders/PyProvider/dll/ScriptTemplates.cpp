//**************************************************************************
//
// ScriptTemplates.cpp
//
// Template content for Python scripts
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include "PyProvider.h"

using namespace Microsoft::WRL;
using namespace Debugger::DataModel;
using namespace Debugger::DataModel::ScriptProvider;
using namespace Debugger::DataModel::ScriptProvider::Python;

namespace Debugger::DataModel::ScriptProvider::Python
{

// APIVERSION:
//
TemplateData g_templates[] =
{
    {
        L"def initializeScript():\r\n" \
        L"    # \r\n" \
        L"    # Return an array of registration objects to modify the object model of the debugger\r\n" \
        L"    # \r\n" \
        L"    return [];\r\n",

        L"Extension Script",

        L"Use this template to help you extend objects in the debugger through the data model."

    },

    {
        L"def initializeScript():\r\n" \
        L"    return [];\r\n" \
        L"\r\n" \
        L"def invokeScript():\r\n" \
        L"    # \r\n" \
        L"    # Insert your script content here.  This method will be called whenever the script is\r\n" \
        L"    # invoked from a client.\r\n",

        L"Imperative Script",

        L"Use this template to create a script that can be invoked to run an arbitrary set of debugger commands."
    }

};

TemplateData* GetDefaultTemplateData()
{
    return &(g_templates[0]);
}

HRESULT PythonScriptTemplate::RuntimeClassInitialize(_In_ TemplateData *pTemplateData)
{
    m_pTemplateData = pTemplateData;
    return S_OK;
}

HRESULT PythonScriptTemplate::GetName(_Out_ BSTR *pTemplateName)
{
    *pTemplateName = SysAllocString(m_pTemplateData->TemplateName);
    if (*pTemplateName == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

HRESULT PythonScriptTemplate::GetDescription(_Out_ BSTR *pTemplateDescription)
{
    *pTemplateDescription = SysAllocString(m_pTemplateData->TemplateDescription);
    if (*pTemplateDescription == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

HRESULT PythonScriptTemplate::GetContent(_COM_Outptr_ IStream **ppContentStream)
{
    PythonProvider *pProvider = PythonProvider::Get();
    *ppContentStream = nullptr;

    ComPtr<IStream> spStream = SHCreateMemStream(reinterpret_cast<const BYTE *>(m_pTemplateData->TemplateContent),
                                                 static_cast<UINT>((wcslen(m_pTemplateData->TemplateContent) + 1) * sizeof(wchar_t)));
    if (spStream == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    *ppContentStream = spStream.Detach();
    return S_OK;
}

HRESULT PythonScriptTemplateEnumerator::Reset()
{
    m_pos = 0;
    return S_OK;
}

HRESULT PythonScriptTemplateEnumerator::GetNext(_COM_Outptr_ IDataModelScriptTemplate **ppTemplate)
{
    HRESULT hr = S_OK;
    *ppTemplate = nullptr;

    if (m_pos >= ARRAYSIZE(g_templates))
    {
        return E_BOUNDS;
    }

    ComPtr<PythonScriptTemplate> spTemplate;
    IfFailedReturn(MakeAndInitialize<PythonScriptTemplate>(&spTemplate, &(g_templates[m_pos])));

    ++m_pos;
    *ppTemplate = spTemplate.Detach();
    return hr;
}

} // Debugger::DataModel::ScriptProvider::Python

