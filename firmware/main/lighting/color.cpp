#include "color.hpp"
#include <cmath>
namespace oml {
ColorFrame color_frame(const Settings& s, double hue, double seconds,
                       uint32_t activity, bool active) {
    ColorFrame frame;
    frame.walk_hue=wrap(hue);
    frame.mode=s.mode; frame.rgb_order=s.rgb_order;
    frame.brightness=s.enabled ? s.brightness : 0;
    frame.saturation=s.saturation;
    if (s.mode==Mode::Breathing) frame.brightness*=0.55+0.45*std::sin(seconds*1.2566370614);
    if (s.mode==Mode::Ambient || s.mode==Mode::Calibration) hue=s.starting_hue;
    if (s.mode==Mode::Experiment) hue=active ? 120 : 220;
    frame.logical_hue=wrap(hue);
    const auto c=hsv_channels(hue,s.saturation);
    frame.logical_rgb={static_cast<uint8_t>(std::lround(c.r*255)),
                       static_cast<uint8_t>(std::lround(c.g*255)),
                       static_cast<uint8_t>(std::lround(c.b*255))};
    if (s.enabled) {
        frame.lit_mask=UINT64_MAX;
        if (s.mode==Mode::Pixel) frame.lit_mask=UINT64_C(1)<<s.pixel;
        if (s.mode==Mode::Activity) frame.lit_mask=(static_cast<uint64_t>(activity)<<32)|activity;
    }
    frame.commanded_rgb=hsv(hue,s.saturation,frame.brightness,s.gain_red,s.gain_green,s.gain_blue);
    const auto rgb=frame.commanded_rgb;
    if (!frame.lit_mask || !(rgb.r || rgb.g || rgb.b)) {
        frame.lit_mask=0; frame.commanded_rgb={0,0,0};
    }
    return frame;
}
}
