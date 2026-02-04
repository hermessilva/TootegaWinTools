#pragma once
// ============================================================================
// TootegaWinLib.h - Master include header for Tootega common library
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// This header aggregates all common utilities for Tootega C++ projects.
// Include this single header to access the complete common library:
//   - Platform detection and configuration
//   - RAII wrappers and smart pointer types
//   - Result type for error handling
//   - Logging infrastructure
//   - String utilities and encoding conversion
//   - Registry operations
//   - Cryptographic helpers
//   - File system utilities
//   - Memory management utilities
//   - Shell extension infrastructure (see XShell.h for focused shell APIs)
//
// ============================================================================

#include "XPlatform.h"
#include "XTypes.h"
#include "XResult.h"
#include "XMemory.h"
#include "XString.h"
#include "XStringConversion.h"
#include "XLogger.h"
#include "XEventLog.h"
#include "XRegistry.h"
#include "XCrypto.h"
#include "XFile.h"
#include "XProcess.h"
#include "XEventLogForensic.h"
#include "XGlobalEvent.h"
