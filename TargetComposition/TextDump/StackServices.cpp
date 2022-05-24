//**************************************************************************
//
// StackServices.cpp
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

#include "TextDump.h"

using namespace Microsoft::WRL;

namespace Debugger
{
namespace TargetComposition
{
namespace Services
{
namespace TextDump
{

HRESULT GenericFrame::GetFrameText(_Out_ BSTR *pFrameText)
{
    auto fn = [&]()
    {
        std::wstring frameText = L"";

        if (!m_pFrame->Module.empty())
        {
            frameText += m_pFrame->Module;
            frameText += L"!";
        }

        frameText += m_pFrame->Symbol;
        if (m_pFrame->Displacement > 0)
        {
            wchar_t buf[32];
            swprintf_s(buf, ARRAYSIZE(buf), L"+0x%I64x", m_pFrame->Displacement);
            frameText += buf;
        }

        *pFrameText = SysAllocString(frameText.c_str());
        return (*pFrameText == nullptr ? E_OUTOFMEMORY : S_OK);
    };

    //
    // We cannot let a C++ exception escape back into the debugger.
    //
    return ConvertException(fn);
}

HRESULT PartialPhysicalFrame::GetInstructionPointer(_Out_ ULONG64 *pInstructionPointer)
{
    auto&& stackFrames = m_spParsedFile->GetStackFrames();
    if (m_pFrame->FrameNumber >= stackFrames.size())
    {
        return E_UNEXPECTED;
    }

    //
    // Bear in mind, we have the result of a 'k' command effectively.  The original instruction pointer
    // would be in the *REGISTER CONTEXT* of the thread.  Each one after would be the retAddr that was
    // saved in the stack (since we don't deal with inline frames).
    //
    if (m_pFrame->FrameNumber == 0)
    {
        if (!m_spParsedFile->HasRegisters())
        {
            return E_NOT_SET;
        }

        //
        // For sample purposes, we hard target x64 and thus look for a register named rip from the text file.
        // We could plumb an ISvcRegisterContext here and ask for the *ABSTRACT* instruction pointer
        // for that register context to be more generic.  We don't for the purposes of this sample.
        //
        auto&& registers = m_spParsedFile->GetRegisters();
        for(auto&& curRegister : registers)
        {
            if (wcscmp(curRegister.Name.c_str(), L"rip") == 0)
            {
                *pInstructionPointer = curRegister.Value;
                return S_OK;
            }
        }

        return E_NOT_SET;
    }
    else
    {
        auto&& prevFrame = stackFrames[static_cast<size_t>(m_pFrame->FrameNumber) - 1];
        *pInstructionPointer = prevFrame.RetAddr;
        return S_OK;
    }
}

HRESULT PartialPhysicalFrame::GetStackPointer(_Out_ ULONG64 *pStackPointer)
{
    auto&& stackFrames = m_spParsedFile->GetStackFrames();
    if (m_pFrame->FrameNumber >= stackFrames.size())
    {
        return E_UNEXPECTED;
    }

    auto&& frame = stackFrames[static_cast<size_t>(m_pFrame->FrameNumber)];
    *pStackPointer = frame.ChildSp;
    return S_OK;
}

HRESULT FrameSetEnumerator::GetCurrentFrame(_COM_Outptr_ ISvcStackProviderFrame **ppCurrentFrame)
{
    HRESULT hr = S_OK;
    *ppCurrentFrame = nullptr;

    auto&& stackFrames = m_spParsedFile->GetStackFrames();
    if (m_pos >= stackFrames.size())
    {
        //
        // E_BOUNDS indicates the end of iteration.
        //
        return E_BOUNDS;
    }

    StackFrame const* pFrame = &(stackFrames[m_pos]);

    //
    // If we have register context, return a partial physical frame.  If we do not, return a generic frame.
    // This will allow some exploration of different features of the stack walker by experimenting with
    // sample "text dumps"
    //

    if (m_spParsedFile->HasRegisters())
    {
        ComPtr<PartialPhysicalFrame> spPartialPhysicalFrame;
        IfFailedReturn(MakeAndInitialize<PartialPhysicalFrame>(&spPartialPhysicalFrame, m_spParsedFile, pFrame));
        *ppCurrentFrame = spPartialPhysicalFrame.Detach();
    }
    else
    {
        ComPtr<GenericFrame> spGenericFrame;
        IfFailedReturn(MakeAndInitialize<GenericFrame>(&spGenericFrame, m_spParsedFile, pFrame));
        *ppCurrentFrame = spGenericFrame.Detach();
    }

    return S_OK;
}

HRESULT FrameSetEnumerator::MoveNext()
{
    auto&& stackFrames = m_spParsedFile->GetStackFrames();
    if (m_pos >= stackFrames.size())
    {
        return E_BOUNDS;
    }

    ++m_pos;
    return (m_pos >= stackFrames.size() ? E_BOUNDS : S_OK);
}

HRESULT StackProviderService::StartStackWalk(_In_ ISvcStackUnwindContext *pUnwindContext,
                                             _COM_Outptr_ ISvcStackProviderFrameSetEnumerator **ppFrameEnum)
{
    HRESULT hr = S_OK;
    *ppFrameEnum = nullptr;

    //
    // Normally, pUnwindContext would tell us what process/thread we were trying to provide a stack for.
    // Given that we know our "text dump" format has only one process and thread, we don't really need to
    // look.  The unwind context also allows us to temporarily store information associated with this stack
    // unwind within the unwind context.
    //
    ComPtr<FrameSetEnumerator> spFrameSetEnum;
    IfFailedReturn(MakeAndInitialize<FrameSetEnumerator>(&spFrameSetEnum, m_spParsedFile, pUnwindContext));

    *ppFrameEnum = spFrameSetEnum.Detach();
    return hr;
}

} // TextDump
} // Services
} // TargetComposition
} // Debugger
