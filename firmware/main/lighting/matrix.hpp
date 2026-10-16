#pragma once
#include "color.hpp"
#include "led_strip.h"
namespace oml {
class Matrix {
    led_strip_handle_t strip_ = nullptr;
public:
    void init();
    bool show(const ColorFrame&);
};
}
