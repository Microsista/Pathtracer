//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <sstream>
#include <iomanip>

#include <list>
#include <string>
#include <wrl.h>
#include <shellapi.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <atlbase.h>
#include <assert.h>
#include <array>
#include <unordered_map>
#include <algorithm>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <atlbase.h>
#include "../Util/d3dx12.h"

#include <DirectXMath.h>
#include <DirectXCollision.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include "../Util/DXSampleHelper.h"
#include "../Util/DeviceResources.h"

#include <iostream>
#include <fstream>

//#include <WICTextureLoader.h>
#include "DDSTextureLoader12.h"
#include <wrl/event.h>
#include "../Util/ResourceUploadBatch.h"
