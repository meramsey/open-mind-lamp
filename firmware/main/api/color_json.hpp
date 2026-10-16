#pragma once
#include "lighting/calibration.hpp"
#include "cJSON.h"
namespace oml {
cJSON* color_json(const RenderedColor&, int64_t now_us);
cJSON* calibration_json(const Calibration&, const Settings&, int64_t now_us);
}
