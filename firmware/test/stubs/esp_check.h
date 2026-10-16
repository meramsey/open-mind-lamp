#pragma once
#include <cassert>
#include "nvs.h"
#define ESP_ERROR_CHECK(expr) do { const int result=(expr); assert(result==ESP_OK); } while(false)
