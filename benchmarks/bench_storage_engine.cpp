// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Storage Engine Benchmarks                                        ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include <benchmark/benchmark.h>

#include "internal/core/storage_engine.hpp"

#include <filesystem>

using namespace datyredb::core;
using namespace datyredb::storage;

class StorageEngineBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State&) override {
        test_dir_ = std::filesystem::temp_directory_path() / "datyredb_engine_bench";
        std::filesystem::remove_all(test_dir_);
        
        StorageEngine::Config config;
        config.data_directory = test_dir_.string();
        config.buffer_pool_size = 10000;
        
        engine_ = std::make_unique<StorageEngine>(config);
        engine_->initialize();
    }
    
    void TearDown(benchmark::State&) override {
        engine_->shutdown();
        engine_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
protected:
    std::filesystem::path test_dir_;
    std::unique_ptr<StorageEngine> engine_;
};

BENCHMARK_DEFINE_F(StorageEngineBenchmark, CreatePage)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = engine_->create_page();
        if (result) {
            engine_->release_page((*result)->id(), false);
        }
    }
}
BENCHMARK_REGISTER_F(StorageEngineBenchmark, CreatePage);

BENCHMARK_DEFINE_F(StorageEngineBenchmark, CreateAndWritePage)(benchmark::State& state) {
    std::vector<char> data(1024, 'X');
    
    for (auto _ : state) {
        auto result = engine_->create_page();
        if (result) {
            std::memcpy((*result)->payload(), data.data(), data.size());
            engine_->release_page((*result)->id(), true);
        }
    }
    
    state.SetBytesProcessed(state.iterations() * 1024);
}
BENCHMARK_REGISTER_F(StorageEngineBenchmark, CreateAndWritePage);

BENCHMARK_DEFINE_F(StorageEngineBenchmark, Checkpoint)(benchmark::State& state) {
    // Create dirty pages
    for (int i = 0; i < 100; ++i) {
        auto result = engine_->create_page();
        if (result) {
            engine_->release_page((*result)->id(), true);
        }
    }
    
    for (auto _ : state) {
        engine_->checkpoint_sync();
        
        // Re-dirty some pages
        for (int i = 0; i < 10; ++i) {
            auto result = engine_->create_page();
            if (result) {
                engine_->release_page((*result)->id(), true);
            }
        }
    }
}
BENCHMARK_REGISTER_F(StorageEngineBenchmark, Checkpoint);

BENCHMARK_MAIN();
