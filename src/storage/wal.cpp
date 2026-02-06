#include "wal.h"

#include <cstring>
#include <cerrno>
#include <iostream>
#include <cstdint>

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#define fsync _commit
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace {

constexpr uint32_t kWalMagic = 0x314C4157u; // "WAL1" little-endian
constexpr uint16_t kWalVersion = 1;

// Simple CRC32 (IEEE 802.3). Good enough for WAL corruption detection.
uint32_t crc32_ieee(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint32_t>(data[i]);
        for (int b = 0; b < 8; ++b) {
            const uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

struct WalRecordHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint64_t lsn;          // byte offset of this record
    uint32_t page_id;
    uint32_t payload_size; // in bytes (for now: PAGE_SIZE)
    uint32_t payload_crc32;
    uint32_t header_crc32; // crc32 of header with this field set to 0
};

static_assert(sizeof(WalRecordHeader) % 4 == 0, "WalRecordHeader alignment");

uint32_t header_crc(const WalRecordHeader& hdr) {
    WalRecordHeader tmp = hdr;
    tmp.header_crc32 = 0;
    return crc32_ieee(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
}

} // namespace

WAL::WAL(const std::string& path) : path_(path) {
    // Open for append (create if missing)
    file_ = std::fopen(path_.c_str(), "ab+");
    if (!file_) {
        std::cerr << "WAL: cannot open file " << path_ << " for append: " << std::strerror(errno) << "\n";
    } else {
        // Determine last valid offset (ignore partial/corrupt tail).
        std::FILE* r = std::fopen(path_.c_str(), "rb");
        if (!r) {
            next_lsn_ = 0;
            return;
        }
        const uint64_t last_valid = ScanLastValidOffset(r);
        std::fclose(r);

        // If file ends with an incomplete/corrupt record, truncate it away.
        std::fseek(file_, 0, SEEK_END);
        long end_pos = std::ftell(file_);
        const uint64_t file_size = end_pos > 0 ? static_cast<uint64_t>(end_pos) : 0;
        if (file_size != last_valid) {
            (void)TruncateFileTo(last_valid); // best-effort
        }
        next_lsn_ = last_valid;
    }
}

WAL::~WAL() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

Status WAL::AppendPageRecord(page_id_t page_id, const Page& page) {
    if (!file_) return Status::ERROR;

    WalRecordHeader hdr{};
    hdr.magic = kWalMagic;
    hdr.version = kWalVersion;
    hdr.header_size = static_cast<uint16_t>(sizeof(WalRecordHeader));
    hdr.lsn = next_lsn_;
    hdr.page_id = static_cast<uint32_t>(page_id);
    hdr.payload_size = static_cast<uint32_t>(PAGE_SIZE);
    hdr.payload_crc32 = crc32_ieee(reinterpret_cast<const uint8_t*>(page.data()), PAGE_SIZE);
    hdr.header_crc32 = header_crc(hdr);

    size_t wrote = std::fwrite(&hdr, sizeof(hdr), 1, file_);
    if (wrote != 1) {
        std::cerr << "WAL: failed to write header: " << std::strerror(errno) << "\n";
        return Status::ERROR;
    }

    wrote = std::fwrite(page.data(), 1, PAGE_SIZE, file_);
    if (wrote != PAGE_SIZE) {
        std::cerr << "WAL: failed to write payload: " << std::strerror(errno) << "\n";
        return Status::ERROR;
    }

    next_lsn_ += sizeof(hdr) + PAGE_SIZE;
    return Status::OK;
}

Status WAL::Flush() {
    if (!file_) return Status::ERROR;
    if (std::fflush(file_) != 0) {
        std::cerr << "WAL: fflush failed: " << std::strerror(errno) << "\n";
        return Status::ERROR;
    }

    int fd = fileno(file_);
    if (fd < 0) return Status::ERROR;

    if (fsync(fd) != 0) {
        std::cerr << "WAL: fsync failed: " << std::strerror(errno) << "\n";
        return Status::ERROR;
    }

    return Status::OK;
}

uint64_t WAL::ScanLastValidOffset(std::FILE* r) {
    if (!r) return 0;
    uint64_t last_good = 0;
    while (true) {
        long start = std::ftell(r);
        if (start < 0) break;

        WalRecordHeader hdr{};
        const size_t got = std::fread(&hdr, sizeof(hdr), 1, r);
        if (got != 1) {
            break; // EOF or partial header
        }

        if (hdr.magic != kWalMagic || hdr.version != kWalVersion || hdr.header_size != sizeof(WalRecordHeader)) {
            break;
        }
        if (hdr.payload_size != PAGE_SIZE) {
            break;
        }
        if (hdr.lsn != static_cast<uint64_t>(start)) {
            break; // inconsistent LSN (offset)
        }
        if (hdr.header_crc32 != header_crc(hdr)) {
            break; // header corruption
        }

        Page page{};
        const size_t read = std::fread(page.data(), 1, PAGE_SIZE, r);
        if (read != PAGE_SIZE) {
            break; // partial payload
        }
        const uint32_t crc = crc32_ieee(reinterpret_cast<const uint8_t*>(page.data()), PAGE_SIZE);
        if (crc != hdr.payload_crc32) {
            break; // payload corruption
        }

        last_good = static_cast<uint64_t>(start) + sizeof(WalRecordHeader) + PAGE_SIZE;
    }
    return last_good;
}

Status WAL::TruncateFileTo(uint64_t size_bytes) {
    if (!file_) return Status::ERROR;
    if (std::fflush(file_) != 0) return Status::ERROR;

    int fd = fileno(file_);
    if (fd < 0) return Status::ERROR;

#if defined(_WIN32)
    // _chsize_s expects signed 64-bit size.
    if (_chsize_s(fd, static_cast<__int64>(size_bytes)) != 0) {
        return Status::ERROR;
    }
#else
    if (ftruncate(fd, static_cast<off_t>(size_bytes)) != 0) {
        return Status::ERROR;
    }
#endif
    // Move append position to end (best effort).
    std::fseek(file_, 0, SEEK_END);
    return Status::OK;
}

Status WAL::Recover(const std::function<void(page_id_t, const Page&)>& apply_fn) {
    // Open file for reading
    std::FILE* r = std::fopen(path_.c_str(), "rb");
    if (!r) {
        // no WAL file — nothing to recover
        return Status::OK;
    }

    const uint64_t last_valid = ScanLastValidOffset(r);

    // Rewind and replay exactly the valid prefix.
    std::rewind(r);
    while (static_cast<uint64_t>(std::ftell(r)) < last_valid) {
        WalRecordHeader hdr{};
        if (std::fread(&hdr, sizeof(hdr), 1, r) != 1) break;
        Page page{};
        if (std::fread(page.data(), 1, PAGE_SIZE, r) != PAGE_SIZE) break;
        apply_fn(static_cast<page_id_t>(hdr.page_id), page);
    }

    std::fclose(r);
    return Status::OK;
}

Status WAL::Truncate() {
    // Close current handle and reopen in write mode to truncate
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }

    file_ = std::fopen(path_.c_str(), "wb+");
    if (!file_) {
        std::cerr << "WAL: cannot truncate file " << path_ << ": " << std::strerror(errno) << "\n";
        return Status::ERROR;
    }

    // Ensure truncated file is durable
    if (std::fflush(file_) != 0) {
        std::cerr << "WAL: fflush after truncate failed: " << std::strerror(errno) << "\n";
        return Status::ERROR;
    }

    int fd = fileno(file_);
    if (fd < 0) return Status::ERROR;
    if (fsync(fd) != 0) {
        std::cerr << "WAL: fsync after truncate failed: " << std::strerror(errno) << "\n";
        return Status::ERROR;
    }

    next_lsn_ = 0;
    return Status::OK;
}

Status WAL::TruncateUpTo(uint64_t upto_lsn) {
    // Best-effort partial truncate. Caller must ensure upto_lsn is at record boundary.
    if (!file_) return Status::ERROR;

    // Don't extend file.
    const uint64_t current_end = next_lsn_;
    if (upto_lsn >= current_end) {
        // Equivalent to full truncate.
        return Truncate();
    }

    if (TruncateFileTo(upto_lsn) != Status::OK) {
        std::cerr << "WAL: TruncateUpTo failed for " << path_ << " at lsn=" << upto_lsn << "\n";
        return Status::ERROR;
    }

    next_lsn_ = upto_lsn;
    return Status::OK;
}
