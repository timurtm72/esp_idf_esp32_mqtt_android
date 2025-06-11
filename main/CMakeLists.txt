# See the build system documentation in IDF programming guide
idf_component_register(
    SRCS "main.c" "dht.c" "led_strip_encoder.c" "util.c" "process.c" "mqtt.c" "wifi.c" "led.c"
    INCLUDE_DIRS "."
    REQUIRES freertos driver esp_driver_rmt esp_timer esp_wifi mqtt esp_event nvs_flash json
)