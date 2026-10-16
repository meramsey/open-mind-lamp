#pragma once
#include "color.hpp"
namespace oml {
enum class TestColor : uint8_t { Red, Green, Blue, White, Yellow, Cyan, Magenta, Other };
const char* color_name(TestColor);
bool parse_color(const char*, TestColor&);
enum class CalibrationPhase { Idle, Identify, Verify, Ready, Saved, Cancelled, TimedOut, Inconclusive, Interrupted };
class Calibration {
    CalibrationPhase phase_ = CalibrationPhase::Idle;
    bool original_order_ = false, candidate_order_ = false, presented_ = false;
    TestColor observed_[3]{};
    uint32_t run_ = 0, step_ = 0, index_ = 0;
    int64_t touched_us_ = 0;
public:
    static constexpr int64_t timeout_us=60000000;
    static constexpr double brightness=0.02;
    bool active() const;
    bool ready() const { return phase_==CalibrationPhase::Ready; }
    bool presented() const { return presented_; }
    uint32_t run() const { return run_; }
    uint32_t step() const { return step_; }
    uint32_t index() const { return index_; }
    bool candidate_order() const { return candidate_order_; }
    const char* phase_name() const;
    TestColor expected() const;
    bool start(bool saved_order, int64_t now_us);
    void presented(uint32_t run, uint32_t step);
    bool observe(uint32_t run, uint32_t step, TestColor seen, int64_t now_us);
    bool matches(uint32_t run, uint32_t step) const;
    void saved() { if (ready()) phase_=CalibrationPhase::Saved; }
    void cancel() { if (active()) phase_=CalibrationPhase::Cancelled; }
    void interrupt() { if (active()) phase_=CalibrationPhase::Interrupted; }
    void expire(int64_t now_us);
    int64_t remaining_ms(int64_t now_us) const;
    ColorFrame frame(bool safe_to_light) const;
};
}
