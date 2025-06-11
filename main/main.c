#include "main.h"


void app_main(void) {
  // Инициализация NVS (энергонезависимая память)
  ESP_ERROR_CHECK(nvs_flash_init());

  // Инициализация Wi-Fi
  wifi_init();

  // Инициализация DHT
  //init_dht();

  // Запуск задач
  //start_dht_task();
  //start_led_task();
  //start_process_task();
  vTaskDelete(NULL); // Удаляем текущий поток (главный поток)
}
