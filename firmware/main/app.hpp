#pragma once
#include "lighting/core.hpp"
#include "lighting/calibration.hpp"
#include "experiments/session.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
namespace oml {
struct App {
    Settings settings;
    Walk walk;
    RenderedColor color;
    Calibration calibration;
    Health health;
    Session session;
    SemaphoreHandle_t mutex = nullptr;
    uint32_t last_word = 0;
    uint64_t missed_slots = 0;
    uint32_t revision = 0;
    bool led_ok = true;
};
class Lock {
    SemaphoreHandle_t mutex_;
public:
    explicit Lock(App& app): mutex_(app.mutex) { xSemaphoreTake(mutex_,portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex_); }
};
void server_start(App&);
}
