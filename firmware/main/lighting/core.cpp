#include "core.hpp"
#include <algorithm>

namespace oml {
bool valid(const Settings& s) {
    return std::isfinite(s.brightness) && s.brightness >= 0 && s.brightness <= 0.125 &&
        std::isfinite(s.saturation) && s.saturation >= 0 && s.saturation <= 1 &&
        std::isfinite(s.sensitivity) && s.sensitivity >= 0 && s.sensitivity <= 5 &&
        std::isfinite(s.smoothing_ms) && s.smoothing_ms >= 0 && s.smoothing_ms <= 30000 &&
        std::isfinite(s.starting_hue) && s.starting_hue >= 0 && s.starting_hue < 360 &&
        s.sample_hz >= 1 && s.sample_hz <= 100 && s.frame_hz >= 5 && s.frame_hz <= 60 &&
        s.accumulation_ms >= 10 && s.accumulation_ms <= 60000 && s.pixel < 64 &&
        static_cast<unsigned>(s.mode) <= static_cast<unsigned>(Mode::Experiment);
}
double wrap(double h) { h = std::fmod(h, 360.0); return h < 0 ? h + 360 : h; }
int net_bits(uint32_t w) { return 2 * __builtin_popcount(w) - 32; }
RGB hsv(double h, double s, double v) {
    h = wrap(h) / 60;
    const double c = s, x = c * (1 - std::abs(std::fmod(h, 2.0) - 1)), m = 1 - c;
    double r = 0, g = 0, b = 0;
    switch (static_cast<int>(h)) {
    case 0: r=c; g=x; break; case 1: r=x; g=c; break;
    case 2: g=c; b=x; break; case 3: g=x; b=c; break;
    case 4: r=x; b=c; break; default: r=c; b=x; break;
    }
    // Gamma on chromatic components; v remains an explicit physical power scale.
    const auto channel = [v,m](double a) { return static_cast<uint8_t>(std::lround(
        std::clamp(std::pow(a+m, 2.2) * std::clamp(v, 0.0, 0.125) * 255, 0.0, 32.0))); };
    return {channel(r), channel(g), channel(b)};
}
void Walk::render(double dt, double tau) {
    const double delta = wrap(target_ - rendered_ + 180) - 180;
    rendered_ = wrap(rendered_ + delta * (tau <= 0 ? 1 : -std::expm1(-dt/tau)));
}
bool Health::observe(uint32_t word) {
    ++words;
    ones += __builtin_popcount(word);
    for (unsigned i=0; i<32; ++i) {
        const bool bit = (word >> i) & 1;
        run_ = (run_ && bit == last_) ? run_ + 1 : 1;
        last_ = bit;
        if (run_ >= 64) failed_ = true;
        window_ones_ += bit;
        if (++window_bits_ == 1024) {
            if (window_ones_ < 384 || window_ones_ > 640) failed_ = true;
            window_bits_ = window_ones_ = 0;
        }
    }
    return !failed_;
}
}
