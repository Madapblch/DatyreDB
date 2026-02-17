// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - High-Performance Embedded Database Engine                        ║
// ║  Copyright (c) 2024 DatyreDB Authors                                         ║
// ║  SPDX-License-Identifier: MIT                                                ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#pragma once

// Configuration (auto-generated)
#include "datyredb/config.hpp"
#include "datyredb/version.hpp"

// Core types
// #include "datyredb/types.hpp"
// #include "datyredb/error.hpp"
// #include "datyredb/database.hpp"

namespace datyredb {

/// Library initialization (call once at startup)
DATYREDB_API bool initialize();

/// Library shutdown (call once at exit)
DATYREDB_API void shutdown();

/// Check if library is initialized
DATYREDB_API bool is_initialized();

} // namespace datyredb
