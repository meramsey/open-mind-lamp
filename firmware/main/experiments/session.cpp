#include "session.hpp"
#include "esp_spiffs.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include <cinttypes>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
namespace oml {
void Session::init() {
    esp_vfs_spiffs_conf_t config = {};
    config.base_path="/sessions"; config.partition_label="sessions"; config.max_files=3;
    // Format only an entirely erased partition. Corruption must not erase trials.
    const auto* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "sessions");
    if (!p) return;
    bool erased=true;
    uint8_t block[256];
    for (size_t offset=0; offset<p->size && erased; offset+=sizeof(block)) {
        if (esp_partition_read(p,offset,block,sizeof(block))!=ESP_OK) return;
        for (uint8_t byte:block) if (byte!=0xff) { erased=false; break; }
    }
    config.format_if_mount_failed=erased;
    ready_=esp_vfs_spiffs_register(&config)==ESP_OK;
    if (!ready_) ESP_LOGE("session", "Session filesystem unavailable; preserving data");
    if (available()) std::strcpy(reason_,"retained_or_interrupted");
}
bool Session::available() const { struct stat s; return ready_ && stat(path,&s)==0; }
bool Session::sync() { return file_ && std::fflush(file_)==0 && fsync(fileno(file_))==0; }
bool Session::start(const cJSON* r, const Settings& settings, const char* source, double hue) {
    if (!ready_ || active() || available() || !cJSON_IsObject(r)) return false;
    const auto* n=cJSON_GetObjectItemCaseSensitive(r,"samples");
    const auto* label=cJSON_GetObjectItemCaseSensitive(r,"condition");
    const auto* direction=cJSON_GetObjectItemCaseSensitive(r,"direction");
    const auto* protocol=cJSON_GetObjectItemCaseSensitive(r,"protocol");
    const auto* utc=cJSON_GetObjectItemCaseSensitive(r,"client_utc");
    if (!cJSON_IsNumber(n) || n->valuedouble<32 || n->valuedouble>4096 || std::floor(n->valuedouble)!=n->valuedouble ||
        !cJSON_IsString(label) || (std::strcmp(label->valuestring,"control") && std::strcmp(label->valuestring,"intention") && std::strcmp(label->valuestring,"baseline")) ||
        !cJSON_IsString(direction) || (std::strcmp(direction->valuestring,"high") && std::strcmp(direction->valuestring,"low")) ||
        !cJSON_IsString(protocol) || !protocol->valuestring[0] || std::strlen(protocol->valuestring)>160 ||
        !cJSON_IsString(utc) || std::strlen(utc->valuestring)>32) return false;
    count_=0; ones_=0; io_failed_=false; planned_=n->valueint; start_=esp_timer_get_time();
    std::snprintf(id_,sizeof(id_),"%08" PRIx32 "%08" PRIx32,esp_random(),esp_random());
    cJSON* header=cJSON_CreateObject();
    cJSON_AddStringToObject(header,"type","header");
    cJSON_AddNumberToObject(header,"schema",1);
    cJSON_AddStringToObject(header,"id",id_);
    cJSON_AddStringToObject(header,"firmware",esp_app_get_description()->version);
    cJSON_AddStringToObject(header,"idf",esp_app_get_description()->idf_ver);
    cJSON_AddStringToObject(header,"source",source);
    cJSON_AddStringToObject(header,"entropy_config","Wi-Fi AP always active; WIFI_PS_NONE; no light sleep; unconditioned 32-bit esp_random words");
    cJSON_AddStringToObject(header,"algorithm","net-bits-v1-lsb-first");
    cJSON_AddStringToObject(header,"condition",label->valuestring);
    cJSON_AddStringToObject(header,"direction",direction->valuestring);
    cJSON_AddStringToObject(header,"protocol",protocol->valuestring);
    cJSON_AddStringToObject(header,"client_utc",utc->valuestring);
    cJSON_AddStringToObject(header,"clock","monotonic microseconds; client UTC is unverified");
    cJSON_AddNumberToObject(header,"planned_samples",planned_);
    cJSON_AddNumberToObject(header,"start_us",static_cast<double>(start_));
    cJSON_AddNumberToObject(header,"initial_hue",hue);
    cJSON_AddItemToObject(header,"settings",settings_json(settings));
    char* text=cJSON_PrintUnformatted(header);
    file_=std::fopen(path,"wx");
    const bool ok=text && file_ && std::fprintf(file_,"%s\n",text)>0 && sync();
    cJSON_free(text); cJSON_Delete(header);
    if (!ok) { if(file_) std::fclose(file_); file_=nullptr; std::strcpy(reason_,"storage_error"); return false; }
    std::strcpy(reason_,"running"); return true;
}
void Session::sample(uint32_t word, int64_t now, uint64_t late, bool healthy) {
    if (!file_) return;
    const int result=std::fprintf(file_,"{\"type\":\"sample\",\"i\":%" PRIu32 ",\"t_us\":%" PRId64 ",\"word\":%" PRIu32 ",\"late_us\":%" PRIu64 ",\"healthy\":%s}\n",
        count_,now-start_,word,late,healthy?"true":"false");
    if (result<0) { io_failed_=true; stop("storage_error"); return; }
    ++count_; ones_+=__builtin_popcount(word);
    if (count_%32==0 && !sync()) { io_failed_=true; stop("storage_error"); return; }
    if (!healthy) stop("health_failure");
    else if (count_>=planned_) stop("completed");
}
void Session::stop(const char* reason) {
    if (!file_) return;
    std::snprintf(reason_,sizeof(reason_),"%s",reason);
    if (std::fprintf(file_,"{\"type\":\"footer\",\"count\":%" PRIu32 ",\"ones\":%" PRIu64 ",\"reason\":\"%s\"}\n",count_,ones_,reason_)<0 || !sync()) io_failed_=true;
    if (std::fclose(file_)!=0) io_failed_=true;
    file_=nullptr;
    if (io_failed_) std::strcpy(reason_,"storage_error");
}
bool Session::clear() { return ready_ && !active() && (!available() || std::remove(path)==0); }
cJSON* Session::status() const {
    cJSON* j=cJSON_CreateObject();
    cJSON_AddBoolToObject(j,"active",active()); cJSON_AddBoolToObject(j,"available",available());
    cJSON_AddBoolToObject(j,"storage_ready",ready_); cJSON_AddBoolToObject(j,"io_failed",io_failed_);
    cJSON_AddStringToObject(j,"id",id_); cJSON_AddStringToObject(j,"reason",reason_);
    cJSON_AddNumberToObject(j,"count",count_); cJSON_AddNumberToObject(j,"planned",planned_);
    return j;
}
}
