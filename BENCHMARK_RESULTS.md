# KATANA Benchmark Results

> Last updated: 2026-02-08

## Summary

All benchmarks show realistic, stable measurements with proper compiler optimization barriers. The previous 0.0ns measurements (caused by compiler optimizations at `-O3`) have been fixed with `do_not_optimize()` / `clobber_memory()` barriers in `include/bench_utils.hpp`.

---

## Codegen Quality Benchmark

Tests the quality of generated parsing code.

| Operation | Latency | Throughput |
|-----------|---------|------------|
| parse_int64 (simple) | 6.2 ns | 163M ops/sec |
| parse_int64 (negative) | 7.4 ns | 135M ops/sec |
| parse_double | 15.7 ns | 64M ops/sec |
| parse_bool (strict validation) | 2.1 ns | 483M ops/sec |
| needs_json_escaping (clean, 58 chars) | 7.8 ns | 129M ops/sec |
| needs_json_escaping (dirty, 25 chars) | 0.7 ns | 1454M ops/sec |
| escape_json_string (no-alloc path) | 11.6 ns | 86M ops/sec |
| escape_json_string_into (append) | 20.2 ns | 50M ops/sec |
| skip_value (nested obj with strings) | 27.5 ns | 36M ops/sec |
| 3-field object parse (linear) | 36.6 ns | 27M ops/sec |
| 8-field object parse (length-switch) | 64.0 ns | 16M ops/sec |
| arena alloc+reset cycle (4KB) | 41.7 ns | 24M ops/sec |

---

## Serialize Benchmark

Tests JSON serialization performance.

| Operation | Latency | Throughput |
|-----------|---------|------------|
| escape_json_string (clean, return) | 21.2 ns | 47M ops/sec |
| escape_json_string_into (clean, append) | 80.2 ns | 12M ops/sec |
| escape_json_string (dirty, return) | 82.2 ns | 12M ops/sec |
| escape_json_string_into (dirty, append) | 66.5 ns | 15M ops/sec |
| needs_json_escaping (16 byte clean) | 2.5 ns | 407M ops/sec |
| needs_json_escaping (64 byte clean) | 5.3 ns | 187M ops/sec |
| needs_json_escaping (256 byte clean) | 15.3 ns | 65M ops/sec |
| needs_json_escaping (64 byte, escape at end) | 3.5 ns | 284M ops/sec |
| needs_json_escaping (64 byte, escape at start) | 1.4 ns | 712M ops/sec |
| serialize 5-field obj (embedded commas) | 75.8 ns | 13M ops/sec |
| serialize array 100 ints (single alloc) | 705.5 ns | 1.4M ops/sec |

---

## JSON Encoder/Decoder Benchmark

| Operation | Throughput |
|-----------|------------|
| JSON String Encoding (Small, 5 bytes) | 34.5M ops/sec |
| JSON String Encoding (Medium, 80 bytes) | 29.4M ops/sec |
| JSON String Encoding (Large, 1000 bytes) | 15.5M ops/sec |
| JSON Object Serialization | 6.0M ops/sec |
| JSON Array (5 elements) | 10.7M ops/sec |
| JSON Array (100 elements) | 0.5M ops/sec |
| Number to String Conversion | 41.8M ops/sec |

---

## Router Benchmark

| Scenario | Throughput | Latency p50 | Latency p99 |
|----------|------------|-------------|-------------|
| Dispatch (hits) | 794K ops/sec | 0.64 us | 1.05 us |
| Dispatch (not found) | 858K ops/sec | 0.67 us | 0.85 us |
| Dispatch (405 Method Not Allowed) | 709K ops/sec | 0.91 us | 1.11 us |

---

## Performance Benchmark (Core Runtime)

| Component | Throughput | Latency p50 | Latency p99 |
|-----------|------------|-------------|-------------|
| Ring Buffer Queue (Single Thread) | 333M ops/sec | 0.003 us | 0.006 us |
| Ring Buffer Queue (Concurrent 4x4) | 4.0M ops/sec | - | - |
| Ring Buffer Queue (High Contention 8x8) | 3.1M ops/sec | - | - |
| Circular Buffer | 500M ops/sec | 0.003 us | 0.006 us |
| SIMD CRLF Search (1.5KB) | 100M ops/sec | 0.012 us | 0.019 us |
| SIMD CRLF Search (16KB) | 3.8M ops/sec | 0.25 us | 0.50 us |
| HTTP Parser (Complete Request) | 1.4M ops/sec | 0.63 us | 1.24 us |
| HTTP Parser (Fragmented Request) | 1.5M ops/sec | 0.61 us | 1.15 us |
| Arena Allocations (64B objects) | 7.9M ops/sec | - | - |
| Memory Allocations (String Queue) | 100M ops/sec | - | - |

---

## Key Observations

1. **Parsing Performance**: Integer parsing achieves 135-163M ops/sec, boolean parsing reaches 483M ops/sec with strict validation.

2. **SIMD Optimizations**: `needs_json_escaping` uses SIMD vectorization for fast string scanning, achieving up to 712M ops/sec for early-escape detection.

3. **Arena Allocations**: Arena alloc+reset cycles complete in ~42ns, providing predictable memory management without heap fragmentation.

4. **Router Dispatch**: Zero-allocation dispatch with 700K-850K ops/sec throughput and sub-microsecond p50 latencies.

5. **HTTP Parser**: Parses complete HTTP requests at 1.4M ops/sec with p99 latency under 1.3 microseconds.

---

## Running Benchmarks

```bash
# Build benchmark targets
cmake --preset bench
cmake --build build/bench -j$(nproc)

# Run individual benchmarks
./build/bench/benchmark/codegen_quality_benchmark
./build/bench/benchmark/serialize_benchmark
./build/bench/benchmark/json_benchmark
./build/bench/benchmark/router_benchmark
./build/bench/benchmark/performance_benchmark
```
