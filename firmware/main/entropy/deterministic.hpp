#pragma once
#include "interface.hpp"
namespace oml {
// TEST ONLY. Never selectable by the firmware API or included in production source.
class DeterministicTestSource final : public EntropySource {
    uint64_t state_;
public:
    explicit DeterministicTestSource(uint64_t seed) : state_(seed) {}
    bool read(uint32_t& word) override { word=next(); return true; }
    const char* name() const override { return "deterministic-splitmix64-TEST-ONLY"; }
    bool experimental() const override { return false; }
    uint32_t next() {
        uint64_t z = (state_ += UINT64_C(0x9e3779b97f4a7c15));
        z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
        return static_cast<uint32_t>((z ^ (z >> 31)) >> 32);
    }
};
}
