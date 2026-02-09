// benchmark/serialize_benchmark.cpp
#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "bench_utils.hpp"
#include "katana/core/serde.hpp"

using bench_util::clobber_memory;
using bench_util::do_not_optimize;

namespace {

template <typename Fn> void bench(const char* name, int iterations, Fn&& fn) {
    for (int i = 0; i < iterations / 10; ++i)
        fn();
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
        fn();
    auto end = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration<double, std::nano>(end - start).count();
    double ns_per = ns / iterations;
    double ops = iterations / (ns / 1e9);
    if (ns_per < 1000.0) {
        std::printf("  %-45s %8.1f ns    %12.0f ops/sec\n", name, ns_per, ops);
    } else {
        std::printf("  %-45s %8.2f us    %12.0f ops/sec\n", name, ns_per / 1000.0, ops);
    }
}

} // namespace

int main() {
    constexpr int N = 1'000'000;

    std::printf("=== Serialize Benchmark ===\n\n");

    // --- escape_json_string ---
    std::printf("--- String Escaping Performance ---\n");
    {
        std::string clean = "simple_value_without_special_chars";
        bench("escape_json_string (clean, return)", N, [&] {
            auto v = katana::serde::escape_json_string(clean);
            do_not_optimize(v);
        });
    }
    {
        std::string clean = "simple_value_without_special_chars";
        std::string buf;
        buf.reserve(64);
        bench("escape_json_string_into (clean, append)", N, [&] {
            buf.clear();
            katana::serde::escape_json_string_into(clean, buf);
            clobber_memory();
        });
    }
    {
        std::string dirty = "has \"quotes\" and \nnewline \t tab";
        bench("escape_json_string (dirty, return)", N, [&] {
            auto v = katana::serde::escape_json_string(dirty);
            do_not_optimize(v);
        });
    }
    {
        std::string dirty = "has \"quotes\" and \nnewline \t tab";
        std::string buf;
        buf.reserve(64);
        bench("escape_json_string_into (dirty, append)", N, [&] {
            buf.clear();
            katana::serde::escape_json_string_into(dirty, buf);
            clobber_memory();
        });
    }

    // --- SIMD scan performance ---
    std::printf("\n--- SIMD Scan Performance ---\n");
    {
        std::string s16(16, 'a');
        bench("needs_json_escaping (16 byte clean)", N, [&] {
            do_not_optimize(katana::serde::needs_json_escaping(s16));
        });
    }
    {
        std::string s64(64, 'a');
        bench("needs_json_escaping (64 byte clean)", N, [&] {
            do_not_optimize(katana::serde::needs_json_escaping(s64));
        });
    }
    {
        std::string s256(256, 'a');
        bench("needs_json_escaping (256 byte clean)", N, [&] {
            do_not_optimize(katana::serde::needs_json_escaping(s256));
        });
    }
    {
        std::string s64(64, 'a');
        s64[63] = '\\'; // escape at end
        bench("needs_json_escaping (64 byte, escape at end)", N, [&] {
            do_not_optimize(katana::serde::needs_json_escaping(s64));
        });
    }
    {
        std::string s64(64, 'a');
        s64[0] = '\\'; // escape at start
        bench("needs_json_escaping (64 byte, escape at start)", N, [&] {
            do_not_optimize(katana::serde::needs_json_escaping(s64));
        });
    }

    // --- Manual JSON construction simulation ---
    std::printf("\n--- JSON Object Construction ---\n");
    {
        // Simulate serialize_into with embedded commas (no first flag)
        // Reuse buffer across iterations to measure serialization, not allocation
        std::string json;
        json.reserve(200);
        bench("serialize 5-field obj (embedded commas)", N, [&] {
            json.clear();
            json.append("{\"name\":");
            json.push_back('"');
            katana::serde::escape_json_string_into("John Doe", json);
            json.push_back('"');
            json.append(",\"email\":");
            json.push_back('"');
            katana::serde::escape_json_string_into("john@example.com", json);
            json.push_back('"');
            json.append(",\"age\":");
            json.append("30");
            json.append(",\"active\":");
            json.append("true");
            json.append(",\"role\":");
            json.push_back('"');
            json.append("admin");
            json.push_back('"');
            json.push_back('}');
            do_not_optimize(json.data());
        });
    }
    {
        // Array of 100 integers — write directly into a flat char buffer
        // to avoid per-element std::string operations
        char flat[512];
        bench("serialize array 100 ints (to_chars)", N / 10, [&] {
            char* p = flat;
            *p++ = '[';
            for (int i = 0; i < 100; ++i) {
                if (i > 0)
                    *p++ = ',';
                auto [end, ec] = std::to_chars(p, flat + sizeof(flat), i);
                p = end;
            }
            *p++ = ']';
            do_not_optimize(flat);
            do_not_optimize(p);
        });
    }
    {
        // Optimized array serialization using lookup tables
        std::array<int, 100> arr;
        for (int i = 0; i < 100; ++i) {
            arr[i] = i;
        }
        std::string buf;
        buf.reserve(512);
        bench("serialize array 100 ints (optimized)", N / 10, [&] {
            buf.clear();
            katana::serde::serialize_int_array_into(arr.data(), arr.size(), buf);
            do_not_optimize(buf.data());
        });
    }
    {
        // Large array serialization test
        std::array<int, 1000> arr;
        for (int i = 0; i < 1000; ++i) {
            arr[i] = i * 7 + 13; // Mix of different digit counts
        }
        std::string buf;
        buf.reserve(8192);
        bench("serialize array 1000 ints (optimized)", N / 100, [&] {
            buf.clear();
            katana::serde::serialize_int_array_into(arr.data(), arr.size(), buf);
            do_not_optimize(buf.data());
        });
    }

    std::printf("\n=== Benchmark Complete ===\n");
    return 0;
}
