#include "settings.hpp"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <cstring>
namespace oml {
void storage_init() {
    // Preserve NVS on errors; never erase user settings/credentials automatically.
    ESP_ERROR_CHECK(nvs_flash_init());
}
bool read_secret(const char* key, char* out, size_t size) {
    nvs_handle_t h;
    if (nvs_open("oml", NVS_READONLY, &h) != ESP_OK) return false;
    const auto result = nvs_get_str(h, key, out, &size);
    nvs_close(h); return result == ESP_OK;
}
bool write_secret(const char* key, const char* value) {
    nvs_handle_t h;
    if (nvs_open("oml", NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = nvs_set_str(h, key, value) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h); return ok;
}
cJSON* settings_json(const Settings& s) {
    cJSON* j = cJSON_CreateObject();
#define NUM(field) cJSON_AddNumberToObject(j, #field, s.field)
    NUM(brightness); NUM(saturation); NUM(sensitivity); NUM(smoothing_ms);
    NUM(starting_hue); NUM(sample_hz); NUM(accumulation_ms); NUM(frame_hz); NUM(pixel);
    NUM(gain_red); NUM(gain_green); NUM(gain_blue);
#undef NUM
    cJSON_AddNumberToObject(j, "mode", static_cast<int>(s.mode));
    cJSON_AddBoolToObject(j, "enabled", s.enabled);
    cJSON_AddBoolToObject(j, "rgb_order", s.rgb_order);
    cJSON_AddBoolToObject(j, "channel_order_confirmed", s.channel_order_confirmed);
    return j;
}
bool patch_settings(const cJSON* j, Settings& output) {
    if (!cJSON_IsObject(j)) return false;
    Settings s = output;
    const cJSON* item;
    cJSON_ArrayForEach(item, j) {
        if (!item->string) return false;
#define DOUBLE(field) if (!std::strcmp(item->string, #field)) { if (!cJSON_IsNumber(item)) return false; s.field=item->valuedouble; continue; }
#define INTEGER(field) if (!std::strcmp(item->string, #field)) { if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) || item->valuedouble<0 || item->valuedouble>60000 || std::floor(item->valuedouble)!=item->valuedouble) return false; s.field=static_cast<uint32_t>(item->valuedouble); continue; }
#define BOOL(field) if (!std::strcmp(item->string, #field)) { if (!cJSON_IsBool(item)) return false; s.field=cJSON_IsTrue(item); continue; }
        DOUBLE(brightness) DOUBLE(saturation) DOUBLE(sensitivity) DOUBLE(smoothing_ms) DOUBLE(starting_hue)
        DOUBLE(gain_red) DOUBLE(gain_green) DOUBLE(gain_blue)
        INTEGER(sample_hz) INTEGER(accumulation_ms) INTEGER(frame_hz) INTEGER(pixel)
        BOOL(enabled) BOOL(rgb_order) BOOL(channel_order_confirmed)
#undef DOUBLE
#undef INTEGER
#undef BOOL
        if (!std::strcmp(item->string, "mode") && cJSON_IsNumber(item) &&
            item->valuedouble >= 0 && item->valuedouble <= 6 && std::floor(item->valuedouble)==item->valuedouble) {
            s.mode=static_cast<Mode>(item->valueint); continue;
        }
        return false;
    }
    if (!valid(s)) return false;
    output=s; return true;
}
Settings load_settings() {
    Settings s;
    char buffer[1024];
    if (read_secret("settings_v1", buffer, sizeof(buffer))) {
        cJSON* j=cJSON_Parse(buffer);
        if (!patch_settings(j,s)) ESP_LOGW("storage", "Invalid saved settings; using validated defaults");
        cJSON_Delete(j);
    }
    return s;
}
bool save_settings(const Settings& s) {
    if (!valid(s)) return false;
    cJSON* j=settings_json(s);
    char* text=cJSON_PrintUnformatted(j);
    const bool ok=text && write_secret("settings_v1", text);
    cJSON_free(text); cJSON_Delete(j); return ok;
}
}
