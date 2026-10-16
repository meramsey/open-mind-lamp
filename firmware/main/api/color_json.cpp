#include "color_json.hpp"
#include <algorithm>
#include <cinttypes>
#include <cstdio>
namespace oml {
static void rgb(cJSON* object, const char* name, RGB color) {
    const int values[]={color.r,color.g,color.b};
    cJSON_AddItemToObject(object,name,cJSON_CreateIntArray(values,3));
}
cJSON* color_json(const RenderedColor& state, int64_t now) {
    cJSON* j=cJSON_CreateObject();
    const auto& f=state.frame;
    cJSON_AddBoolToObject(j,"valid",state.valid);
    cJSON_AddNumberToObject(j,"frame_id",static_cast<double>(state.frame_id));
    cJSON_AddNumberToObject(j,"rendered_at_us",static_cast<double>(state.rendered_at_us));
    cJSON_AddNumberToObject(j,"age_ms",std::max<int64_t>(0,now-state.rendered_at_us)/1000.0);
    cJSON_AddStringToObject(j,"meaning","last successfully transmitted command; not optically measured");
    cJSON_AddNumberToObject(j,"logical_hue",f.logical_hue);
    cJSON_AddNumberToObject(j,"saturation",f.saturation); cJSON_AddNumberToObject(j,"brightness",f.brightness);
    cJSON_AddNumberToObject(j,"mode",static_cast<unsigned>(f.mode));
    rgb(j,"logical_rgb",f.logical_rgb); rgb(j,"commanded_rgb",f.commanded_rgb);
    rgb(j,"driver_rgb",driver_rgb(f.commanded_rgb,f.rgb_order));
    rgb(j,"wire_bytes",wire_bytes(f.commanded_rgb,f.rgb_order));
    cJSON_AddStringToObject(j,"channel_order",f.rgb_order ? "RGB" : "GRB");
    cJSON_AddNumberToObject(j,"lit_pixels",__builtin_popcountll(f.lit_mask));
    char mask[17]; std::snprintf(mask,sizeof(mask),"%016" PRIx64,f.lit_mask);
    cJSON_AddStringToObject(j,"lit_mask",mask);
    cJSON_AddNumberToObject(j,"calibration_run",f.calibration_run);
    cJSON_AddNumberToObject(j,"calibration_step",f.calibration_step);
    return j;
}
cJSON* calibration_json(const Calibration& c, const Settings& settings, int64_t now) {
    cJSON* j=cJSON_CreateObject();
    cJSON_AddBoolToObject(j,"active",c.active()); cJSON_AddBoolToObject(j,"ready",c.ready());
    cJSON_AddBoolToObject(j,"presented",c.presented());
    cJSON_AddStringToObject(j,"phase",c.phase_name());
    cJSON_AddNumberToObject(j,"run_id",c.run()); cJSON_AddNumberToObject(j,"step",c.step());
    cJSON_AddNumberToObject(j,"index",c.index());
    cJSON_AddStringToObject(j,"expected",c.active() ? color_name(c.expected()) : "none");
    cJSON_AddStringToObject(j,"candidate_order",c.candidate_order() ? "RGB" : "GRB");
    cJSON_AddStringToObject(j,"saved_order",settings.rgb_order ? "RGB" : "GRB");
    cJSON_AddBoolToObject(j,"saved_order_confirmed",settings.channel_order_confirmed);
    cJSON_AddNumberToObject(j,"remaining_ms",c.remaining_ms(now));
    cJSON_AddNumberToObject(j,"brightness",Calibration::brightness);
    return j;
}
}
