#include "main.h"

void app_main(void) {
  init_dht();
  dht_start_task();
  led_start_task();
  vTaskDelete(NULL); // Удаляем текущий поток (главный поток)
}

//…or create a new repository on the command line
//echo "# esp_idf_esp32_mqtt_android" >> README.md
//git init
//git add README.md
//git commit -m "first commit"
//git branch -M main
//git remote add origin https://github.com/timurtm72/esp_idf_esp32_mqtt_android.git
//git push -u origin main
//…or push an existing repository from the command line
//git remote add origin https://github.com/timurtm72/esp_idf_esp32_mqtt_android.git
//git branch -M main
//git push -u origin main