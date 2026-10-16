#pragma once
#include "cJSON.h"
namespace oml {
void network_init();
bool entropy_ready();
bool entropy_ready_checked();
const char* admin_token();
cJSON* network_status();
bool network_configure(const cJSON*);
bool network_recover();
}
