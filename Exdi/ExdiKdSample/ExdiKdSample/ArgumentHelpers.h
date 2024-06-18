//----------------------------------------------------------------------------
//
// ArgumentHelpers.h
//
// Helper methods used to validate and reset output arguments.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once
#include <comdef.h>

template <typename T1> void CheckAndZeroOutArgs(_Out_ T1 *pArg1)
{
    if (pArg1 != nullptr)
    {
        *pArg1 = T1();
    }
    if (pArg1 == nullptr)
    {
        throw _com_error(E_POINTER);
    }
}

template <typename T1, typename T2> void CheckAndZeroOutArgs(_Out_ T1 *pArg1, _Out_ T2 *pArg2)
{
    if (pArg1 != nullptr)
    {
        *pArg1 = T1();
    }
    if (pArg2 != nullptr)
    {
        *pArg2 = T2();
    }

    if (pArg1 == nullptr || pArg2 == nullptr)
    {
        throw _com_error(E_POINTER);
    }
}

template <typename T1, typename T2, typename T3> void CheckAndZeroOutArgs(_Out_ T1 *pArg1, _Out_ T2 *pArg2, _Out_ T3 *pArg3)
{
    if (pArg1 != nullptr)
    {
        *pArg1 = T1();
    }
    if (pArg2 != nullptr)
    {
        *pArg2 = T2();
    }
    if (pArg3 != nullptr)
    {
        *pArg3 = T3();
    }

    if (pArg1 == nullptr || pArg2 == nullptr || pArg3 == nullptr)
    {
        throw _com_error(E_POINTER);
    }
}

template <typename T1, typename T2, typename T3, typename T4> void CheckAndZeroOutArgs(_Out_ T1 *pArg1, 
                                                                                       _Out_ T2 *pArg2, 
                                                                                       _Out_ T3 *pArg3,
                                                                                       _Out_ T4 *pArg4)
{
    if (pArg1 != nullptr)
    {
        *pArg1 = T1();
    }
    if (pArg2 != nullptr)
    {
        *pArg2 = T2();
    }
    if (pArg3 != nullptr)
    {
        *pArg3 = T3();
    }
    if (pArg4 != nullptr)
    {
        *pArg4 = T4();
    }

    if (pArg1 == nullptr || pArg2 == nullptr || pArg3 == nullptr || pArg4 == nullptr)
    {
        throw _com_error(E_POINTER);
    }
}

template <typename T1, typename T2, typename T3, typename T4, typename T5> void CheckAndZeroOutArgs(_Out_ T1 *pArg1, 
                                                                                                    _Out_ T2 *pArg2, 
                                                                                                    _Out_ T3 *pArg3,
                                                                                                    _Out_ T4 *pArg4,
                                                                                                    _Out_ T5 *pArg5)
{
    if (pArg1 != nullptr)
    {
        *pArg1 = T1();
    }
    if (pArg2 != nullptr)
    {
        *pArg2 = T2();
    }
    if (pArg3 != nullptr)
    {
        *pArg3 = T3();
    }
    if (pArg4 != nullptr)
    {
        *pArg4 = T4();
    }
    if (pArg5 != nullptr)
    {
        *pArg5 = T5();
    }

    if (pArg1 == nullptr || pArg2 == nullptr || pArg3 == nullptr || pArg4 == nullptr || pArg5 == nullptr)
    {
        throw _com_error(E_POINTER);
    }
}