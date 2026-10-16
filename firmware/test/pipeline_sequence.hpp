#pragma once
#include "entropy/interface.hpp"
#include <cstddef>
namespace diagnostic_test {
inline constexpr uint32_t words[]={0xaaaaaaab,0xaaaaaaab,0xaaaaaaa8,0xaaaaaaa8,
                                    0xaaaaaaab,0xaaaaaaa8,0xaaaaaaaa,0xaaaaaaaa};
inline constexpr const char* name="deterministic-diagnostic-TEST-ONLY";
class KnownSequence final : public oml::EntropySource {
    size_t next_=0;
public:
    bool read(uint32_t& word) override {
        if(next_==sizeof(words)/sizeof(words[0])) return false;
        word=words[next_++]; return true;
    }
    const char* name() const override { return diagnostic_test::name; }
    bool experimental() const override { return false; }
};
}
