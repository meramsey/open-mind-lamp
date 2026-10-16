#pragma once
#include <cstdio>
#include "storage/settings.hpp"
namespace oml {
class Session {
    FILE* file_ = nullptr;
    bool ready_ = false, io_failed_ = false;
    uint32_t count_ = 0, planned_ = 0;
    uint64_t ones_ = 0;
    int64_t start_ = 0;
    char id_[33] = {};
    char reason_[32] = "none";
    bool sync();
public:
    void init();
    bool start(const cJSON* request, const Settings&, const char* source, double starting_hue);
    void sample(uint32_t word, int64_t now, uint64_t late_us, bool healthy);
    void stop(const char* reason);
    bool clear();
    bool active() const { return file_ != nullptr; }
    bool available() const;
    cJSON* status() const;
    static constexpr const char* path = "/sessions/trial.ndjson";
};
}
