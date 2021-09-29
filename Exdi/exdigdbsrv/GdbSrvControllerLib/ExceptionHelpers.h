//----------------------------------------------------------------------------
//
// ExceptionHelpers.h
//
// Auxiliary definitions used to handle exceptions.
//
// Copyright (c) Microsoft. All rights reserved.
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

#define CATCH_AND_RETURN_BOOLEAN    \
    catch(std::bad_alloc const &)   \
    {                               \
        return false;               \
    }                               \
    catch (...)                     \
    {                               \
        return false;               \
    }

#define CATCH_AND_RETURN_DWORD      \
    catch(std::bad_alloc const &)   \
    {                               \
        return ERROR_NOT_ENOUGH_MEMORY;   \
    }                               \
    catch (...)                     \
    {                               \
        return ERROR_UNHANDLED_EXCEPTION; \
    }
