/*++

Copyright (c) 2019, Matthieu Suiche. All rights reserved.

Module Name:

    PSHistory.cpp

Abstract:

    This module is a wrapper for some of the Data Model functions.

    More information: https://gist.github.com/msuiche/2324aa8147c483a7a3e7d1b2d23ee407#file-getpowershellinfo-ps1-L23

Author:

    Matthieu Suiche (@msuiche) 23-Jan-2019

Revision History:


--*/

#include "SimpleIntro.h"


size_t Split(const wstring &txt, std::vector<wstring> &strs, wchar_t ch)
{
    size_t pos = txt.find(ch);
    size_t initialPos = 0;
    strs.clear();

    while (pos != std::wstring::npos)
    {
        if (txt[pos + 1] != ch)
        {
            strs.push_back(txt.substr(initialPos, pos - initialPos));
        }
        initialPos = pos + 1;

        pos = txt.find(ch, initialPos);
    }

    strs.push_back(txt.substr(initialPos, min(pos, txt.size()) - initialPos + 1));

    return strs.size();
}

size_t
Contains(
    vector<wstring> &Input,
    LPCWSTR targetString,
    vector<wstring> &Output
)
{
    size_t occurences = 0;

    for (wstring result : Input)
    {
        if (result.find(targetString) != wstring::npos)
        {
            Output.push_back(result);
            occurences += 1;
        }
    }

    return occurences;
}

HRESULT
PSHistory::Initialize(
    VOID
)
{
    HRESULT hr = S_OK;

    IfFailedReturn(GetManager()->AcquireNamedModel(L"Debugger.Models.Process", &m_spProcessModelObject));

    return hr;
}

HRESULT
PSHistory::ExecuteCommand(
    LPCWSTR lpwsCommand,
    vector<wstring> &vResults
)
{
    HRESULT hr = S_OK;

    vResults.clear();

    //
    // Get the root namespace and then walk it down one key at a time: .Debugger.Utility.Control.ExecuteCommand
    //
    ComPtr<IModelObject> spRootNamespace;
    IfFailedReturn(GetManager()->GetRootNamespace(&spRootNamespace));

    ComPtr<IModelObject> spDebugger;
    IfFailedReturn(spRootNamespace->GetKeyValue(L"Debugger", &spDebugger, nullptr));

    ComPtr<IModelObject> spUtility;
    IfFailedReturn(spDebugger->GetKeyValue(L"Utility", &spUtility, nullptr));

    ComPtr<IModelObject> spControl;
    IfFailedReturn(spUtility->GetKeyValue(L"Control", &spControl, nullptr));

    ComPtr<IModelObject> spExecuteCommand;
    IfFailedReturn(spControl->GetKeyValue(L"ExecuteCommand", &spExecuteCommand, nullptr));

    VARIANT vExecuteCommand;
    IfFailedReturn(spExecuteCommand->GetIntrinsicValue(&vExecuteCommand));

    ComPtr<IModelMethod> spExecuteCommandMethod;
    IfFailedReturn(vExecuteCommand.punkVal->QueryInterface(IID_PPV_ARGS(&spExecuteCommandMethod)));

    ComPtr<IModelObject> spCommandString;
    IfFailedReturn(CreateString(lpwsCommand, &spCommandString));

    IModelObject *pArg1 = spCommandString.Get();
    ComPtr<IModelObject> spCommandResult;
    IfFailedReturn(spExecuteCommandMethod->Call(spControl.Get(), 1, &pArg1, &spCommandResult, nullptr));

    ComPtr<IIterableConcept> spIterable;
    IfFailedReturn(spCommandResult->GetConcept(__uuidof(IIterableConcept), &spIterable, nullptr));

    ComPtr<IModelIterator> spIterator;
    IfFailedReturn(spIterable->GetIterator(spCommandResult.Get(), &spIterator));

    for (;;)
    {
        ComPtr<IModelObject> spItem;
        hr = spIterator->GetNext(&spItem, 0, nullptr, nullptr);

        if (hr == E_BOUNDS)
        {
            hr = S_OK;

            break;
        }
        else if (FAILED(hr))
        {
            return hr;
        }

        ComPtr<IStringDisplayableConcept> spStringConversion;
        if (SUCCEEDED(spItem->GetConcept(__uuidof(IStringDisplayableConcept), &spStringConversion, nullptr)))
        {
            BSTR bstrDisplayString;
            if (SUCCEEDED(spStringConversion->ToDisplayString(spItem.Get(), nullptr, &bstrDisplayString)))
            {
                wstring entry(bstrDisplayString);
                vResults.push_back(entry);

                SysFreeString(bstrDisplayString);
            }
        }
    }

    return hr;
}

HRESULT
PSHistory::GetProcessNameFromDataModel(
    wstring *outProcessName
)
{
    HRESULT hr = S_OK;

    ComPtr<IModelObject> spObject;

    wstring processName;

    IfFailedReturn(m_spProcessModelObject->GetKeyValue(L"Name", &spObject, nullptr)); // returns 8000000b
    VARIANT vtVal;
    hr = spObject->GetIntrinsicValueAs(VT_BSTR, &vtVal);

    if (SUCCEEDED(hr))
    {
        processName = vtVal.bstrVal;
        *outProcessName = processName;
    }

    return hr;
}

HRESULT
PSHistory::AddChildrenToParentModel(
    LPCWSTR ModelPath,
    LPCWSTR KeyName,
    vector<wstring> &results
)
{
    HRESULT hr = S_OK;

    ComPtr<HelloExtensionModel> spHelloExtensionModel;
    IfFailedReturn(MakeAndInitialize<HelloExtensionModel>(&spHelloExtensionModel));

    IfFailedReturn(GetManager()->CreateDataModelObject(spHelloExtensionModel.Get(), &m_spHelloExtensionModelObject));

    ComPtr<MyIterableConcept> spMyIterableConcept;
    IfFailedReturn(MakeAndInitialize<MyIterableConcept>(&spMyIterableConcept, results));

    ComPtr<MyIterator> spMyIterator;
    IfFailedReturn(MakeAndInitialize<MyIterator>(&spMyIterator, spMyIterableConcept));

    ComPtr<IModelObject> m_spKeyNameType;
    IfFailedReturn(GetManager()->CreateDataModelObject(spHelloExtensionModel.Get(), &m_spKeyNameType));

    ComPtr<IModelObject> spResult;
    ComPtr<IModelObject> spIndexers;

    int i = 0;
    while (spMyIterator->GetNext(&spResult, 0, &spIndexers, nullptr) == S_OK) {
        WCHAR Idx[MAX_PATH] = { 0 };
        wsprintf(Idx, L"[0x%x]", i);
        IfFailedReturn(m_spKeyNameType->SetKey(Idx, spResult.Get(), nullptr));
        i++;
    }

    IfFailedReturn(m_spHelloExtensionModelObject->SetKey(KeyName, m_spKeyNameType.Get(), nullptr));

    IfFailedReturn(m_spProcessModelObject->AddParentModel(m_spHelloExtensionModelObject.Get(), nullptr, false));

    return hr;
}

HRESULT
PSHistory::GetHistory(
    VOID
)
{
    HRESULT hr = S_OK;
    vector<wstring> HistoryObjects;
    vector<wstring> cmdLines;

    vector<wstring> tmp;

    wstring processName;

    // TODO: Compare processName with "powershell.exe"
    hr = GetProcessNameFromDataModel(&processName);
    if (SUCCEEDED(hr))
    {
        g_Control4->Output(DEBUG_OUTPUT_NORMAL, "process name = %S\n", processName.c_str());
    }


    if (m_PowerShellHistory.size()) return S_OK;

    IfFailedReturn(ExecuteCommand(L".loadby sos clr;.symfix;.reload", HistoryObjects));

    //
    // We need to filter out the exceptions. For some reasons due to a bug to sos.dll, !DumpHeap has to be executed twice.
    //
    hr = ExecuteCommand(L"!DumpHeap -Type HistoryInfo -short", HistoryObjects);
    if (Contains(HistoryObjects, L"Exception", tmp))
    {
        g_Control4->Output(DEBUG_OUTPUT_NORMAL, "Hopefully MSFT will fix this null pointer bug in sos!IsMiniDumpFileNODAC()\n");
        IfFailedReturn(ExecuteCommand(L"!DumpHeap -Type HistoryInfo -short", HistoryObjects));
    }

    for (wstring historyObject : HistoryObjects)
    {
        vector<wstring> results;
        WCHAR command[MAX_PATH] = { 0 };
        swprintf_s(command, L"!do /d %s", historyObject.c_str());
        ExecuteCommand(command, results);

        Contains(results, L"_cmdline", cmdLines);
    }

    for (wstring output : cmdLines)
    {
        vector<wstring> outputCommand;
        vector<wstring> outputSplit;
        vector<wstring> outputContains;

        Split(output, outputSplit, L' ');

        wstring cmdOffset = outputSplit[6];

        WCHAR command[MAX_PATH] = { 0 };
        swprintf_s(command, L"!do /d %s", cmdOffset.c_str());
        ExecuteCommand(command, outputCommand);

        if (Contains(outputCommand, L"String:", outputContains))
        {
            if (Split(outputContains[0], outputSplit, L' '))
            {
                m_PowerShellHistory.push_back(outputSplit[1]);
            }
        }
    }

    return S_OK;
}

void
PSHistory::OutHistory(
    void
)
{
    for (wstring line : m_PowerShellHistory)
    {
        g_Control4->Output(DEBUG_OUTPUT_NORMAL, "PS timemachine> %S\n", line.c_str());
    }
}

void
PSHistory::AddHistoryToModel(
    void
)
{
    if (m_PowerShellHistory.size())
    {
        AddChildrenToParentModel(L"Debugger.Models.Process", L"PSHistory", m_PowerShellHistory);
    }
}

void
PSHistory::Uninitialize()
{
    if (m_spProcessModelObject != nullptr && m_spHelloExtensionModelObject != nullptr)
    {
        (void)m_spProcessModelObject->RemoveParentModel(m_spHelloExtensionModelObject.Get());
        m_spProcessModelObject = nullptr;
        m_spHelloExtensionModelObject = nullptr;
    }
}

HRESULT
MyIterableConcept::GetIterator(
    _In_ IModelObject * /*pContextObject*/,
    _Out_ IModelIterator **ppIterator)
{
    HRESULT hr = S_OK;
    *ppIterator = nullptr;

    ComPtr<MyIterator> spIterator;
    IfFailedReturn(MakeAndInitialize<MyIterator>(&spIterator, this));

    *ppIterator = spIterator.Detach();
    return S_OK;
}
