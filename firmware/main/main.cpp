#include "app.hpp"
#include "lighting/matrix.hpp"
#include "entropy/source.hpp"
#include "networking/network.hpp"
#include "storage/settings.hpp"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include <algorithm>
namespace oml {
static App app;
}
extern "C" void app_main() {
    using namespace oml;
    esp_chip_info_t chip; esp_chip_info(&chip);
    uint32_t size=0; ESP_ERROR_CHECK(esp_flash_get_size(nullptr,&size));
    if (chip.model!=CHIP_ESP32S3 || size!=4*1024*1024) {
        ESP_LOGE("boot","Unexpected chip/flash; refusing GPIO initialization"); return;
    }
    ESP_LOGI("boot","ESP32-S3 revision %d, flash %lu, reset reason %d; PCB revision requires visual confirmation",
             chip.revision,static_cast<unsigned long>(size),esp_reset_reason());
    storage_init(); app.settings=load_settings(); app.walk.reset(app.settings.starting_hue);
    app.mutex=xSemaphoreCreateMutex(); ESP_ERROR_CHECK(app.mutex?ESP_OK:ESP_ERR_NO_MEM);
    app.session.init(); network_init();
    Matrix matrix; matrix.init();
    gpio_config_t button={}; button.pin_bit_mask=1ULL<<0; button.mode=GPIO_MODE_INPUT; button.pull_up_en=GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&button));
    HardwareSource rng(entropy_ready);
    server_start(app);
    int64_t next_sample=esp_timer_get_time(), next_commit=next_sample+app.settings.accumulation_ms*1000LL;
    int64_t last_frame=next_sample, next_frame=next_sample, pressed=0;
    uint32_t revision=app.revision;
    while (true) {
        const int64_t now=esp_timer_get_time();
        {
            Lock lock(app);
            app.calibration.expire(now);
            if (revision!=app.revision) {
                revision=app.revision; next_sample=now; next_commit=now+app.settings.accumulation_ms*1000LL;
                app.walk.reset(app.settings.starting_hue);
            }
            if (!entropy_ready()) app.session.stop("entropy_unavailable");
            if (now>=next_sample) {
                const int64_t period=1000000/app.settings.sample_hz;
                const uint64_t late=now-next_sample;
                app.missed_slots+=late/period;
                next_sample+=static_cast<int64_t>(late/period+1)*period; // never fabricate catch-up words
                uint32_t word;
                if (!app.health.failed() && rng.read(word)) {
                    app.last_word=word;
                    const bool healthy=app.health.observe(word);
                    app.session.sample(word,now,late,healthy);
                    if (healthy) app.walk.sample(word);
                }
            }
            if (now>=next_commit) {
                app.walk.commit(app.settings.sensitivity);
                next_commit=now+app.settings.accumulation_ms*1000LL;
            }
            if (now>=next_frame) {
                app.walk.render((now-last_frame)/1000.0,app.settings.smoothing_ms);
                last_frame=now; next_frame=now+1000000/app.settings.frame_hz;
                Settings display=app.settings;
                const bool safe=!app.health.failed() && entropy_ready();
                if (!safe) { display.enabled=false; app.calibration.interrupt(); }
                const auto frame=app.calibration.active() ? app.calibration.frame(safe) :
                    color_frame(display,app.walk.hue(),now/1e6,app.last_word,app.session.active());
                app.led_ok=matrix.show(frame);
                app.color.transmitted(frame,esp_timer_get_time(),app.led_ok);
                if (app.led_ok) app.calibration.presented(frame.calibration_run,frame.calibration_step);
                else app.calibration.interrupt();
                if (!app.led_ok) { app.session.stop("led_failure"); ESP_LOGE("led","RMT refresh failed"); }
            }
        }
        if (!gpio_get_level(GPIO_NUM_0)) {
            if (!pressed) pressed=now;
            if (pressed>0 && now-pressed>=5000000) {
                Lock lock(app); app.session.stop("network_recovery"); app.calibration.interrupt(); network_recover(); pressed=-1;
                ESP_LOGI("recovery","Home Wi-Fi cleared; AP and data retained");
            }
        } else pressed=0;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
