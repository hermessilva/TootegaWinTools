// TootegaWinMFCLib - MFC Static Library
// Tootega Pesquisa e Inovação - All rights reserved © 1999-2026
#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include <SDKDDKVer.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _AFX_ALL_WARNINGS

#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>
#include <afxcontrolbars.h>
#include <afxdialogex.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwmapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <atomic>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// Use the unified Tootega Windows Library
#include "TootegaWinLib.h"
#include "XStringConversion.h"
