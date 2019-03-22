//**************************************************************************
// 
// SimpleIntro.h
//
// Main Internal Header 
//
//**************************************************************************

#include <windows.h>
#include <oaidl.h>
#include <wrl.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/module.h>

// This is needed when using SDK version 10.0.17763.0
// For newer versions of the SDK, this is not necessary
using namespace Microsoft::WRL;

#include <dbgmodel.h>
#include "DbgModelClientEx.h"

#include "HelloProvider.h"
#include "SimpleIntroExtension.h"

