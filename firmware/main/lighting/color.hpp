#pragma once
#include "core.hpp"
namespace oml {
// A bounded, allocation-free frame. All lit pixels share commanded_rgb; unlit
// pixels are black. logical_rgb is unscaled/pre-gamma chromatic RGB (0..255).
struct ColorFrame {
    RGB logical_rgb{0,0,0}, commanded_rgb{0,0,0};
    double logical_hue = 0, saturation = 0, brightness = 0, walk_hue = 0;
    uint64_t lit_mask = 0;
    bool rgb_order = false;
    Mode mode = Mode::Walk;
    uint32_t calibration_run = 0, calibration_step = 0;
};
ColorFrame color_frame(const Settings&, double smoothed_hue, double seconds,
                       uint32_t activity, bool experiment_active);
struct RenderedColor {
    ColorFrame frame;
    uint64_t frame_id = 0;
    int64_t rendered_at_us = 0;
    bool valid = false;
    WalkRender render_step;
    bool render_tracked = false;
    // Retain last successful command on failure, but explicitly mark it unknown.
    void transmitted(const ColorFrame& next, int64_t now_us, bool success,const WalkRender* step=nullptr) {
        valid=success;
        if (success) {
            frame=next; rendered_at_us=now_us; ++frame_id;
            render_tracked=step!=nullptr; if (step) render_step=*step;
        }
    }
};
}
