#pragma once
#include "core.hpp"
#include "led_strip.h"
namespace oml {
class Matrix {
    led_strip_handle_t strip_ = nullptr;
public:
    void init();
    bool show(const Settings&, double hue, double seconds, uint32_t activity, bool active);
};
}
