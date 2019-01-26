/*++

Copyright (c) 2019, Matthieu Suiche. All rights reserved.

Module Name:

    DataModel.h

Abstract:

    This module is the header for some of the Data Model functions.

Author:

    Matthieu Suiche (@msuiche) 23-Jan-2019

Revision History:


--*/

class PSHistory {
public:
    PSHistory()
    {
        // Initialize();
    }

    HRESULT
    ExecuteCommand(
        LPCWSTR lpwsCommand,
        std::vector<std::wstring> &vResults
    );

    HRESULT
    Initialize(
        VOID
    );

    HRESULT
    GetHistory(
        VOID
    );

    void
    OutHistory(
        void
    );

    void
    AddHistoryToModel(
        void
    );

    HRESULT
    GetProcessNameFromDataModel(
        wstring *outProcessName
    );

    HRESULT
    AddChildrenToParentModel(
        LPCWSTR ModelPath,
        LPCWSTR KeyName,
        vector<wstring> &vResults
    );

    void
    Uninitialize(
        VOID
    );

    ComPtr<IModelObject> m_spProcessModelObject;
    ComPtr<IModelObject> m_spHelloExtensionModelObject;

    vector<wstring> m_PowerShellHistory;
};

class MyIterableConcept :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
    IIterableConcept
    >
{
public:
    IFACEMETHOD(GetDefaultIndexDimensionality)(
        _In_ IModelObject *pObject,
        _Out_ ULONG64 *pDimensionality)
    {
        *pDimensionality = 0;

        return S_OK;
    }

    IFACEMETHOD(GetIterator)(
        _In_ IModelObject * /*pContextObject*/,
        _Out_ IModelIterator **ppIterator);

    HRESULT RuntimeClassInitialize(_In_ vector<wstring> &results)
    {
        m_results = results;

        return S_OK;
    }

private:
    friend class MyIterator;

    std::vector<std::wstring> m_results;
};

class MyIterator :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
    IModelIterator
    >
{
public:
    IFACEMETHOD(Reset)(VOID)
    {
        m_pos = 0;

        return S_OK;
    }

    STDMETHOD(GetNext)(
        _COM_Errorptr_ IModelObject **ppResult,
        _In_ ULONG64 dimensions,
        _Out_writes_opt_(dimensions) IModelObject **ppIndexers,
        _COM_Outptr_opt_result_maybenull_ IKeyStore **ppMetadata)
    {
        HRESULT hr = S_OK;
        *ppResult = nullptr;

        if (ppMetadata != nullptr)
        {
            *ppMetadata = nullptr;
        }

        for (ULONG64 i = 0; i < dimensions; ++i)
        {
            ppIndexers[i] = nullptr;
        }

        if (dimensions != 0)
        {
            return E_INVALIDARG;
        }

        std::vector<std::wstring> const& vec = m_spIterable->m_results;
        if (m_pos >= vec.size())
        {
            return E_BOUNDS;
        }

        ComPtr<IModelObject> spString;
        IfFailedReturn(CreateString(vec[m_pos].c_str(), &spString));

        ++m_pos;
        *ppResult = spString.Detach();
        return hr;
    }

    HRESULT RuntimeClassInitialize(_In_ ComPtr<MyIterableConcept> pIterable)
    {
        m_spIterable = pIterable;

        return Reset();
    }

private:
    ComPtr<MyIterableConcept> m_spIterable;
    size_t m_pos;
};

