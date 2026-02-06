#include "disk_manager.h"

#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#ifdef ERROR
#undef ERROR
#endif
#else
#include <fcntl.h>
#include <unistd.h>
#endif

DiskManager::DiskManager(const std::string& db_path) : db_path_(db_path) {
    // Пытаемся открыть существующий файл
    file_handle_.open(db_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_handle_) {
        // Если не получилось, создаём новый
        file_handle_.clear();
        file_handle_.open(db_path, std::ios::out | std::ios::binary);
        if (!file_handle_) {
            std::cerr << "Error: cannot create database file: " << db_path << "\n";
            return;
        }
        file_handle_.close();

        // Открываем снова для чтения/записи
        file_handle_.open(db_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_handle_) {
            std::cerr << "Error: cannot reopen database file: " << db_path << "\n";
            return;
        }
    }

    file_handle_.seekg(0, std::ios::end);
    std::streamoff size = file_handle_.tellg();
    if (size <= 0) {
        total_pages_ = 0;
        InitFileIfEmpty();
    } else {
        total_pages_ = static_cast<page_id_t>(
            size / static_cast<std::streamoff>(PAGE_SIZE)
        );
    }
}

DiskManager::~DiskManager() {
    if (file_handle_.is_open()) {
        file_handle_.close();
    }
}

Status DiskManager::Sync() {
    if (!file_handle_.is_open()) return Status::FILE_NOT_FOUND;
    file_handle_.flush();

    // Cross-platform durability:
    // - Windows: FlushFileBuffers(handle)
    // - POSIX (Linux/macOS): fsync(fd)
#if defined(_WIN32)
    HANDLE h = ::CreateFileA(
        db_path_.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (h == INVALID_HANDLE_VALUE) {
        return Status::ERROR;
    }
    const BOOL ok = ::FlushFileBuffers(h);
    ::CloseHandle(h);
    return ok ? Status::OK : Status::ERROR;
#else
    const int fd = ::open(db_path_.c_str(), O_RDONLY);
    if (fd < 0) return Status::ERROR;
    const int rc = ::fsync(fd);
    ::close(fd);
    return (rc == 0) ? Status::OK : Status::ERROR;
#endif
}

void DiskManager::InitFileIfEmpty() {
    if (total_pages_ > 0) return;
    Page blank{};
    file_handle_.seekp(0, std::ios::beg);
    file_handle_.write(blank.data(), PAGE_SIZE);
    file_handle_.flush();
    total_pages_ = 1;
}

page_id_t DiskManager::AllocatePage() {
    Page blank{};
    file_handle_.seekp(static_cast<std::streamoff>(total_pages_) * PAGE_SIZE,
                       std::ios::beg);
    file_handle_.write(blank.data(), PAGE_SIZE);
    file_handle_.flush();
    return total_pages_++;
}

Status DiskManager::ReadPage(page_id_t page_id, Page& page_data) {
    if (!file_handle_.is_open())
        return Status::FILE_NOT_FOUND;

    if (page_id >= total_pages_) {
        return Status::PAGE_NOT_FOUND;
    }

    file_handle_.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE,
                       std::ios::beg);
    file_handle_.read(page_data.data(), PAGE_SIZE);
    if (!file_handle_) {
        file_handle_.clear();
        return Status::ERROR;
    }

    return Status::OK;
}

Status DiskManager::WritePage(page_id_t page_id, const Page& page_data) {
    if (!file_handle_.is_open())
        return Status::FILE_NOT_FOUND;

    if (page_id >= total_pages_) {
        return Status::PAGE_NOT_FOUND;
    }

    file_handle_.seekp(static_cast<std::streamoff>(page_id) * PAGE_SIZE,
                       std::ios::beg);
    file_handle_.write(page_data.data(), PAGE_SIZE);
    if (!file_handle_) {
        file_handle_.clear();
        return Status::ERROR;
    }
    file_handle_.flush();
    return Status::OK;
}

page_id_t DiskManager::GetTotalPages() const {
    return total_pages_;
}