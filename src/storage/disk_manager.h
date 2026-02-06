#pragma once

#include "../common/types.h"
#include <string>
#include <fstream>

class DiskManager {
public:
    explicit DiskManager(const std::string& db_path);
    ~DiskManager();

    Status ReadPage(page_id_t page_id, Page& page_data);
    Status WritePage(page_id_t page_id, const Page& page_data);

    // Выделить новую пустую страницу и вернуть её id
    page_id_t AllocatePage();

    page_id_t GetTotalPages() const;

    // Ensure database file data is durable on disk
    Status Sync();

private:
    void InitFileIfEmpty();

    std::string db_path_;
    std::fstream file_handle_;
    page_id_t total_pages_{0};
};