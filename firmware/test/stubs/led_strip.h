#pragma once
#include "nvs.h"
constexpr int LED_MODEL_WS2812=0, LED_STRIP_COLOR_COMPONENT_FMT_GRB=0, RMT_CLK_SRC_DEFAULT=0;
struct led_strip_config_t { int strip_gpio_num=0, max_leds=0, led_model=0, color_component_format=0; };
struct led_strip_rmt_config_t { int clk_src=0, resolution_hz=0, mem_block_symbols=0; struct { bool with_dma=false; } flags; };
struct FakeStrip;
using led_strip_handle_t=FakeStrip*;
int led_strip_new_rmt_device(const led_strip_config_t*,const led_strip_rmt_config_t*,led_strip_handle_t*);
int led_strip_clear(led_strip_handle_t);
int led_strip_set_pixel(led_strip_handle_t,unsigned,unsigned,unsigned,unsigned);
int led_strip_refresh(led_strip_handle_t);
