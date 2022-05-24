//**************************************************************************
//
// InternalGuids.cpp
//
// Internal GUID definitions for our "text dump" plug - in.
// 
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <stack>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <functional>

//*************************************************
// GUID Defintions for the plug-in
// 

#define INITGUID
#include <guiddef.h>
#include "InternalGuids.h"
#include <DbgServices.h>
#undef INITGUID