#include "main.h"

// Определение логового тега
static const char *TAG = "process_task";

QueueHandle_t qProcessQueue;
TaskHandle_t thProcessHandle = NULL;

void start_process_task() {
  xTaskCreatePinnedToCore(process_task, // Функция задачи
                          "process_task", // Имя задачи для отладки
                          4096, // Размер стека в словах
                          NULL, // Параметры задачи
                          0,    // Приоритет задачи
                          &thProcessHandle, // Указатель на хендл задачи
                          1                 // Номер ядра (1)
  );
}

void process_task(void *pvParameter) {
  dht_reading_t dht_reading = {.humidity = 0, .temperature = 0, .valid = false};
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(20);
  uint32_t print_counter = 0;
  uint32_t mqtt_pub_counter = 0;

#define PRINT_INTERVAL 150
#define MQTT_PUB_INTERVAL 250 // Примерно 5 секунд

  while (1) {
    // Получаем данные с DHT датчика
    if (uxQueueMessagesWaiting(qDhtQueue) > 0) {
      xQueueReceive(qDhtQueue, &dht_reading, portMAX_DELAY);
    }

    // Публикация данных DHT на MQTT сервер каждые ~5 секунд
    mqtt_pub_counter++;
    if (mqtt_pub_counter >= MQTT_PUB_INTERVAL) {
      mqtt_pub_counter = 0;
      // Отправляем только данные с датчика DHT
      mqtt_publish_dht_data(&dht_reading);
    }

    // Вывод логов каждые 3 секунды
    if ((print_counter++) >= PRINT_INTERVAL) {
      print_counter = 0;
      if (dht_reading.valid) {
        ESP_LOGI(TAG, "Humidity: %.1f%% Temperature: %.1fC",
                 dht_reading.humidity, dht_reading.temperature);
      } else {
        ESP_LOGW(TAG, "No valid DHT readings available");
      }
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}
