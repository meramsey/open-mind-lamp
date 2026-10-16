#pragma once
#include <cmath>
#include <cstdint>

namespace oml {
enum class Mode : uint8_t { Ambient, Walk, Breathing, Pixel, Calibration, Activity, Experiment };
struct Settings {
    double brightness = 0.08; // physical PWM ceiling is separately fixed at 32/255
    double saturation = 0.9;
    double sensitivity = 0.2; // degrees per net bit
    double smoothing_ms = 1500;
    double starting_hue = 180;
    uint32_t sample_hz = 20;
    uint32_t accumulation_ms = 1000;
    uint32_t frame_hz = 30;
    uint32_t pixel = 0;
    Mode mode = Mode::Walk;
    bool enabled = true;
    bool rgb_order = false; // GRB per manufacturer's Font demo; verify calibration
    double gain_red = 1;
    double gain_green = 1;
    double gain_blue = 1;
    bool channel_order_confirmed = false; // set only by a completed observation workflow
};
bool valid(const Settings& s);
double wrap(double hue);
int net_bits(uint32_t word);
struct RGB { uint8_t r, g, b; };
struct Channels { double r, g, b; };
Channels hsv_channels(double hue, double saturation);
RGB hsv(double hue, double saturation, double brightness,
        double gain_red = 1, double gain_green = 1, double gain_blue = 1);
// Driver is configured GRB. Adapt library arguments once for an RGB physical chain.
RGB driver_rgb(RGB commanded, bool rgb_order);
RGB wire_bytes(RGB commanded, bool rgb_order);
struct WalkCommit {
    uint64_t window_id = 0;
    uint32_t samples = 0;
    int32_t net = 0;
    double before = 180, after = 180, movement_deg = 0;
};
struct WalkRender {
    double before=0, target=0, dt_ms=0, smoothing_ms=0, after=0;
};
class Walk {
    double target_ = 180, rendered_ = 180;
    int32_t accumulated_ = 0;
    uint32_t pending_samples_ = 0;
    uint64_t window_id_ = 0;
    WalkCommit last_commit_;
public:
    void reset(double hue) {
        target_ = rendered_ = wrap(hue); accumulated_ = 0; pending_samples_=0; window_id_=0;
        last_commit_={0,0,0,target_,target_,0};
    }
    void sample(uint32_t word) { accumulated_ += net_bits(word); ++pending_samples_; }
    WalkCommit commit(double sensitivity) {
        last_commit_={window_id_,pending_samples_,accumulated_,target_,0,sensitivity*accumulated_};
        target_=wrap(target_+last_commit_.movement_deg); last_commit_.after=target_;
        accumulated_=0; pending_samples_=0; ++window_id_; return last_commit_;
    }
    WalkRender render(double dt_ms, double smoothing_ms);
    double hue() const { return rendered_; }
    double target() const { return target_; }
    int32_t pending_net() const { return accumulated_; }
    uint32_t pending_samples() const { return pending_samples_; }
    uint64_t window_id() const { return window_id_; }
    const WalkCommit& last_commit() const { return last_commit_; }
};
// Latching gross-failure indicators, not certification or an entropy estimator.
class Health {
    uint32_t run_ = 0, window_bits_ = 0, window_ones_ = 0;
    bool last_ = false, failed_ = false;
public:
    uint64_t words = 0, ones = 0;
    bool observe(uint32_t word);
    bool failed() const { return failed_; }
};
}
