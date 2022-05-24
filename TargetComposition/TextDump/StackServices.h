//**************************************************************************
//
// StackServices.h
//
// Target composition services to provide call stack information to the debugger
// from our "text dump" file format.
//
// There are two levels that stacks can be provided to the debugger:
//
//     - The stack *UNWINDER* service.  This understands how to take a register
//       context and information about a previously unwound stack frame and
//       return an abstract frame and register context for the next stack frame.
//       This is typically the layer used for most targets.  The debugger has
//       a built-in and deep understanding of how to unwind standard frames
//       on Windows. 
//
//       The stack unwinder does, however, require pretty full access to memory,
//       registers, and images/symbols in order to get unwind data.
//
//     - The stack *PROVIDER* service.  This is a higher level abstraction that
//       returns a set of stack frames which may or may not be based on
//       a hard register context.  A stack provider can return synthetic frames
//       without having any memory underneath.  A stack unwinder cannot.
//
// For the purposes of this text format, we provide a stack *PROVIDER* and return
// different types of frames depending on what kinds of other information is
// available in the text dump.  If there is no memory/register information, the
// frames are synthetic.  If there is memory/register/module information, the 
// frames are close to physical frames.
//
// Again, this strategy is done to show the breadth of what can be done in terms
// of "call stacks" with respect to the debugger.  Many plug-ins that deal in standard
// Windows terms and act like a minidump do not even need to add either to the
// container.  The debugger will do so automatically.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef __STACKSERVICES_H__
#define __STACKSERVICES_H__

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

// GenericFrame:
//
// Represents one of our stack frames expressed as a "generic frame".  In other words, we do not
// have enough information (instruction pointer, stack pointer, frame pointer) to express our stack
// frames as partial physical frames, so we express them as something akin to a "synthetic frame".
//
// Here, we are just returning a textual representation of the frame.
//
class GenericFrame :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcStackProviderFrame,                 // Mandatory (all frame types)
        ISvcStackProviderFrameAttributes        // Mandatory (generic frames, optional all others)
        >
{
public:

    //*************************************************
    // ISvcStackProviderFrame:
    //

    // GetFrameKind():
    //
    // Gets the kind of stack frame that this ISvcStackProviderFrame represents.
    //
    IFACEMETHOD_(StackProviderFrameKind, GetFrameKind)()
    {
        return StackProviderFrameGeneric;
    }

    //*************************************************
    // ISvcStackProviderFrameAttributes:
    //

    // GetFrameText():
    //
    // Gets the "textual representation" of this stack frame.  The meaning of this can vary by stack provider.
    // Conceptually, this is what a debugger would place in a "call stack" window representing this frame.
    //
    // Anyone who implements ISvcStackProviderFrameAttributes *MUST* implement GetFrameText.
    //
    IFACEMETHOD(GetFrameText)(_Out_ BSTR *pFrameText);

    // GetSourceAssociation():
    //
    // Gets the "source association" for this stack frame (e.g.: the source file, line number, and column
    // number).  This is an optional attribute.  It is legal for any implementation to E_NOTIMPL this.
    // The line number and column number are optional (albeit a column cannot be provided without a line).
    // A client may legally return a value of zero for either of these indicating that it is not available or not 
    // relevant (e.g.: compiler generated code which does not necessarily map to a line of code may legally
    // return 0 for the source line).
    //
    IFACEMETHOD(GetSourceAssociation)(_Out_ BSTR *pSourceFile,
                                      _Out_ ULONG64 * /*pSourceLine*/,
                                      _Out_ ULONG64 * /*pSourceColumn*/)
    {
        //
        // We do not save the source lines for each stack frame.  Indicate this.
        //
        *pSourceFile = nullptr;
        return E_NOTIMPL;
    }

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> &parsedFile,
                                   _In_ StackFrame const* pFrame)
    {
        m_spParsedFile = parsedFile;
        m_pFrame = pFrame;
        return S_OK;
    }



private:

    std::shared_ptr<TextDumpParser> m_spParsedFile;
    StackFrame const *m_pFrame;
};

// PartialPhysicalFrame:
//
// Represents one of our stack frames expressed as a "partial physical frame".  In other words,
// we have some of the abstract information associated with a frame for one of these:
// an instruction pointer, a stack pointer, and a frame pointer.  At a *MINIMUM*, we *MUST* have
// the instruction pointer.  The other two values are optional.
//
// If we do not have an instruction pointer (because we don't have the initial register context), 
// we will produce GenericFrame objects which are, in some sense, synthetic stack frames.
//
class PartialPhysicalFrame :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcStackProviderFrame,                 // Mandatory (all frame types)
        ISvcStackProviderPartialPhysicalFrame   // Mandatory (partial physical frames),
        >
{
public:

    //*************************************************
    // ISvcStackProviderFrame:
    //

    // GetFrameKind():
    //
    // Gets the kind of stack frame that this ISvcStackProviderFrame represents.
    //
    IFACEMETHOD_(StackProviderFrameKind, GetFrameKind)()
    {
        return StackProviderFramePartialPhysical;
    }

    //*************************************************
    // ISvcStackProviderPartialPhysicalFrame:
    //

    // GetInstructionPointer():
    //
    // Gets the instruction pointer for this partial physical frame.  This is the *MINIMUM MUST* implement
    // for a partial physical frame.  All other Get* methods within ISvcStackProviderPartialPhysicalFrame
    // may legally return E_NOT_SET.
    //
    IFACEMETHOD(GetInstructionPointer)(_Out_ ULONG64 *pInstructionPointer);

    // GetStackPointer():
    //
    // Gets the stack pointer for this partial physical frame.  This may return E_NOT_SET indicating that there
    // is no available stack pointer value for this partial frame.  All users of a partial physical frame must 
    // be able to deal with such.
    //
    IFACEMETHOD(GetStackPointer)(_Out_ ULONG64 *pStackPointer);

    // GetFramePointer():
    //
    // Gets the frame pointer for this partial physical frame.  This may return E_NOT_SET indicating that there
    // is no available frame pointer value for this partial frame.  All users of a partial physical frame must
    // be able to deal with such.
    //
    IFACEMETHOD(GetFramePointer)(_Out_ ULONG64 * /*pFramePointer*/)
    {
        //
        // Our "text dump" format does not have enough information to return the abstract frame pointer
        // for stack frames.  Indicate this.
        //
        return E_NOT_SET;
    }

    //*************************************************
    // Internal APIs:
    //

    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> &parsedFile,
                                   _In_ StackFrame const* pFrame)
    {
        m_spParsedFile = parsedFile;
        m_pFrame = pFrame;
        return S_OK;
    }

private:

    std::shared_ptr<TextDumpParser> m_spParsedFile;
    StackFrame const *m_pFrame;

};

// FrameSetEnumerator:
//
// An enumerator for the set of stack frames that we expose.
//
class FrameSetEnumerator :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        ISvcStackProviderFrameSetEnumerator     // Mandatory (for all frame set enumerators)
        >
{
public:

    //*************************************************
    // ISvcStackProviderFrameSetEnumerator:
    //

    // GetUnwindContext():
    //
    // Gets the unwinder context which is associated with this frame set.
    //
    IFACEMETHOD(GetUnwindContext)(_COM_Outptr_ ISvcStackUnwindContext **ppUnwindContext)
    {
        Microsoft::WRL::ComPtr<ISvcStackUnwindContext> spUnwindContext = m_spUnwindContext;
        *ppUnwindContext = spUnwindContext.Detach();
        return S_OK;
    }

    // Reset();
    //
    // Resets the enumerator back to the top of the set of frames which it represents.
    //
    IFACEMETHOD(Reset)()
    {
        m_pos = 0;
        return S_OK;
    }

    // GetCurrentFrame():
    //
    // Returns the current frame of the set.  If there is no current frame, this will return E_BOUNDS.
    //
    IFACEMETHOD(GetCurrentFrame)(_COM_Outptr_ ISvcStackProviderFrame **ppCurrentFrame);

    // MoveNext():
    //
    // Moves the enumerator to the next frame.  This will return E_BOUNDS at the end of enumeration.
    //
    IFACEMETHOD(MoveNext)();

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the frame set enumerator.
    //
    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> const& parsedFile,
                                   _In_ ISvcStackUnwindContext *pUnwindContext)
    {
        m_spParsedFile = parsedFile;
        m_spUnwindContext = pUnwindContext;
        return Reset();
    }

private:

    std::shared_ptr<TextDumpParser> m_spParsedFile;
    Microsoft::WRL::ComPtr<ISvcStackUnwindContext> m_spUnwindContext;
    size_t m_pos;
};

// StackProviderService:
//
// A stack provider service that is able to return stack frames to the debugger. 
//
class StackProviderService :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IDebugServiceLayer,                     // Mandatory (for all services)
        ISvcStackProvider                       // Mandatory (for all stack provider services)
        >
{
public:

    //*************************************************
    // IDebugServiceLayer:
    //

    // RegisterServices():
    //
    // Registers all services contained in this component with the services manager.
    //
    IFACEMETHOD(RegisterServices)(_In_ IDebugServiceManager *pServiceManager)
    {
        HRESULT hr = S_OK;
        IfFailedReturn(pServiceManager->RegisterService(DEBUG_SERVICE_STACK_PROVIDER, this));
        return hr;
    }

    // GetServiceDependencies():
    //
    // Returns the set of services which this service layer / component depends on.  Having sizeHardDependencies or
    // sizeSoftDependencies set to 0 will pass back the number of dependencies and do nothing else.
    //
    IFACEMETHOD(GetServiceDependencies)(_In_ ServiceNotificationKind /*notificationKind*/,
                                        _In_ IDebugServiceManager * /*pServiceManager*/,
                                        _In_ REFGUID /*serviceGuid*/,
                                        _In_ ULONG64 sizeHardDependencies,
                                        _Out_writes_(sizeHardDependencies) GUID * /*pHardDependencies*/,
                                        _Out_ ULONG64 *pNumHardDependencies,
                                        _In_ ULONG64 sizeSoftDependencies,
                                        _Out_writes_(sizeSoftDependencies) GUID * /*pSoftDependencies*/,
                                        _Out_ ULONG64 *pNumSoftDependencies)
    {
        HRESULT hr = S_OK;
        if (sizeHardDependencies == 0 && sizeSoftDependencies == 0)
        {
            *pNumHardDependencies = 0;
            *pNumSoftDependencies = 0;
            return S_OK;
        }

        *pNumHardDependencies = 0;
        *pNumSoftDependencies = 0;
        return hr;
    }

    // InitializeServices():
    //
    // Performs initialization of the services in a service layer / component.  Services which aggregate, 
    // encapsulate, or stack on top of other services must pass down the initialization notification in an 
    // appropriate manner (with notificationKind set to LayeredNotification).
    //
    IFACEMETHOD(InitializeServices)(_In_ ServiceNotificationKind /*notificationKind*/,
                                    _In_ IDebugServiceManager * /*pServiceManager*/,
                                    _In_ REFIID /*serviceGuid*/)
    {
        return S_OK;
    }

    // NotifyServiceChange():
    //
    // Called when there is a change in the component registered as a service in the
    // target composition stack.
    //
    IFACEMETHOD(NotifyServiceChange)(_In_ ServiceNotificationKind /*notificationKind*/,
                                     _In_ IDebugServiceManager * /*pServiceManager*/,
                                     _In_ REFIID /*serviceGuid*/,
                                     _In_opt_ IDebugServiceLayer * /*pPriorService*/,
                                     _In_opt_ IDebugServiceLayer * /*pNewService*/)
    {
        return S_OK;
    }

    // NotifyEvent():
    //
    // Called to notify this component that an event of interest occurred.
    //
    IFACEMETHOD(NotifyEvent)(_In_ IDebugServiceManager * /*pServiceManager*/,
                             _In_ REFIID /*eventGuid*/,
                             _In_opt_ IUnknown * /*pEventArgument*/)
    {
        return S_OK;
    }

    //*************************************************
    // ISvcStackProvider
    //

    // StartStackWalk():
    //
    // Starts a stack walk for the execution unit given by the unwind context and returns a frame set enumerator
    // representing the frames within that stack walk.
    //
    IFACEMETHOD(StartStackWalk)(_In_ ISvcStackUnwindContext *pUnwindContext,
                                _COM_Outptr_ ISvcStackProviderFrameSetEnumerator **ppFrameEnum);

    // StartStackWalkForAlternateContext():
    //
    // Starts a stack walk given an alternate starting register context.  Other than assuming a different
    // initial register context than StartStackWalk, the method operates identically.  Stack providers which deal
    // in physical frames *SHOULD* implement this method.  Stack providers which do not may legally E_NOTIMPL
    // this method.
    //
    IFACEMETHOD(StartStackWalkForAlternateContext)(_In_ ISvcStackUnwindContext * /*pUnwindContext*/,
                                                   _In_ ISvcRegisterContext * /*pRegisterContext*/,
                                                   _COM_Outptr_ ISvcStackProviderFrameSetEnumerator **ppFrameEnum)
    {
        //
        // As we do not deal with anything but the singular stack and cannot necessarily start returning stack frames
        // from an arbitrary register context, we return E_NOTIMPL here.
        //
        *ppFrameEnum = nullptr;
        return E_NOTIMPL;
    }

    //*************************************************
    // Internal APIs:
    //

    // RuntimeClassInitialize():
    //
    // Initializes the stack provider service.
    //
    HRESULT RuntimeClassInitialize(_In_ std::shared_ptr<TextDumpParser> const& parsedFile)
    {
        m_spParsedFile = parsedFile;
        return S_OK;
    }

private:

    std::shared_ptr<TextDumpParser> m_spParsedFile;
};

} // TextDump
} // Services
} // TargetComposition
} // Debugger

#endif // __STACKSERVICES_H__

