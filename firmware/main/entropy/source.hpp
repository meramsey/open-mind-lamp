#pragma once
#include "interface.hpp"
#include "esp_random.h"

namespace oml {
class HardwareSource final : public EntropySource {
    bool (*ready_)();
public:
    explicit HardwareSource(bool (*ready)()) : ready_(ready) {}
    bool read(uint32_t& word) override {
        if (!ready_()) return false;
        const uint32_t fresh=esp_random();
        if (!ready_()) return false; // a transition during the read is not an accepted measurement
        word=fresh; return true;
    }
    const char* name() const override { return hardware_source_name; }
    bool experimental() const override { return true; }
};
// Adapter point for future transports. They must supply framing, timeouts,
// provenance and health checks before being enabled; neither is shipped as a TRNG.
class ExternalSource : public EntropySource {
public:
    bool experimental() const override { return false; }
};
}
