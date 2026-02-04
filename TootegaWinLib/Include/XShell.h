#pragma once
// ============================================================================
// XShell.h - Windows Shell Extension Library
// Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
// ============================================================================
//
// This header aggregates all shell extension utilities for building
// Windows Explorer extensions:
//   - Shell extension base classes (COM objects)
//   - Context menu handlers
//   - Property handlers
//   - Icon handlers
//   - Preview handlers
//   - Shell folder namespace extensions
//   - Registry utilities for shell extension registration
//
// Include this single header to build complete shell extensions.
//
// ============================================================================

#include "Shell/XShellExtension.h"
#include "Shell/XShellRegistry.h"
#include "Shell/XContextMenu.h"
#include "Shell/XPropertyHandler.h"
#include "Shell/XIconHandler.h"
#include "Shell/XPreviewHandler.h"
#include "Shell/XShellFolder.h"

