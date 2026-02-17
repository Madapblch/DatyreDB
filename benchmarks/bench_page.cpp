// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Page Benchmarks                                                  ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include <benchmark/benchmark.h>

#include "internal/storage/page.hpp"

using namespace datyredb::storage;

static void BM_PageConstruction(benchmark::State& state) {
    for (auto _ : state) {
        Page page(42);
        benchmark::DoNotOptimize(page);
    }
}
BENCHMARK(BM_PageConstruction);

static void BM_PageChecksum(benchmark::State& state) {
    Page page(42);
    std::memset(page.payload(), 0xAB, Page::payload_size());
    
    for (auto _ : state) {
        auto checksum = page.compute_checksum();
        benchmark::DoNotOptimize(checksum);
    }
}
BENCHMARK(BM_PageChecksum);

static void BM_PageChecksumVerify(benchmark::State& state) {
    Page page(42);
    std::memset(page.payload(), 0xAB, Page::payload_size());
    page.update_checksum();
    
    for (auto _ : state) {
        bool valid = page.verify_checksum();
        benchmark::DoNotOptimize(valid);
    }
}
BENCHMARK(BM_PageChecksumVerify);

static void BM_PagePayloadWrite(benchmark::State& state) {
    Page page(42);
    std::vector<char> data(state.range(0), 'X');
    
    for (auto _ : state) {
        std::memcpy(page.payload(), data.data(), data.size());
        benchmark::ClobberMemory();
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PagePayloadWrite)->Range(64, 4096);

BENCHMARK_MAIN();
