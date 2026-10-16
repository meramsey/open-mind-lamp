#pragma once
#include <cstdint>
// Host test double only. Production includes ESP-IDF's real hardware RNG header.
uint32_t esp_random();
