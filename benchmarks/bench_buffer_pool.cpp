// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Buffer Pool Benchmarks                                           ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include <benchmark/benchmark.h>

#include "internal/storage/buffer_pool.hpp"
#include "internal/storage/disk_manager.hpp"

#include <filesystem>
#include <memory>

using namespace datyredb::storage;

class BufferPoolBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override {
        test_dir_ = std::filesystem::temp_directory_path() / "datyredb_bench";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
        
        metrics_ = std::make_shared<CheckpointMetrics>();
        disk_manager_ = std::make_shared<DiskManager>(test_dir_);
        disk_manager_->open();
        
        buffer_pool_ = std::make_shared<BufferPool>(
            static_cast<std::size_t>(state.range(0)),
            disk_manager_,
            metrics_
        );
    }
    
    void TearDown(benchmark::State&) override {
        buffer_pool_.reset();
        disk_manager_->close();
        disk_manager_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
protected:
    std::filesystem::path test_dir_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    std::shared_ptr<DiskManager> disk_manager_;
    std::shared_ptr<BufferPool> buffer_pool_;
};

BENCHMARK_DEFINE_F(BufferPoolBenchmark, NewPage)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = buffer_pool_->new_page();
        if (result) {
            buffer_pool_->unpin_page((*result)->id(), false);
        }
    }
}
BENCHMARK_REGISTER_F(BufferPoolBenchmark, NewPage)->Arg(1000)->Arg(10000);

BENCHMARK_DEFINE_F(BufferPoolBenchmark, FetchPage)(benchmark::State& state) {
    // Pre-create pages
    std::vector<PageId> page_ids;
    for (int i = 0; i < 100; ++i) {
        auto result = buffer_pool_->new_page();
        if (result) {
            page_ids.push_back((*result)->id());
            buffer_pool_->unpin_page((*result)->id(), true);
        }
    }
    buffer_pool_->flush_pages(page_ids);
    
    std::size_t idx = 0;
    for (auto _ : state) {
        PageId page_id = page_ids[idx % page_ids.size()];
        auto result = buffer_pool_->fetch_page(page_id);
        if (result) {
            buffer_pool_->unpin_page(page_id, false);
        }
        ++idx;
    }
}
BENCHMARK_REGISTER_F(BufferPoolBenchmark, FetchPage)->Arg(1000);

BENCHMARK_MAIN();
