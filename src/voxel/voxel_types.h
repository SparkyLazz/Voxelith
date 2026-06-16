#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace vox {

struct UUID {
    std::array<uint8_t, 16> bytes{};

    static UUID generate();        // thread-local mt19937_64, seeded once per thread
    std::string to_string() const; // 8-4-4-4-12 lowercase hex

    bool operator==(const UUID&) const = default;
};

} // namespace vox

namespace std {

// glm::ivec3 hash — boost-style hash_combine with the golden-ratio constant
// 0x9e3779b9, which minimises clustering on low-entropy integer coordinates.
template <>
struct hash<glm::ivec3> {
    size_t operator()(const glm::ivec3& v) const noexcept {
        size_t seed = 0;
        auto combine = [&seed](int val) {
            seed ^= hash<int>{}(val) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        };
        combine(v.x);
        combine(v.y);
        combine(v.z);
        return seed;
    }
};

template <>
struct hash<vox::UUID> {
    size_t operator()(const vox::UUID& u) const noexcept {
        size_t seed = 0;
        for (auto b : u.bytes) {
            seed ^= hash<uint8_t>{}(b) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

} // namespace std
