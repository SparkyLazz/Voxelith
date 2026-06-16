#include <cstdint>
#include <cstring>
#include <random>

#include "voxel/voxel_types.h"

namespace vox {

UUID UUID::generate() {
    thread_local auto rng = []() -> std::mt19937_64 {
        std::random_device rd;
        return std::mt19937_64(rd());
    }();

    UUID uuid{};
    const uint64_t hi = rng();
    const uint64_t lo = rng();
    std::memcpy(uuid.bytes.data(),     &hi, 8);
    std::memcpy(uuid.bytes.data() + 8, &lo, 8);
    return uuid;
}

std::string UUID::to_string() const {
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(36);

    auto append = [&](size_t start, size_t count) {
        for (size_t i = start; i < start + count; ++i) {
            result += hex[(bytes[i] >> 4) & 0xFu];
            result += hex[bytes[i] & 0xFu];
        }
    };

    append(0,  4); result += '-';
    append(4,  2); result += '-';
    append(6,  2); result += '-';
    append(8,  2); result += '-';
    append(10, 6);

    return result;
}

} // namespace vox
