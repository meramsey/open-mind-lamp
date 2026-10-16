idf_component_register(
    SRCS "main.cpp" "lighting/core.cpp" "lighting/matrix.cpp" "lighting/color.cpp" "lighting/calibration.cpp"
         "storage/settings.cpp" "experiments/session.cpp"
         "networking/network.cpp" "api/server.cpp" "api/color_json.cpp" "api/rng_json.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_event esp_netif esp_http_server nvs_flash
                  esp_timer esp_driver_gpio esp_system esp_hw_support
                  spi_flash spiffs bootloader_support esp_app_format
    EMBED_FILES "../../web/index.html" "../../web/src/app.js" "../../web/src/style.css")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
