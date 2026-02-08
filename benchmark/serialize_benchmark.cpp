// benchmark/serialize_benchmark.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "katana/core/serde.hpp"

namespace {

template<typename Fn>
void bench(const char* name, int iterations, Fn&& fn) {
    for (int i = 0; i < iterations / 10; ++i) fn();
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) fn();
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
            (void)v;
        });
    }
    {
        std::string clean = "simple_value_without_special_chars";
        std::string buf;
        buf.reserve(64);
        bench("escape_json_string_into (clean, append)", N, [&] {
            buf.clear();
            katana::serde::escape_json_string_into(clean, buf);
        });
    }
    {
        std::string dirty = "has \"quotes\" and \nnewline \t tab";
        bench("escape_json_string (dirty, return)", N, [&] {
            auto v = katana::serde::escape_json_string(dirty);
            (void)v;
        });
    }
    {
        std::string dirty = "has \"quotes\" and \nnewline \t tab";
        std::string buf;
        buf.reserve(64);
        bench("escape_json_string_into (dirty, append)", N, [&] {
            buf.clear();
            katana::serde::escape_json_string_into(dirty, buf);
        });
    }

    // --- SIMD scan performance ---
    std::printf("\n--- SIMD Scan Performance ---\n");
    {
        std::string s16(16, 'a');
        bench("needs_json_escaping (16 byte clean)", N, [&] {
            (void)katana::serde::needs_json_escaping(s16);
        });
    }
    {
        std::string s64(64, 'a');
        bench("needs_json_escaping (64 byte clean)", N, [&] {
            (void)katana::serde::needs_json_escaping(s64);
        });
    }
    {
        std::string s256(256, 'a');
        bench("needs_json_escaping (256 byte clean)", N, [&] {
            (void)katana::serde::needs_json_escaping(s256);
        });
    }
    {
        std::string s64(64, 'a');
        s64[63] = '\\'; // escape at end
        bench("needs_json_escaping (64 byte, escape at end)", N, [&] {
            (void)katana::serde::needs_json_escaping(s64);
        });
    }
    {
        std::string s64(64, 'a');
        s64[0] = '\\'; // escape at start
        bench("needs_json_escaping (64 byte, escape at start)", N, [&] {
            (void)katana::serde::needs_json_escaping(s64);
        });
    }

    // --- Manual JSON construction simulation ---
    std::printf("\n--- JSON Object Construction ---\n");
    {
        // Simulate serialize_into with embedded commas (no first flag)
        bench("serialize 5-field obj (embedded commas)", N, [&] {
            std::string json;
            json.reserve(200);
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
        });
    }
    {
        // Array of 100 integers
        bench("serialize array 100 ints (single alloc)", N / 10, [&] {
            std::string json;
            json.reserve(512);
            json.push_back('[');
            for (int i = 0; i < 100; ++i) {
                if (i > 0) json.push_back(',');
                char buf[16];
                auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), i);
                json.append(buf, static_cast<size_t>(ptr - buf));
            }
            json.push_back(']');
        });
    }

    std::printf("\n=== Benchmark Complete ===\n");
    return 0;
}
