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
#include <queue>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <experimental/generator>
#include <boost/icl/interval_map.hpp>

#include <DbgServices.h>
#include <DbgServicesBridgeClient.h>

//
// NOTE: A number of things in Sym* rely on constants that are in cvconst.h.  Unfortunately, that is NOT
//       in the same SDKs as DbgHelp.  *SOME* of these are defined in the DbgHelp.h header with _NO_CVCONST_H
//       defined and others are not.  We manually copy a few definitions here.
//
#define _NO_CVCONST_H
#include <DbgHelp.h>

enum BasicType
{
    btNoType = 0,
    btVoid = 1,
    btChar = 2,
    btWChar = 3,
    btInt = 6,
    btUInt = 7,
    btFloat = 8,
    btBCD = 9,
    btBool = 10,
    btLong = 13,
    btULong = 14,
    btCurrency = 25,
    btDate = 26,
    btVariant = 27,
    btComplex = 28,
    btBit = 29,
    btBSTR = 30,
    btHresult = 31,
    btChar16 = 32,  // char16_t
    btChar32 = 33,  // char32_t
};

enum DataKind
{
    DataIsUnknown,
    DataIsLocal,
    DataIsStaticLocal,
    DataIsParam,
    DataIsObjectPtr,
    DataIsFileStatic,
    DataIsGlobal,
    DataIsMember,
    DataIsStaticMember,
    DataIsConstant
};

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

struct LocalStringDeleter
{
    void operator()(_In_ WCHAR *pwsz)
    {
        LocalFree(pwsz);
    }
};

typedef std::unique_ptr<WCHAR, LocalStringDeleter> localstr_ptr;

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
// NOTE: Anything logged through this mechanism will go to the WinDbg logs window assuming that the logging
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
#include "SymbolFunction.h"
#include "ImportSymbols.h"
#include "SymbolSet.h"
#include "CallingConvention.h"
#include "SymManager.h"
#include "SymbolServices.h"

#ifdef max
#undef max
#endif // max

#ifdef min
#undef min
#endif // min

#endif // __SYMBUILDER_H__
