//**************************************************************************
//
// PyProvider.h
//
// Core header for Python Script Extension.
//
//**************************************************************************
//
//**************************************************************************

//*************************************************
// General:
//

#include <windows.h>
#include <shlwapi.h>
#include <wrl.h>

#include <exception>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>

//*************************************************
// Extension:
//

#include <dbgmodel.h>
#include <DbgModelClientEx.h>

//*************************************************
// Python Specific
//

#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else // !defined(_DEBUG)
#include <Pyton.h>
#endif // _DEBUG

//*************************************************
// Macros:
//

#ifdef max
#undef max
#endif // max

#ifdef min
#undef min
#endif // min

#define IfFailedReturn(EXPR) do { hr = (EXPR); if (FAILED(hr)) { return hr; }} while(FALSE, FALSE)
#define IfFailedThrow(hr) if (FAILED(hr)) { PythonProvider::Get()->GetMarshaler()->SetDataModelError(hr, nullptr); return nullptr; }
#define IfStatusErrorConvertAndReturn(result) if (PyStatus_Exception(result)) { return E_FAIL; }
#define IfObjectErrorConvertAndReturn(result) if (result == nullptr) { return E_FAIL; }

//*************************************************
// Key Forward References
//

namespace Debugger::DataModel::ScriptProvider::Python
{
    class PythonProvider;
    class PythonScript;
    class PythonScriptState;

    namespace Marshal
    {
        class PythonMarshaler;
    };

} // Debugger::DataModel::ScriptProvider::Python

//*************************************************
// Core Provider:
//

#include "StringResource.h"
#include "Utility.h"
#include "Marshal.h"
#include "PyFunctions.h"
#include "HostLibrary.h"
#include "ScriptProvider.h"
#include "ScriptTemplates.h"

