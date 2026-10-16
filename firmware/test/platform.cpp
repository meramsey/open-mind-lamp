#include "platform.hpp"
#include "led_strip.h"
#include <cassert>
#include <cstring>
#include <map>
#include <string>

// Host doubles exercise production persistence code; this is not a flash/eFuse
// or RF simulator. A close/reopen discards uncommitted writes like NVS handles.
static std::map<std::string,std::string> committed,pending;
bool fail_refresh=false, fail_commit=false;
int nvs_open(const char* name,int,nvs_handle_t* handle) { assert(std::string(name)=="oml"); pending.clear(); *handle=1; return ESP_OK; }
int nvs_get_str(nvs_handle_t,const char* key,char* out,size_t* size) {
    const auto item=committed.find(key); if(item==committed.end()) return ESP_FAIL;
    const auto required=item->second.size()+1;
    if(*size<required) { *size=required; return ESP_FAIL; }
    std::memcpy(out,item->second.c_str(),required); *size=required; return ESP_OK;
}
int nvs_set_str(nvs_handle_t,const char* key,const char* value) { pending[key]=value; return ESP_OK; }
int nvs_commit(nvs_handle_t) {
    if(fail_commit) return ESP_FAIL;
    for(const auto& entry:pending) committed[entry.first]=entry.second;
    pending.clear(); return ESP_OK;
}
void nvs_close(nvs_handle_t) { pending.clear(); }

struct FakeStrip { oml::RGB pending[64]{}; };
oml::RGB transmitted_bytes[64]{};
static FakeStrip strip;
int led_strip_new_rmt_device(const led_strip_config_t* c,const led_strip_rmt_config_t* r,led_strip_handle_t* out) {
    assert(c->strip_gpio_num==14 && c->max_leds==64 && c->color_component_format==LED_STRIP_COLOR_COMPONENT_FMT_GRB);
    assert(r->flags.with_dma); *out=&strip; return ESP_OK;
}
int led_strip_clear(led_strip_handle_t) { for(auto& p:transmitted_bytes) p={0,0,0}; return ESP_OK; }
int led_strip_set_pixel(led_strip_handle_t s,unsigned i,unsigned r,unsigned g,unsigned b) {
    assert(i<64 && r<=32 && g<=32 && b<=32);
    // Model the configured GRB library serializer, not a second color transform.
    s->pending[i]={static_cast<uint8_t>(g),static_cast<uint8_t>(r),static_cast<uint8_t>(b)};
    return ESP_OK;
}
int led_strip_refresh(led_strip_handle_t s) {
    if(fail_refresh) return ESP_FAIL;
    for(unsigned i=0;i<64;++i) transmitted_bytes[i]=s->pending[i];
    return ESP_OK;
}
