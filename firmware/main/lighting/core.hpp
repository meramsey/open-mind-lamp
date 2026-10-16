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
class Walk {
    double target_ = 180, rendered_ = 180;
    int32_t accumulated_ = 0;
public:
    void reset(double hue) { target_ = rendered_ = wrap(hue); accumulated_ = 0; }
    void sample(uint32_t word) { accumulated_ += net_bits(word); }
    void commit(double sensitivity) { target_ = wrap(target_ + sensitivity * accumulated_); accumulated_ = 0; }
    void render(double dt_ms, double smoothing_ms);
    double hue() const { return rendered_; }
    double target() const { return target_; }
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
