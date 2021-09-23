//----------------------------------------------------------------------------
//
// ComHelpers.h
//
// Helper methods used to facilitate COM-related tasks.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once
#include <assert.h>
#include <stdlib.h>

namespace COMHelpers
{
    static inline LPOLESTR CopyStringToTaskMem(const wchar_t *pString)
    {
        assert(pString != nullptr);
        size_t stringSizeInBytes = (wcslen(pString) + 1) * sizeof(wchar_t);
        LPOLESTR pResult = (LPOLESTR)CoTaskMemAlloc(stringSizeInBytes);
        if (pResult != nullptr)
        {
            memcpy(pResult, pString, stringSizeInBytes);
        }
        return pResult;
    }
}