#include "../db/database.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string ExecCapture(Database& db, const std::string& cmd) {
    std::ostringstream out;
    auto* old = std::cout.rdbuf(out.rdbuf());
    const Status s = db.Execute(cmd);
    std::cout.rdbuf(old);
    // Keep status visible in case of failures.
    if (s != Status::OK) {
        out << "[STATUS=ERROR]\n";
    }
    return out.str();
}

static void RemoveIfExists(const std::string& path) {
    std::remove(path.c_str());
}

static void CorruptPage0(const std::string& db_file_path) {
    // Overwrite first page with zeros to simulate torn/stale DB file.
    std::fstream f(db_file_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) {
        std::cerr << "Cannot open DB file for corruption: " << db_file_path << "\n";
        std::exit(2);
    }
    Page zero{};
    f.seekp(0, std::ios::beg);
    f.write(zero.data(), PAGE_SIZE);
    f.flush();
}

int main() {
    const std::string db_name = "crash_smoke_db";
    const std::string wal_path = db_name + ".wal";

    // Clean start.
    RemoveIfExists(db_name);
    RemoveIfExists(wal_path);

    // "Crash" run: disable shutdown checkpoint so WAL remains.
    {
        Database db(/*enable_shutdown_checkpoint=*/false);
        ExecCapture(db, "CREATE DATABASE " + db_name + ";");
        ExecCapture(db, "USE " + db_name + ";");

        // Write a value to page 0 (SET uses page 0 in this project).
        ExecCapture(db, "SET foo bar;");
    } // no checkpoint, WAL should remain

    // Corrupt DB file so only WAL can fix it.
    CorruptPage0(db_name);

    // Restart and recover.
    {
        Database db;
        ExecCapture(db, "CREATE DATABASE " + db_name + ";"); // opens existing file; also runs recovery
        ExecCapture(db, "USE " + db_name + ";");             // also runs recovery

        const std::string got = ExecCapture(db, "GET foo;");
        if (got.find("bar") == std::string::npos) {
            std::cerr << "Crash recovery smoke test FAILED.\n";
            std::cerr << "Expected GET foo to contain 'bar'. Got:\n" << got << "\n";
            return 1;
        }
    }

    std::cout << "Crash recovery smoke test PASSED.\n";
    return 0;
}

