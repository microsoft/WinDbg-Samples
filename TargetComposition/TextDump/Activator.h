//**************************************************************************
//
// Activator.h
//
// File activator for our "text dumps" as a post-mortem dump target.  The file activator 
// is what the debugger will call to determine whether you handle a particular file format 
// and, if you do, to fill a target composition container with the services required to actually
// debug a particular target.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __ACTIVATOR_H__
#define __ACTIVATOR_H__

namespace Debugger
{
namespace TargetComposition
{
namespace FileActivators
{

// TextDumpActivator:
//
// A file activator which initializes a composition stack for the examination of text dumps.
//
class TextDumpActivator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugTargetCompositionFileActivator,
        ISvcDiagnosticLoggableControl
        >
{
public:

    //*************************************************
    // IDebugTargetCompositionFileActivator:
    //

    // IsRecognizedFile():
    //
    // Returns whether or not we recognize the file target as our "text dump" format.  Note that
    // we will potentially be called for *any* file that has a ".txt" extension and is opened as a
    // post-mortem dump target by the debugger.  We must actually parse enough to say "yes/no" that 
    // this is *OUR* file format.
    //
    IFACEMETHOD(IsRecognizedFile)(_In_ IDebugServiceManager *pServiceManager,
                                  _In_ ISvcDebugSourceFile *pFile,
                                  _Out_ bool *pIsRecognized);

    // InitializeServices():
    //
    // If we recognize the file type as *OUR* text dump format, populate the composition stack with the 
    // components necessary to debug the text dump as a target.
    //
    IFACEMETHOD(InitializeServices)(_In_ IDebugServiceManager *pServiceManager);

    //*************************************************
    // ISvcDiagnosticLoggableControl:
    //

    IFACEMETHOD_(DiagnosticLogLevel, GetLoggingLevel)() { return m_diagLevel; }
    IFACEMETHOD_(void, SetLoggingLevel)(_In_ DiagnosticLogLevel level) { m_diagLevel = level; }

    //*************************************************
    // Internal:
    //

    // RuntimeClassInitialize():
    //
    // Initialize the activator.
    //
    HRESULT RuntimeClassInitialize(_In_ IDebugTargetComposition *pCompositionManager)
    {
        m_diagLevel = DiagnosticLevelInfo;
        m_spCompositionManager = pCompositionManager;
        return S_OK;
    }

private:

    Microsoft::WRL::ComPtr<IDebugTargetComposition> m_spCompositionManager;
    DiagnosticLogLevel m_diagLevel;

    // DiagnosticLog():
    //
    // Makes a call to the diagnostic logging service to log a message.  If the diagnostic log service is not present,
    // S_FALSE is returned.
    //
    HRESULT DiagnosticLog(_In_ IDebugServiceManager *pServiceManager,
                          _In_ DiagnosticLogLevel level,
                          _In_opt_ PCWSTR pwszComponent,
                          _In_opt_ PCWSTR pwszCategory,
                          _In_ PCWSTR pwszLogMessage);

};

} // FileActivators
} // TargetComposition
} // Debugger

#endif // __ACTIVATOR_H__
