#pragma once
#include "lighting/core.hpp"
#include "cJSON.h"
namespace oml {
void storage_init();
Settings load_settings();
bool save_settings(const Settings&);
cJSON* settings_json(const Settings&);
bool patch_settings(const cJSON*, Settings&);
bool read_secret(const char* key, char* out, size_t size);
bool write_secret(const char* key, const char* value);
}
