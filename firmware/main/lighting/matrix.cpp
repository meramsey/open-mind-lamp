#include "matrix.hpp"
#include "esp_check.h"
namespace oml {
void Matrix::init() {
    led_strip_config_t config = {};
    config.strip_gpio_num = 14;
    config.max_leds = 64;
    config.led_model = LED_MODEL_WS2812;
    config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    led_strip_rmt_config_t rmt = {};
    rmt.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt.resolution_hz = 10000000;
    rmt.mem_block_symbols = 64;
    rmt.flags.with_dma = true;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&config, &rmt, &strip_));
    ESP_ERROR_CHECK(led_strip_clear(strip_));
}
bool Matrix::show(const Settings& s, double hue, double t, uint32_t activity, bool active) {
    double brightness = s.enabled ? s.brightness : 0;
    if (s.mode == Mode::Breathing) brightness *= 0.55 + 0.45 * std::sin(t * 1.2566370614);
    if (s.mode == Mode::Ambient) hue = s.starting_hue;
    if (s.mode == Mode::Experiment) hue = active ? 120 : 220;
    if (s.mode == Mode::Calibration) hue = s.starting_hue;
    const RGB rgb = hsv(hue, s.saturation, brightness);
    for (unsigned i=0; i<64; ++i) {
        const bool dark = (s.mode == Mode::Pixel && i != s.pixel) ||
            (s.mode == Mode::Activity && !((activity >> (i % 32)) & 1));
        const RGB p = dark ? RGB{0,0,0} : rgb;
        if (led_strip_set_pixel(strip_, i, s.rgb_order ? p.g : p.r,
                                s.rgb_order ? p.r : p.g, p.b) != ESP_OK) return false;
    }
    return led_strip_refresh(strip_) == ESP_OK;
}
}
