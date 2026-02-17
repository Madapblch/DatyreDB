// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Embedded Usage Example                                           ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "datyredb/version.hpp"
#include "datyredb/config.hpp"
#include "internal/core/storage_engine.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

#include <cstring>
#include <iostream>
#include <vector>

int main() {
    fmt::print(fmt::emphasis::bold, "\n=== DatyreDB Embedded Usage Example ===\n\n");
    
    fmt::print("Version: {}\n", datyredb::VERSION_STRING);
    fmt::print("Build:   {} ({})\n\n", datyredb::BUILD_TYPE, datyredb::COMPILER_ID);
    
    // Configure storage engine
    datyredb::core::StorageEngine::Config config;
    config.data_directory = "./example_data";
    config.buffer_pool_size = 1000;
    config.checkpoint.max_interval = std::chrono::seconds(30);
    
    // Create and initialize engine
    datyredb::core::StorageEngine engine(config);
    
    fmt::print("Initializing storage engine...\n");
    
    auto init_status = engine.initialize();
    if (!init_status) {
        fmt::print(fg(fmt::color::red), "ERROR: {}\n", init_status.error().to_string());
        return 1;
    }
    
    fmt::print(fg(fmt::color::green), "Storage engine initialized successfully!\n\n");
    
    // Create some pages
    fmt::print("Creating pages...\n");
    
    std::vector<datyredb::storage::PageId> page_ids;
    
    for (int i = 0; i < 5; ++i) {
        auto result = engine.create_page();
        if (!result) {
            fmt::print(fg(fmt::color::red), "Failed to create page: {}\n", 
                result.error().to_string());
            continue;
        }
        
        datyredb::storage::Page* page = *result;
        page_ids.push_back(page->id());
        
        // Write some data
        std::string data = fmt::format("Page {} data - Hello from DatyreDB!", i);
        std::memcpy(page->payload(), data.c_str(), data.size() + 1);
        
        // Release with dirty flag
        if (auto status = engine.release_page(page->id(), true); !status) {
            fmt::print(fg(fmt::color::red), "Failed to release page {}: {}\n", 
                page->id(), status.error().to_string());
        }
        
        fmt::print("  Created page {} with data\n", page->id());
    }
    
    fmt::print("\n");
    
    // Print statistics
    fmt::print("Statistics:\n");
    fmt::print("  Buffer pool size:  {} pages\n", engine.buffer_pool_size());
    fmt::print("  Dirty pages:       {}\n", engine.dirty_page_count());
    fmt::print("  Total pages:       {}\n", engine.page_count());
    fmt::print("  WAL size:          {} bytes\n", engine.wal_size());
    fmt::print("  Current LSN:       {}\n", engine.current_lsn());
    fmt::print("\n");
    
    // Perform checkpoint
    fmt::print("Performing checkpoint...\n");
    
    auto checkpoint_status = engine.checkpoint_sync();
    if (!checkpoint_status) {
        fmt::print(fg(fmt::color::red), "Checkpoint failed: {}\n",
            checkpoint_status.error().to_string());
    } else {
        fmt::print(fg(fmt::color::green), "Checkpoint completed!\n");
    }
    
    fmt::print("\nAfter checkpoint:\n");
    fmt::print("  Dirty pages: {}\n", engine.dirty_page_count());
    fmt::print("\n");
    
    // Read back data
    fmt::print("Reading pages back...\n");
    
    for (auto page_id : page_ids) {
        auto result = engine.get_page(page_id);
        if (!result) {
            fmt::print(fg(fmt::color::red), "  Failed to read page {}: {}\n",
                page_id, result.error().to_string());
            continue;
        }
        
        datyredb::storage::Page* page = *result;
        fmt::print("  Page {}: {}\n", page->id(), page->payload());
        
        // Release page (not modified)
        (void)engine.release_page(page->id(), false);
    }
    
    fmt::print("\n");
    
    // Shutdown
    fmt::print("Shutting down...\n");
    engine.shutdown();
    
    fmt::print(fg(fmt::color::green), "Done!\n\n");
    
    return 0;
}
