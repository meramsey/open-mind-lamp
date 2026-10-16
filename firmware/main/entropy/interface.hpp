#pragma once
#include <cstdint>
namespace oml {
inline constexpr const char* hardware_source_name="esp32s3-hwrng-rf-no-sleep";
enum class SourceKind { EspHardware, ExternalSerial, ExternalUsb, DeterministicTest };
class EntropySource {
public:
    virtual ~EntropySource() = default;
    virtual bool read(uint32_t& word) = 0;
    virtual const char* name() const = 0;
    virtual bool experimental() const = 0;
};
}
