#include "app.hpp"
#include "color_json.hpp"
#include "networking/network.hpp"
#include "esp_http_server.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <cstring>
#include <cstdio>
namespace oml {
extern const unsigned char index_start[] asm("_binary_index_html_start");
extern const unsigned char index_end[] asm("_binary_index_html_end");
extern const unsigned char js_start[] asm("_binary_app_js_start");
extern const unsigned char js_end[] asm("_binary_app_js_end");
extern const unsigned char css_start[] asm("_binary_style_css_start");
extern const unsigned char css_end[] asm("_binary_style_css_end");
static App* app;
static bool step_request(const cJSON* j, uint32_t& run, uint32_t& step) {
    const auto* r=cJSON_GetObjectItemCaseSensitive(j,"run_id");
    const auto* s=cJSON_GetObjectItemCaseSensitive(j,"step");
    const auto integer=[](const cJSON* v) {
        return cJSON_IsNumber(v) && std::isfinite(v->valuedouble) && v->valuedouble>=0 &&
            v->valuedouble<=UINT32_MAX && std::floor(v->valuedouble)==v->valuedouble;
    };
    if (!integer(r) || !integer(s)) return false;
    run=static_cast<uint32_t>(r->valuedouble); step=static_cast<uint32_t>(s->valuedouble); return true;
}
static void headers(httpd_req_t* req) {
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    httpd_resp_set_hdr(req,"X-Frame-Options","DENY");
    httpd_resp_set_hdr(req,"Referrer-Policy","no-referrer");
    httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
}
static esp_err_t fail(httpd_req_t* r,const char* status,const char* error) {
    httpd_resp_set_status(r,status); httpd_resp_set_type(r,"application/json");
    return httpd_resp_send(r,error,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t json(httpd_req_t* r,cJSON* data) {
    if (!data) return fail(r,"500 Internal Server Error","{\"error\":\"allocation failure\"}");
    char* text=cJSON_PrintUnformatted(data); cJSON_Delete(data);
    if (!text) return fail(r,"500 Internal Server Error","{\"error\":\"allocation failure\"}");
    httpd_resp_set_type(r,"application/json"); auto result=httpd_resp_send(r,text,HTTPD_RESP_USE_STRLEN);
    cJSON_free(text); return result;
}
static bool authorized(httpd_req_t* r) {
    char auth[80]={};
    if (httpd_req_get_hdr_value_len(r,"Authorization")!=71 || httpd_req_get_hdr_value_str(r,"Authorization",auth,sizeof(auth))!=ESP_OK) return false;
    unsigned diff=0; const char* token=admin_token();
    for (unsigned i=0;i<7;++i) diff|=static_cast<unsigned char>(auth[i]^"Bearer "[i]);
    for (unsigned i=0;i<64;++i) diff|=static_cast<unsigned char>(auth[i+7]^token[i]);
    return diff==0;
}
static cJSON* body(httpd_req_t* r) {
    if (r->content_len==0 || r->content_len>2048) return nullptr;
    char buffer[2049]; size_t used=0;
    while(used<r->content_len) {
        const int n=httpd_req_recv(r,buffer+used,r->content_len-used);
        if(n<=0) return nullptr;
        used+=n;
    }
    buffer[used]=0;
    // Bound nesting BEFORE calling cJSON on the HTTP task's limited stack.
    int depth=0; bool quoted=false,escaped=false;
    for(size_t i=0;i<used;++i) {
        char c=buffer[i]; if(c==0) return nullptr;
        if(quoted) { if(escaped) escaped=false; else if(c=='\\') escaped=true; else if(c=='"') quoted=false; }
        else if(c=='"') quoted=true;
        else if(c=='{' || c=='[') { if(++depth>8) return nullptr; }
        else if(c=='}' || c==']') { if(--depth<0) return nullptr; }
    }
    cJSON* j=cJSON_ParseWithOpts(buffer,nullptr,true);
    if (!cJSON_IsObject(j)) { cJSON_Delete(j); return nullptr; }
    for(const cJSON* a=j->child;a;a=a->next) for(const cJSON* b=a->next;b;b=b->next)
        if(!std::strcmp(a->string,b->string)) { cJSON_Delete(j); return nullptr; }
    return j;
}
static esp_err_t get(httpd_req_t* r) {
    headers(r);
    if(!std::strcmp(r->uri,"/")) { httpd_resp_set_type(r,"text/html; charset=utf-8"); return httpd_resp_send(r,reinterpret_cast<const char*>(index_start),index_end-index_start); }
    if(!std::strcmp(r->uri,"/src/app.js")) { httpd_resp_set_type(r,"text/javascript; charset=utf-8"); return httpd_resp_send(r,reinterpret_cast<const char*>(js_start),js_end-js_start); }
    if(!std::strcmp(r->uri,"/src/style.css")) { httpd_resp_set_type(r,"text/css; charset=utf-8"); return httpd_resp_send(r,reinterpret_cast<const char*>(css_start),css_end-css_start); }
    if(!authorized(r)) return fail(r,"401 Unauthorized","{\"error\":\"USB onboarding token required\"}");
    if(!std::strcmp(r->uri,"/api/export")) {
        FILE* file;
        { Lock lock(*app);
          if(app->session.active() || !app->session.available()) return fail(r,"409 Conflict","{\"error\":\"Stop a recorded session before export\"}");
          file=std::fopen(Session::path,"r"); }
        if(!file) return fail(r,"500 Internal Server Error","{\"error\":\"Could not read session\"}");
        httpd_resp_set_type(r,"application/x-ndjson");
        httpd_resp_set_hdr(r,"Content-Disposition","attachment; filename=trial.ndjson");
        char buffer[1024]; size_t n; esp_err_t result=ESP_OK;
        while((n=std::fread(buffer,1,sizeof(buffer),file))>0) if((result=httpd_resp_send_chunk(r,buffer,n))!=ESP_OK) break;
        const bool bad=std::ferror(file); std::fclose(file);
        if(result!=ESP_OK || bad) return ESP_FAIL;
        return httpd_resp_send_chunk(r,nullptr,0);
    }
    Lock lock(*app);
    if(!std::strcmp(r->uri,"/api/settings")) return json(r,settings_json(app->settings));
    if(!std::strcmp(r->uri,"/api/status")) {
        cJSON* j=cJSON_CreateObject();
        cJSON_AddStringToObject(j,"version",esp_app_get_description()->version);
        cJSON_AddStringToObject(j,"source","esp32s3-hwrng-rf-no-sleep");
        cJSON_AddNumberToObject(j,"uptime_s",esp_timer_get_time()/1e6);
        cJSON_AddNumberToObject(j,"free_heap",esp_get_free_heap_size());
        cJSON_AddNumberToObject(j,"min_heap",esp_get_minimum_free_heap_size());
        cJSON_AddNumberToObject(j,"hue",app->walk.hue()); cJSON_AddNumberToObject(j,"target_hue",app->walk.target());
        const auto now=esp_timer_get_time();
        cJSON_AddItemToObject(j,"color",color_json(app->color,now));
        cJSON_AddItemToObject(j,"calibration",calibration_json(app->calibration,app->settings,now));
        cJSON_AddNumberToObject(j,"rng_words",static_cast<double>(app->health.words));
        cJSON_AddNumberToObject(j,"rng_ones",static_cast<double>(app->health.ones));
        cJSON_AddNumberToObject(j,"missed_slots",static_cast<double>(app->missed_slots));
        cJSON_AddBoolToObject(j,"rng_failed",app->health.failed()); cJSON_AddBoolToObject(j,"led_ok",app->led_ok);
        cJSON_AddItemToObject(j,"network",network_status()); cJSON_AddItemToObject(j,"session",app->session.status());
        return json(r,j);
    }
    return fail(r,"404 Not Found","{\"error\":\"Unknown route\"}");
}
static esp_err_t post(httpd_req_t* r) {
    headers(r);
    if(!authorized(r)) return fail(r,"401 Unauthorized","{\"error\":\"Invalid token\"}");
    cJSON* j=body(r);
    if(!j) { httpd_resp_set_hdr(r,"Connection","close"); return fail(r,"400 Bad Request","{\"error\":\"Expected a JSON object, maximum 2048 bytes, depth 8, unique keys\"}"); }
    bool ok=false; const char* error="Operation unavailable or invalid parameters";
    {
        Lock lock(*app);
        const auto now=esp_timer_get_time();
        app->calibration.expire(now);
        if (app->health.failed() || !entropy_ready() || !app->led_ok) app->calibration.interrupt();
        if(!std::strcmp(r->uri,"/api/settings")) {
            Settings next=app->settings;
            if(!app->session.active() && !app->calibration.active() &&
               !cJSON_HasObjectItem(j,"channel_order_confirmed") && patch_settings(j,next)) {
                if (next.rgb_order!=app->settings.rgb_order) next.channel_order_confirmed=false;
                if (save_settings(next)) { app->settings=next; ++app->revision; ok=true; }
            }
            error="Settings must be in range; stop trial/calibration first; confirmation is workflow-only; storage must be writable";
        } else if(!std::strcmp(r->uri,"/api/calibration/start")) {
            if (!app->session.active() && !app->health.failed() && entropy_ready() && app->led_ok)
                ok=app->calibration.start(app->settings.rgb_order,now);
            error="Stop the trial first; calibration requires healthy entropy and LED transmission";
        } else if(!std::strcmp(r->uri,"/api/calibration/observe")) {
            uint32_t run,step; TestColor seen;
            const auto* observation=cJSON_GetObjectItemCaseSensitive(j,"observed");
            if (step_request(j,run,step) && cJSON_IsString(observation) && parse_color(observation->valuestring,seen))
                ok=app->calibration.observe(run,step,seen,now);
            error="Refresh status: observation must identify the current successfully transmitted calibration step";
        } else if(!std::strcmp(r->uri,"/api/calibration/save")) {
            uint32_t run,step;
            if (step_request(j,run,step) && app->calibration.matches(run,step) &&
                app->calibration.ready() && app->calibration.presented()) {
                Settings next=app->settings;
                next.rgb_order=app->calibration.candidate_order(); next.channel_order_confirmed=true;
                if (save_settings(next)) { app->settings=next; app->calibration.saved(); ok=true; }
            }
            error="Confirm all seven colors first; refresh the step token; settings storage must be writable";
        } else if(!std::strcmp(r->uri,"/api/calibration/cancel")) {
            uint32_t run,step;
            if (step_request(j,run,step) && app->calibration.matches(run,step)) { app->calibration.cancel(); ok=true; }
            error="Refresh status before cancelling this calibration run";
        } else if(!std::strcmp(r->uri,"/api/session/start")) {
            if(!app->calibration.active() && !app->health.failed() && entropy_ready() && app->led_ok) {
                ok=app->session.start(j,app->settings,"esp32s3-hwrng-rf-no-sleep",app->settings.starting_hue);
                if(ok) ++app->revision;
            }
            error="Export and clear previous trial; provide condition, direction, protocol, samples and client_utc; RNG must be healthy";
        } else if(!std::strcmp(r->uri,"/api/session/stop")) { app->session.stop("user_stopped"); ok=true;
        } else if(!std::strcmp(r->uri,"/api/session/clear")) { ok=app->session.clear();
        } else if(!std::strcmp(r->uri,"/api/wifi")) { if(!app->session.active() && !app->calibration.active()) ok=network_configure(j);
        } else if(!std::strcmp(r->uri,"/api/wifi/recover")) { if(!app->session.active() && !app->calibration.active()) ok=network_recover(); }
    }
    cJSON_Delete(j);
    if(ok) return fail(r,"200 OK","{\"ok\":true}");
    cJSON* result=cJSON_CreateObject(); cJSON_AddStringToObject(result,"error",error);
    httpd_resp_set_status(r,"409 Conflict"); return json(r,result);
}
void server_start(App& state) {
    app=&state; httpd_handle_t server=nullptr; httpd_config_t config=HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn=httpd_uri_match_wildcard; config.max_uri_handlers=2;
    config.stack_size=8192; config.max_open_sockets=4; config.lru_purge_enable=true;
    config.recv_wait_timeout=5; config.send_wait_timeout=5;
    ESP_ERROR_CHECK(httpd_start(&server,&config));
    httpd_uri_t handler={}; handler.uri="/*"; handler.method=HTTP_GET; handler.handler=get;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&handler));
    handler.method=HTTP_POST; handler.handler=post; ESP_ERROR_CHECK(httpd_register_uri_handler(server,&handler));
}
}
