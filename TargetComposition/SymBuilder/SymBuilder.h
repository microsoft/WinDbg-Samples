//**************************************************************************
//
// SymBuilder.h
//
// Core header for the symbol builder plug-in sample.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __SYMBUILDER_H__
#define __SYMBUILDER_H__

//*************************************************
// General:
//

#ifndef IfFailedReturn
#define IfFailedReturn(EXPR) do { hr = (EXPR); if (FAILED(hr)) { return hr; }} while(FALSE, FALSE)
#endif // IfFailedReturn

#include <wrl.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/module.h>

#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <stack>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <experimental/generator>

#include <DbgServices.h>
#include <DbgServicesBridgeClient.h>

#include <DbgHelp.h>

template<typename FN>
HRESULT ConvertException(const FN& fn)
{
    HRESULT hr;
    try
    {
        hr = fn();
    }
    catch(const std::bad_alloc&)
    {
        hr = E_OUTOFMEMORY;
    }
    catch(...)
    {
        hr = E_FAIL;
    }

    return hr;
}

struct BSTRDeleter
{
    void operator()(_In_z_ wchar_t *pwsz)
    {
        SysFreeString(reinterpret_cast<BSTR>(pwsz));
    }
};

typedef std::unique_ptr<wchar_t, BSTRDeleter> bstr_ptr;

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

// DiagnosticLog():
//
// Sends a diagnostic log message given a diagnostic logging service.  If no diagnostic logging service
// is provided, S_FALSE is returned.
//
// NOTE: Anything logged through this mechanism will go to the WinDbg Preview logs window assuming that the logging
//       level is set appropriately.  Most of our services will default to logging anything of "info" category
//       or higher.  "verbose info" is not normally logged.  This can be changed with the .targetloglevel command
//       or a configuration file.
//
HRESULT DiagnosticLog(_In_opt_ ISvcDiagnosticLogging *pDiagnosticLogging,
                      _In_ DiagnosticLogLevel level,
                      _In_ DiagnosticLogLevel setLevel,
                      _In_opt_ PCWSTR pwszComponent,
                      _In_opt_ PCWSTR pwszCategory,
                      _In_ _Printf_format_string_ PCWSTR pwszMessage,
                      ...);

} // TextDump
} // Services
} // TargetComposition
} // Debugger

#include "InternalGuids.h"
#include "HelpStrings.h"
#include "SymbolBase.h"
#include "SymbolData.h"
#include "SymbolTypes.h"
#include "SymbolSet.h"
#include "SymManager.h"
#include "SymbolServices.h"

#ifdef max
#undef max
#endif // max

#ifdef min
#undef min
#endif // min

#endif // __SYMBUILDER_H__
