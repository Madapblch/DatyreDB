#pragma once

#include "../common/types.h"
#include <cstdio>
#include <string>
#include <functional>

class WAL {
public:
    explicit WAL(const std::string& path);
    ~WAL();

    // Append a full-page image record for page_id
    Status AppendPageRecord(page_id_t page_id, const Page& page);

    // Ensure data is flushed to durable storage
    Status Flush();

    // Recover by replaying all complete records and calling apply_fn for each
    Status Recover(const std::function<void(page_id_t, const Page&)>& apply_fn);

    // Truncate WAL (remove all records). Used after checkpoint.
    Status Truncate();

    // Truncate WAL up to given byte offset (LSN). Safe to pass ScanLastValidOffset().
    Status TruncateUpTo(uint64_t upto_lsn);

    // Current end-of-log LSN (next byte offset to be written).
    uint64_t NextLSN() const { return next_lsn_; }

private:
    // Scan log from start, validating records; returns last valid byte offset.
    uint64_t ScanLastValidOffset(std::FILE* r);

    // Truncate underlying file to size bytes.
    Status TruncateFileTo(uint64_t size_bytes);

    std::FILE* file_{nullptr};
    std::string path_;
    uint64_t next_lsn_{0};
};
