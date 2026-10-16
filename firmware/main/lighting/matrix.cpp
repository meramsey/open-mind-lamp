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
bool Matrix::show(const ColorFrame& frame) {
    const RGB rgb=driver_rgb(frame.commanded_rgb,frame.rgb_order);
    for (unsigned i=0; i<64; ++i) {
        const RGB p=(frame.lit_mask & (UINT64_C(1)<<i)) ? rgb : RGB{0,0,0};
        if (led_strip_set_pixel(strip_,i,p.r,p.g,p.b)!=ESP_OK) return false;
    }
    return led_strip_refresh(strip_) == ESP_OK;
}
}
