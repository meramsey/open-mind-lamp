#include "network.hpp"
#include "storage/settings.hpp"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "bootloader_random.h"
#include "mdns.h"
#include "sdkconfig.h"
#include <atomic>
#include <cstring>
#include <cstdio>
namespace oml {
#if defined(CONFIG_PM_ENABLE)
#error "Open Mind Lamp RF entropy profile requires power management disabled"
#endif
static std::atomic<bool> ap_running{false}, connected{false}, station_enabled{false};
static esp_timer_handle_t retry_timer;
static char token[65], ap_password[25], ssid[33], hostname[33];
static esp_netif_t* sta_netif;
static std::atomic<bool> no_sleep_configured{false}, profile_ready{false};
static void events(void*,esp_event_base_t base,int32_t id,void*) {
    if (base==WIFI_EVENT) {
        if (id==WIFI_EVENT_AP_START) ap_running=true;
        if (id==WIFI_EVENT_AP_STOP) ap_running=false;
        if (id==WIFI_EVENT_STA_DISCONNECTED) {
            connected=false;
            if (station_enabled && !esp_timer_is_active(retry_timer)) esp_timer_start_once(retry_timer,5000000);
        }
    }
    if (base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        connected=true;
        esp_timer_stop(retry_timer);
        esp_netif_ip_info_t info; esp_netif_get_ip_info(sta_netif,&info);
        ESP_LOGI("network","LAN address: " IPSTR,IP2STR(&info.ip));
    }
}
static void secret(const char* key,char* output,size_t bytes) {
    if (read_secret(key,output,bytes*2+1)) return;
    uint8_t random[32]; esp_fill_random(random,bytes);
    for (size_t i=0;i<bytes;++i) std::snprintf(output+2*i,3,"%02x",random[i]);
    ESP_ERROR_CHECK(write_secret(key,output)?ESP_OK:ESP_FAIL);
}
bool entropy_ready() { return ap_running.load() && no_sleep_configured.load() && profile_ready.load(); }
bool entropy_ready_checked() {
    wifi_mode_t mode=WIFI_MODE_NULL; wifi_ps_type_t ps=WIFI_PS_MIN_MODEM;
    const bool ready=ap_running.load() && no_sleep_configured.load() &&
        esp_wifi_get_mode(&mode)==ESP_OK && (mode==WIFI_MODE_AP || mode==WIFI_MODE_APSTA) &&
        esp_wifi_get_ps(&ps)==ESP_OK && ps==WIFI_PS_NONE;
    profile_ready=ready; return ready;
}
const char* admin_token() { return token; }
void network_init() {
    // Early first-boot secrets get physical entropy before RF is started.
    bootloader_random_enable(); secret("admin",token,32); secret("ap_password",ap_password,12); bootloader_random_disable();
    uint8_t mac[6]; ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP));
    std::snprintf(ssid,sizeof(ssid),"OpenMindLamp-%02X%02X%02X",mac[3],mac[4],mac[5]);
    std::snprintf(hostname,sizeof(hostname),"openmindlamp-%02x%02x%02x",mac[3],mac[4],mac[5]);
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap(); sta_netif=esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif,hostname));
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&init));
    esp_timer_create_args_t retry={};
    retry.callback=[](void*) { if(station_enabled && !connected) esp_wifi_connect(); };
    retry.name="wifi-retry"; ESP_ERROR_CHECK(esp_timer_create(&retry,&retry_timer));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,events,nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,events,nullptr));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    wifi_config_t ap={}; std::strcpy(reinterpret_cast<char*>(ap.ap.ssid),ssid);
    std::strcpy(reinterpret_cast<char*>(ap.ap.password),ap_password);
    ap.ap.ssid_len=std::strlen(ssid); ap.ap.channel=1; ap.ap.max_connection=4;
    ap.ap.authmode=WIFI_AUTH_WPA2_PSK; ap.ap.pmf_cfg.capable=true;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));
    ESP_ERROR_CHECK(esp_wifi_start()); ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    no_sleep_configured=true;
    char saved[256];
    if (read_secret("wifi_v1",saved,sizeof(saved))) {
        cJSON* j=cJSON_Parse(saved); network_configure(j); cJSON_Delete(j);
    }
    ESP_ERROR_CHECK(mdns_init()); ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set("Open Mind Lamp"));
    ESP_ERROR_CHECK(mdns_service_add(nullptr,"_http","_tcp",80,nullptr,0));
    // Physical USB access is the onboarding trust channel. Never send these in API responses.
    ESP_LOGI("onboarding","SSID: %s | WPA2 password: %s",ssid,ap_password);
    ESP_LOGI("onboarding","Admin token: %s",token);
    ESP_LOGI("onboarding","Open http://192.168.4.1 or http://%s.local",hostname);
}
bool network_configure(const cJSON* j) {
    const auto* name=cJSON_GetObjectItemCaseSensitive(j,"ssid");
    const auto* pass=cJSON_GetObjectItemCaseSensitive(j,"password");
    if (!cJSON_IsString(name) || !cJSON_IsString(pass) || std::strlen(name->valuestring)<1 ||
        std::strlen(name->valuestring)>32 || std::strlen(pass->valuestring)<8 || std::strlen(pass->valuestring)>63) return false;
    wifi_config_t config={};
    std::memcpy(config.sta.ssid,name->valuestring,std::strlen(name->valuestring));
    std::strcpy(reinterpret_cast<char*>(config.sta.password),pass->valuestring);
    config.sta.threshold.authmode=WIFI_AUTH_WPA2_PSK; config.sta.pmf_cfg.capable=true;
    cJSON* saved=cJSON_CreateObject(); cJSON_AddStringToObject(saved,"ssid",name->valuestring); cJSON_AddStringToObject(saved,"password",pass->valuestring);
    char* text=cJSON_PrintUnformatted(saved);
    bool ok=text && write_secret("wifi_v1",text);
    cJSON_free(text); cJSON_Delete(saved);
    if (!ok) return false;
    station_enabled=false; esp_timer_stop(retry_timer); esp_wifi_disconnect();
    if (esp_wifi_set_config(WIFI_IF_STA,&config)!=ESP_OK) return false;
    station_enabled=true;
    return esp_wifi_connect()==ESP_OK;
}
bool network_recover() {
    if (!write_secret("wifi_v1","{}")) return false;
    station_enabled=false; connected=false; esp_timer_stop(retry_timer);
    wifi_config_t blank={}; esp_wifi_disconnect();
    return esp_wifi_set_config(WIFI_IF_STA,&blank)==ESP_OK;
}
cJSON* network_status() {
    cJSON* j=cJSON_CreateObject(); char ip[16]=""; esp_netif_ip_info_t info={};
    if (connected && esp_netif_get_ip_info(sta_netif,&info)==ESP_OK) std::snprintf(ip,sizeof(ip),IPSTR,IP2STR(&info.ip));
    cJSON_AddStringToObject(j,"ap_ssid",ssid); cJSON_AddStringToObject(j,"ap_ip","192.168.4.1");
    cJSON_AddStringToObject(j,"hostname",hostname); cJSON_AddStringToObject(j,"station_ip",ip);
    cJSON_AddBoolToObject(j,"connected",connected.load()); cJSON_AddBoolToObject(j,"rf_entropy_ready",entropy_ready_checked());
    cJSON_AddBoolToObject(j,"ap_started",ap_running.load());
    wifi_mode_t mode=WIFI_MODE_NULL; wifi_ps_type_t ps=WIFI_PS_MIN_MODEM;
    const bool mode_ok=esp_wifi_get_mode(&mode)==ESP_OK, ps_ok=esp_wifi_get_ps(&ps)==ESP_OK;
    cJSON_AddStringToObject(j,"wifi_mode",mode_ok && mode==WIFI_MODE_APSTA ? "APSTA" : mode_ok && mode==WIFI_MODE_AP ? "AP" : "other_or_unavailable");
    cJSON_AddBoolToObject(j,"power_save_none",ps_ok && ps==WIFI_PS_NONE);
    return j;
}
}
