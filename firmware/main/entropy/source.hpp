#pragma once
#include "interface.hpp"
#include "esp_random.h"

namespace oml {
class HardwareSource final : public EntropySource {
    bool (*ready_)();
public:
    explicit HardwareSource(bool (*ready)()) : ready_(ready) {}
    bool read(uint32_t& word) override { if (!ready_()) return false; word=esp_random(); return true; }
    const char* name() const override { return "esp32s3-hwrng-rf-no-sleep"; }
    bool experimental() const override { return true; }
};
// Adapter point for future transports. They must supply framing, timeouts,
// provenance and health checks before being enabled; neither is shipped as a TRNG.
class ExternalSource : public EntropySource {
public:
    bool experimental() const override { return false; }
};
}
