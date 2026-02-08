# KATANA Benchmark Results

> Last updated: 2026-02-09

## Summary

All benchmarks show realistic, stable measurements with proper compiler optimization barriers (`do_not_optimize()` / `clobber_memory()` in `include/bench_utils.hpp`). Performance-critical paths have been optimized with hand-rolled parsers, SIMD-accelerated string processing, and lock-free concurrent data structures.

### Key Improvements (vs. baseline)

| Metric | Before | After | Speedup |
|--------|--------|-------|---------|
| parse_int64 | 7.4 ns (135M) | 5.2 ns (194M) | **1.4x** |
| parse_double | 15.7 ns (64M) | 12.8 ns (78M) | **1.2x** |
| parse_bool | 2.1 ns (483M) | 1.1 ns (910M) | **1.9x** |
| escape_json_string_into (clean) | 80.2 ns (12M) | 4.9 ns (204M) | **16x** |
| serialize 5-field obj | 75.8 ns (13M) | 42.4 ns (24M) | **1.8x** |
| Router dispatch (hits) | 794K ops/sec | 1.08M ops/sec | **1.35x** |
| Ring Buffer 4x4 concurrent | 4.0M ops/sec | 13.2M ops/sec | **3.3x** |
| Ring Buffer 8x8 contention | 3.1M ops/sec | 9.3M ops/sec | **3.0x** |

---

## Codegen Quality Benchmark

Tests the quality of generated parsing code.

| Operation | Latency | Throughput |
|-----------|---------|------------|
| parse_int64 (simple) | 5.2 ns | 194M ops/sec |
| parse_int64 (negative) | 6.2 ns | 161M ops/sec |
| parse_double | 12.8 ns | 78M ops/sec |
| parse_bool (strict validation) | 1.1 ns | 910M ops/sec |
| needs_json_escaping (clean, 58 chars) | 9.4 ns | 107M ops/sec |
| needs_json_escaping (dirty, 25 chars) | 0.9 ns | 1124M ops/sec |
| escape_json_string (no-alloc path) | 11.4 ns | 88M ops/sec |
| escape_json_string_into (append) | 4.9 ns | 204M ops/sec |
| skip_value (nested obj with strings) | 30.9 ns | 32M ops/sec |
| 3-field object parse (linear) | 50.3 ns | 20M ops/sec |
| 8-field object parse (length-switch) | 54.2 ns | 18M ops/sec |
| arena alloc+reset cycle (4KB) | 34.1 ns | 29M ops/sec |

---

## Serialize Benchmark

Tests JSON serialization performance.

| Operation | Latency | Throughput |
|-----------|---------|------------|
| escape_json_string (clean, return) | 21.4 ns | 47M ops/sec |
| escape_json_string_into (clean, append) | 9.1 ns | 110M ops/sec |
| escape_json_string (dirty, return) | 54.5 ns | 18M ops/sec |
| escape_json_string_into (dirty, append) | 41.1 ns | 24M ops/sec |
| needs_json_escaping (16 byte clean) | 2.3 ns | 443M ops/sec |
| needs_json_escaping (64 byte clean) | 5.1 ns | 198M ops/sec |
| needs_json_escaping (256 byte clean) | 15.2 ns | 66M ops/sec |
| needs_json_escaping (64 byte, escape at end) | 4.0 ns | 247M ops/sec |
| needs_json_escaping (64 byte, escape at start) | 1.2 ns | 811M ops/sec |
| serialize 5-field obj (embedded commas) | 42.4 ns | 24M ops/sec |
| serialize array 100 ints (single alloc) | 646.3 ns | 1.5M ops/sec |

---

## JSON Encoder/Decoder Benchmark

| Operation | Throughput |
|-----------|------------|
| JSON String Encoding (Small, 5 bytes) | 31.7M ops/sec |
| JSON String Encoding (Medium, 80 bytes) | 23.5M ops/sec |
| JSON String Encoding (Large, 1000 bytes) | 15.5M ops/sec |
| JSON Object Serialization | 5.6M ops/sec |
| JSON Array (5 elements) | 10.0M ops/sec |
| JSON Array (100 elements) | 0.4M ops/sec |
| Number to String Conversion | 40.9M ops/sec |

---

## Router Benchmark

| Scenario | Throughput | Latency p50 | Latency p99 |
|----------|------------|-------------|-------------|
| Dispatch (hits) | 1.08M ops/sec | 0.45 us | 0.93 us |
| Dispatch (not found) | 1.20M ops/sec | 0.36 us | 0.71 us |
| Dispatch (405 Method Not Allowed) | 873K ops/sec | 0.57 us | 1.02 us |

---

## Performance Benchmark (Core Runtime)

| Component | Throughput | Latency p50 | Latency p99 |
|-----------|------------|-------------|-------------|
| Ring Buffer Queue (Single Thread) | 250M ops/sec | 0.005 us | 0.005 us |
| Ring Buffer Queue (Concurrent 4x4) | 13.2M ops/sec | - | - |
| Ring Buffer Queue (High Contention 8x8) | 9.3M ops/sec | - | - |
| Circular Buffer | 500M ops/sec | 0.003 us | 0.006 us |
| SIMD CRLF Search (1.5KB) | 100M ops/sec | 0.012 us | 0.020 us |
| SIMD CRLF Search (16KB) | 3.8M ops/sec | 0.25 us | 0.47 us |
| HTTP Parser (Complete Request) | 1.6M ops/sec | 0.62 us | 1.12 us |
| HTTP Parser (Fragmented Request) | 1.6M ops/sec | 0.61 us | 1.08 us |
| Arena Allocations (64B objects) | 6.6M ops/sec | - | - |
| Memory Allocations (String Queue) | 100M ops/sec | - | - |

---

## Optimization Techniques

1. **Hand-rolled Integer Parser**: Custom digit-by-digit accumulation in `parse_int64` avoids `from_chars` overhead, achieving 194M ops/sec (1.4x improvement).

2. **Word-level Boolean Comparison**: `parse_bool` uses `memcmp` for 4/5-byte word comparison instead of byte-by-byte, reaching 910M ops/sec (1.9x improvement).

3. **SIMD String Escaping**: `escape_json_string_into` uses SSE2 vectorized scanning with a fast path for clean strings (single bulk `append`), achieving 204M ops/sec (16x improvement).

4. **Lock-free Ring Buffer**: Removed spin-wait overhead in MPMC operations, using relaxed CAS ordering and `_mm_pause()` CPU hints instead of `std::this_thread::yield()`, reaching 13M ops/sec concurrent (3.3x improvement).

5. **Router Fast Path**: Early segment-count filtering and pointer-based path splitting reduce dispatch overhead, achieving 1.08M ops/sec (1.35x improvement).

6. **Fast Number Serialization**: `std::to_chars` replaces `std::to_string` in JSON object/array construction for lower-overhead integer formatting.

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
