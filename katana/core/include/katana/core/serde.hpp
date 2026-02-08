#pragma once

#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#ifdef __SSE2__
#include <emmintrin.h> // SSE2
#endif

namespace katana::serde {

// Fast JSON whitespace detection (space, tab, newline, carriage return)
// Lookup table is faster than std::isspace (avoids function call and locale checks)
constexpr bool is_json_whitespace_table[256] = {
    false, false, false, false, false, false, false, false, false, true,  true,  false, false,
    true,  false, false, // 0x09=tab, 0x0A=\n, 0x0D=\r
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, true,  false, false, false, false, false, false, false, // 0x20=space
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
};

inline constexpr bool is_json_whitespace(unsigned char c) noexcept {
    return is_json_whitespace_table[c];
}

inline std::string_view trim_view(std::string_view sv) noexcept {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

struct json_cursor {
    const char* ptr;
    const char* end;
    const char* start; // Track start for position calculation

    json_cursor(const char* p, const char* e) : ptr(p), end(e), start(p) {}

    bool eof() const noexcept { return ptr >= end; }

    size_t pos() const noexcept { return static_cast<size_t>(ptr - start); }

    void skip_ws() noexcept {
        // Fast path: if no whitespace at current position, return immediately
        // This is critical for compact JSON where whitespace is minimal
        if (eof() || !is_json_whitespace(static_cast<unsigned char>(*ptr))) {
            return;
        }

#ifdef __SSE2__
        // SIMD path: only use for large amounts of whitespace (8+ chars)
        // This amortizes the SIMD setup overhead
        constexpr size_t simd_threshold = 8;
        constexpr size_t simd_width = 16;

        // Quick scalar skip for small whitespace runs
        const char* scan_ptr = ptr;
        size_t count = 0;
        while (count < simd_threshold && scan_ptr < end &&
               is_json_whitespace(static_cast<unsigned char>(*scan_ptr))) {
            ++scan_ptr;
            ++count;
        }

        // If less than threshold, use scalar result
        if (count < simd_threshold) {
            ptr = scan_ptr;
            return;
        }

        // We have significant whitespace - use SIMD
        ptr = scan_ptr; // Start from where scalar left off

        while (static_cast<size_t>(end - ptr) >= simd_width) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));

            __m128i space = _mm_set1_epi8(' ');
            __m128i tab = _mm_set1_epi8('\t');
            __m128i lf = _mm_set1_epi8('\n');
            __m128i cr = _mm_set1_epi8('\r');

            __m128i eq_space = _mm_cmpeq_epi8(chunk, space);
            __m128i eq_tab = _mm_cmpeq_epi8(chunk, tab);
            __m128i eq_lf = _mm_cmpeq_epi8(chunk, lf);
            __m128i eq_cr = _mm_cmpeq_epi8(chunk, cr);

            __m128i is_ws =
                _mm_or_si128(_mm_or_si128(eq_space, eq_tab), _mm_or_si128(eq_lf, eq_cr));

            int mask = _mm_movemask_epi8(is_ws);

            if (mask == 0) {
                break;
            }

            if (mask == 0xFFFF) {
                ptr += simd_width;
                continue;
            }

            int first_non_ws = __builtin_ctz(~mask & 0xFFFF);
            ptr += first_non_ws;
            return;
        }
#endif

        // Scalar fallback for remaining bytes
        while (!eof() && is_json_whitespace(static_cast<unsigned char>(*ptr))) {
            ++ptr;
        }
    }

    bool consume(char c) noexcept {
        skip_ws();
        if (eof() || *ptr != c) {
            return false;
        }
        ++ptr;
        return true;
    }

    std::optional<std::string_view> string() noexcept {
        skip_ws();
        if (eof() || *ptr != '\"') {
            return std::nullopt;
        }
        ++ptr;
        const char* str_start = ptr;
        while (!eof() && *ptr != '\"') {
            if (*ptr == '\\' && (ptr + 1) < end) {
                ptr += 2;
                continue;
            }
            ++ptr;
        }
        if (eof()) {
            return std::nullopt;
        }
        const char* stop = ptr;
        ++ptr; // consume closing quote
        return std::string_view(str_start, static_cast<size_t>(stop - str_start));
    }

    bool try_object_start() noexcept { return consume('{'); }
    bool try_object_end() noexcept { return consume('}'); }
    bool try_array_start() noexcept { return consume('['); }
    bool try_array_end() noexcept { return consume(']'); }
    bool try_comma() noexcept { return consume(','); }

    void skip_value() noexcept {
        skip_ws();
        if (eof())
            return;
        char ch = *ptr;
        if (ch == '{' || ch == '[') {
            char open = ch;
            char close = (ch == '{') ? '}' : ']';
            ++ptr;
            int depth = 1;
            while (!eof() && depth > 0) {
                ch = *ptr;
                if (ch == '\"') {
                    // Skip over string content (handles \" inside strings)
                    ++ptr;
                    while (!eof()) {
                        if (*ptr == '\\' && (ptr + 1) < end) {
                            ptr += 2;
                            continue;
                        }
                        if (*ptr == '\"') {
                            ++ptr;
                            break;
                        }
                        ++ptr;
                    }
                } else if (ch == open) {
                    ++depth;
                    ++ptr;
                } else if (ch == close) {
                    --depth;
                    ++ptr;
                } else {
                    ++ptr;
                }
            }
            return;
        }
        if (ch == '\"') {
            (void)string();
            return;
        }
        while (!eof() && *ptr != ',' && *ptr != '}' && *ptr != ']') {
            ++ptr;
        }
    }
};

inline std::optional<size_t> parse_size(json_cursor& cur) noexcept {
    cur.skip_ws();
    if (cur.eof()) {
        return std::nullopt;
    }
    if (*cur.ptr == '\"') {
        if (auto sv = cur.string()) {
            size_t value = 0;
            auto fc = std::from_chars(sv->data(), sv->data() + sv->size(), value);
            if (fc.ec == std::errc()) {
                return value;
            }
        }
        return std::nullopt;
    }
    const char* start = cur.ptr;
    const char* p = start;
    if (p < cur.end && (*p == '+' || *p == '-')) {
        ++p;
    }
    while (p < cur.end && std::isdigit(static_cast<unsigned char>(*p))) {
        ++p;
    }
    if (p == start) {
        return std::nullopt;
    }
    size_t value = 0;
    auto fc = std::from_chars(start, p, value);
    if (fc.ec != std::errc()) {
        return std::nullopt;
    }
    cur.ptr = p;
    return value;
}

inline std::optional<int64_t> parse_int64(json_cursor& cur) noexcept {
    cur.skip_ws();
    if (cur.eof()) {
        return std::nullopt;
    }
    if (*cur.ptr == '\"') {
        if (auto sv = cur.string()) {
            int64_t value = 0;
            auto fc = std::from_chars(sv->data(), sv->data() + sv->size(), value);
            if (fc.ec == std::errc()) {
                return value;
            }
        }
        return std::nullopt;
    }
    const char* start = cur.ptr;
    const char* p = start;
    if (p < cur.end && (*p == '+' || *p == '-')) {
        ++p;
    }
    while (p < cur.end && std::isdigit(static_cast<unsigned char>(*p))) {
        ++p;
    }
    if (p == start || (p == start + 1 && (*start == '+' || *start == '-'))) {
        return std::nullopt;
    }
    int64_t value = 0;
    auto fc = std::from_chars(start, p, value);
    if (fc.ec != std::errc()) {
        return std::nullopt;
    }
    cur.ptr = p;
    return value;
}

inline std::optional<double> parse_double(json_cursor& cur) noexcept {
    cur.skip_ws();
    if (cur.eof()) {
        return std::nullopt;
    }
    if (*cur.ptr == '\"') {
        if (auto sv = cur.string()) {
            double val = 0.0;
            auto [p, ec] = std::from_chars(sv->data(), sv->data() + sv->size(), val);
            if (ec == std::errc()) {
                return val;
            }
        }
        return std::nullopt;
    }
    // Fast path: use std::from_chars instead of std::strtod (2-3x faster)
    const char* start = cur.ptr;
    double v = 0.0;
    auto [p, ec] = std::from_chars(start, cur.end, v);
    if (ec != std::errc() || p == start) {
        return std::nullopt;
    }
    cur.ptr = p;
    return v;
}

inline std::optional<bool> parse_bool(json_cursor& cur) noexcept {
    cur.skip_ws();
    if (cur.eof()) {
        return std::nullopt;
    }
    if (*cur.ptr == 't') {
        if (cur.end - cur.ptr >= 4 && cur.ptr[1] == 'r' && cur.ptr[2] == 'u' && cur.ptr[3] == 'e') {
            cur.ptr += 4;
            return true;
        }
        return std::nullopt;
    }
    if (*cur.ptr == 'f') {
        if (cur.end - cur.ptr >= 5 && cur.ptr[1] == 'a' && cur.ptr[2] == 'l' && cur.ptr[3] == 's' &&
            cur.ptr[4] == 'e') {
            cur.ptr += 5;
            return false;
        }
        return std::nullopt;
    }
    if (*cur.ptr == '\"') {
        if (auto sv = cur.string()) {
            auto v = trim_view(*sv);
            if (v == "true") {
                return true;
            }
            if (v == "false") {
                return false;
            }
        }
    }
    return std::nullopt;
}

inline std::string_view parse_unquoted_string(json_cursor& cur) {
    cur.skip_ws();
    const char* start = cur.ptr;
    while (!cur.eof() && *cur.ptr != ',' && *cur.ptr != '}' && *cur.ptr != ']') {
        ++cur.ptr;
    }
    const char* end = cur.ptr;
    auto sv = std::string_view(start, static_cast<size_t>(end - start));
    return trim_view(sv);
}

inline bool is_bool_literal(std::string_view sv) noexcept {
    return sv == "true" || sv == "false";
}
inline bool is_null_literal(std::string_view sv) noexcept {
    return sv == "null";
}

inline bool needs_json_escaping(std::string_view sv) noexcept {
#ifdef __SSE2__
    const char* ptr = sv.data();
    const char* end = ptr + sv.size();
    const __m128i backslash = _mm_set1_epi8('\\');
    const __m128i quote = _mm_set1_epi8('\"');
    const __m128i control_max = _mm_set1_epi8(0x1F);
    while (end - ptr >= 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        __m128i eq_bs = _mm_cmpeq_epi8(chunk, backslash);
        __m128i eq_qt = _mm_cmpeq_epi8(chunk, quote);
        // Check for control characters (bytes <= 0x1F): compare unsigned via max
        __m128i is_ctrl = _mm_cmpeq_epi8(_mm_min_epu8(chunk, control_max), chunk);
        __m128i needs = _mm_or_si128(_mm_or_si128(eq_bs, eq_qt), is_ctrl);
        if (_mm_movemask_epi8(needs) != 0) {
            return true;
        }
        ptr += 16;
    }
    while (ptr < end) {
        unsigned char c = static_cast<unsigned char>(*ptr);
        if (c == '\\' || c == '\"' || c <= 0x1F) {
            return true;
        }
        ++ptr;
    }
    return false;
#elif defined(__ARM_NEON) || defined(__aarch64__)
    const char* ptr = sv.data();
    const char* end = ptr + sv.size();
    const uint8x16_t backslash = vdupq_n_u8('\\');
    const uint8x16_t quote = vdupq_n_u8('\"');
    const uint8x16_t control_max = vdupq_n_u8(0x1F);
    while (end - ptr >= 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(ptr));
        uint8x16_t eq_bs = vceqq_u8(chunk, backslash);
        uint8x16_t eq_qt = vceqq_u8(chunk, quote);
        uint8x16_t is_ctrl = vcleq_u8(chunk, control_max);
        uint8x16_t needs = vorrq_u8(vorrq_u8(eq_bs, eq_qt), is_ctrl);
        if (vmaxvq_u8(needs) != 0) {
            return true;
        }
        ptr += 16;
    }
    while (ptr < end) {
        unsigned char c = static_cast<unsigned char>(*ptr);
        if (c == '\\' || c == '\"' || c <= 0x1F) {
            return true;
        }
        ++ptr;
    }
    return false;
#else
    for (char c : sv) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc == '\\' || uc == '\"' || uc <= 0x1F) {
            return true;
        }
    }
    return false;
#endif
}

inline void escape_json_string_into(std::string_view sv, std::string& out) {
    for (char c : sv) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '\"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                // Escape other control characters as \u00XX
                static constexpr char hex[] = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[(static_cast<unsigned char>(c) >> 4) & 0xF]);
                out.push_back(hex[static_cast<unsigned char>(c) & 0xF]);
            } else {
                out.push_back(c);
            }
            break;
        }
    }
}

inline std::string escape_json_string(std::string_view sv) {
    if (!needs_json_escaping(sv)) {
        return std::string(sv);
    }
    std::string out;
    out.reserve(sv.size() + 8);
    escape_json_string_into(sv, out);
    return out;
}

} // namespace katana::serde
