//----------------------------------------------------------------------------
//
// ExceptionHandlers.h
//
// Auxiliary definitions used to handle exceptions.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once

#include <exception>
#include <comdef.h> 

#define CATCH_AND_RETURN_HRESULT    \
    catch(_com_error const &error)  \
    {                               \
        return error.Error();       \
    }                               \
    catch(std::bad_alloc const &)   \
    {                               \
        return E_OUTOFMEMORY;       \
    }                               \
    catch (...)                     \
    {                               \
        return E_FAIL;              \
    }
