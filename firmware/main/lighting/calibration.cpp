#include "calibration.hpp"
#include <algorithm>
#include <cstring>
namespace oml {
static constexpr const char* names[]={"red","green","blue","white","yellow","cyan","magenta","other"};
const char* color_name(TestColor c) { return names[static_cast<unsigned>(c)]; }
bool parse_color(const char* text, TestColor& output) {
    if (!text) return false;
    for (unsigned i=0;i<8;++i) if (!std::strcmp(text,names[i])) { output=static_cast<TestColor>(i); return true; }
    return false;
}
bool Calibration::active() const {
    return phase_==CalibrationPhase::Identify || phase_==CalibrationPhase::Verify || ready();
}
const char* Calibration::phase_name() const {
    switch (phase_) {
    case CalibrationPhase::Identify: return "identify";
    case CalibrationPhase::Verify: return "verify";
    case CalibrationPhase::Ready: return "ready";
    case CalibrationPhase::Saved: return "saved";
    case CalibrationPhase::Cancelled: return "cancelled";
    case CalibrationPhase::TimedOut: return "timed_out";
    case CalibrationPhase::Inconclusive: return "inconclusive";
    case CalibrationPhase::Interrupted: return "interrupted";
    default: return "idle";
    }
}
TestColor Calibration::expected() const { return static_cast<TestColor>(index_); }
bool Calibration::start(bool order, int64_t now) {
    if (active()) return false;
    phase_=CalibrationPhase::Identify; original_order_=candidate_order_=order;
    ++run_; if (!run_) ++run_;
    step_=index_=0; touched_us_=now; presented_=false;
    return true;
}
bool Calibration::matches(uint32_t run, uint32_t step) const { return active() && run==run_ && step==step_; }
void Calibration::presented(uint32_t run, uint32_t step) {
    if (matches(run,step)) presented_=true;
}
bool Calibration::observe(uint32_t run, uint32_t step, TestColor seen, int64_t now) {
    expire(now);
    if (!matches(run,step) || !presented_ || ready()) return false;
    touched_us_=now;
    if (phase_==CalibrationPhase::Identify) {
        observed_[index_]=seen;
        if (++index_==3) {
            if (observed_[0]==TestColor::Red && observed_[1]==TestColor::Green && observed_[2]==TestColor::Blue)
                candidate_order_=original_order_;
            else if (observed_[0]==TestColor::Green && observed_[1]==TestColor::Red && observed_[2]==TestColor::Blue)
                candidate_order_=!original_order_;
            else { phase_=CalibrationPhase::Inconclusive; return true; }
            phase_=CalibrationPhase::Verify; index_=0;
        }
    } else {
        if (seen!=expected()) { phase_=CalibrationPhase::Inconclusive; return true; }
        if (++index_==7) { phase_=CalibrationPhase::Ready; index_=6; }
    }
    ++step_; presented_=false; return true;
}
void Calibration::expire(int64_t now) { if (active() && now-touched_us_>=timeout_us) phase_=CalibrationPhase::TimedOut; }
int64_t Calibration::remaining_ms(int64_t now) const { return active() ? std::max<int64_t>(0,(timeout_us-(now-touched_us_))/1000) : 0; }
ColorFrame Calibration::frame(bool safe) const {
    Settings settings;
    settings.mode=Mode::Calibration; settings.brightness=brightness;
    settings.rgb_order=candidate_order_; settings.enabled=safe;
    settings.saturation=expected()==TestColor::White ? 0 : 1;
    switch (expected()) {
    case TestColor::Green: settings.starting_hue=120; break;
    case TestColor::Blue: settings.starting_hue=240; break;
    case TestColor::Yellow: settings.starting_hue=60; break;
    case TestColor::Cyan: settings.starting_hue=180; break;
    case TestColor::Magenta: settings.starting_hue=300; break;
    default: settings.starting_hue=0; break;
    }
    // Identification tests wiring only: unity gains and exactly 2% physical PWM.
    auto result=color_frame(settings,0,0,0,false);
    result.calibration_run=run_; result.calibration_step=step_;
    return result;
}
}
