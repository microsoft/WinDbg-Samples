//----------------------------------------------------------------------------
//
// stdafx.h
//
// The headers referenced in this file will be precompiled.
//
// Copyright (c) Microsoft. All rights reserved.
//
//----------------------------------------------------------------------------

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include "targetver.h"

#define _ATL_APARTMENT_THREADED

#define _ATL_NO_AUTOMATIC_NAMESPACE

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit


#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#define _CRT_SECURE_NO_WARNINGS
#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
